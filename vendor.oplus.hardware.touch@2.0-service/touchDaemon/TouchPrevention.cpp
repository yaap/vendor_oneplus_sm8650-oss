/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Userspace port of the OPLUS touch daemon prevention layer.  The algorithm
 * body in TouchPreventionCore.h is the same OPLUS V2/V4 implementation used
 * by the stock daemon and the in-tree kernel; this file supplies the
 * userspace configuration, lifetime, and make-up point plumbing.
 */

#include "TouchPrevention.h"

#include <tinyxml2.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "TouchPreventionPort.h"

namespace oplus_prevention {

std::map<std::string, std::vector<uint32_t>> *g_config = nullptr;
MakeupState g_makeup_state = {};

static void start_makeup_timer(struct kernel_grip_info *grip_info, uint8_t index)
{
    if (grip_info == nullptr || index >= TOUCH_MAX_NUM) {
        return;
    }

    grip_info->grip_hold_status[index] = 1;
    g_makeup_state.pending[index] = true;
    g_makeup_state.deadline_ms[index] =
            ktime_to_ms(ktime_get()) + grip_info->report_updelay_ms;
}

#include "TouchPreventionCore.h"

void kernel_grip_release(struct kernel_grip_info *grip_info)
{
    if (grip_info == nullptr) {
        return;
    }

    kfree(grip_info->coord_buf);
    grip_info->coord_buf = nullptr;
    kfree(grip_info);
}

static uint8_t get_bit_count(int var)
{
    uint8_t bits = 0;
    for (int bit = 0; bit < static_cast<int>(8 * sizeof(var)); ++bit) {
        if ((var >> bit) & 0x01) {
            ++bits;
        }
    }
    return bits;
}

int notify_prevention_handle(struct kernel_grip_info *grip_info, int obj_attention,
                             struct point_info *points)
{
    int index = 0;

    if (grip_info == nullptr || grip_info->p_ts == nullptr || points == nullptr) {
        return obj_attention;
    }

    mutex_lock(&grip_info->grip_mutex);

    grip_info->obj_bit_rcd = obj_attention;
    if (!(grip_info->grip_disable_level & (1 << GRIP_DISABLE_LARGE))) {
        if (grip_info->is_curved_screen) {
            obj_attention = curved_large_handle(grip_info, obj_attention, points);
        } else if (grip_info->is_curved_screen_V2) {
            obj_attention = curved_large_handle_V2(grip_info, obj_attention, points);
        } else {
            obj_attention = large_condition_handle(grip_info, obj_attention, points);
        }
    }
    if (!(grip_info->grip_disable_level & (1 << GRIP_DISABLE_ELI))) {
        obj_attention = touch_elimination_handle(grip_info, obj_attention, points);
    }
    grip_info->obj_prced_bit_rcd = obj_attention;

    for (index = 0; index < TOUCH_MAX_NUM; ++index) {
        if (!grip_info->grip_hold_status[index]) {
            continue;
        }

        if (((grip_info->obj_prev_bit & TOUCH_BIT_CHECK) >> index) & 0x01) {
            // The vendor frame still reports this id, so the held point must
            // be released now.  The stock daemon sends the corresponding
            // extra input event; the daemon reporter releases the uinput slot
            // when the returned mask no longer contains it.
            grip_status_reset(grip_info, index);
            if (grip_info->record_total_cnt) {
                --grip_info->record_total_cnt;
            }
            grip_info->grip_hold_status[index] = 0;
            g_makeup_state.pending[index] = false;
        } else if (!grip_info->eli_reject_status[index]) {
            // Keep the held point down until the make-up timer releases it.
            obj_attention |= (1 << index);
        }
    }

    grip_info->record_total_cnt = get_bit_count(obj_attention & TOUCH_BIT_CHECK);
    mutex_unlock(&grip_info->grip_mutex);
    return obj_attention;
}

void run_makeup(struct kernel_grip_info *grip_info, int slot,
                const std::function<void(int, uint16_t, uint16_t)> &on_point,
                const std::function<void(int)> &on_release)
{
    if (grip_info == nullptr || slot < 0 || slot >= TOUCH_MAX_NUM) {
        return;
    }

    if (grip_info->grip_hold_status[slot] && grip_info->sync_up_makeup[slot]) {
        const uint16_t filter_count = grip_info->coord_filter_cnt;
        if (filter_count > 0) {
            struct grip_point_info *latest = grip_info->latest_points[slot];
            const uint16_t first = static_cast<uint16_t>(acquire_matched_point(
                    latest, grip_info->coord_buf[(slot + 1) * filter_count - 1]));

            for (int in = first; in < POINT_DIFF_CNT; ++in) {
                if (!grip_info->grip_hold_status[slot]) {
                    break;
                }

                const uint16_t point_x = latest[in].x;
                const uint16_t point_y = latest[in].y;
                if (on_point) {
                    on_point(slot, point_x, point_y);
                }

                if (in < POINT_DIFF_CNT - 1 && latest[in + 1].x == point_x &&
                    latest[in + 1].y == point_y) {
                    continue;
                }
                usleep(5000);
            }
        }
    }

    if (on_release) {
        on_release(slot);
    }

    grip_status_reset(grip_info, slot);
    if (grip_info->record_total_cnt) {
        --grip_info->record_total_cnt;
    }
    grip_info->grip_hold_status[slot] = 0;
    g_makeup_state.pending[slot] = false;
    g_makeup_state.deadline_ms[slot] = 0;
}

#include "TouchPreventionInit.h"

struct kernel_grip_info *kernel_grip_init(struct device *dev)
{
    struct touchpanel_data *ts =
            static_cast<struct touchpanel_data *>(dev_get_drvdata(dev));
    if (ts == nullptr) {
        return nullptr;
    }

    struct kernel_grip_info *grip_info =
            static_cast<struct kernel_grip_info *>(std::calloc(1, sizeof(*grip_info)));
    if (grip_info == nullptr) {
        return nullptr;
    }

    grip_info->p_ts = ts;
    grip_info->tp_index = ts->tp_index;
    grip_info->coord_buf = nullptr;
    grip_info->grip_up_handle_wq = nullptr;
    grip_info->is_curved_screen_V2 =
            of_property_read_bool(dev->of_node, "prevention,curved_screen_V2");

    INIT_LIST_HEAD(&grip_info->dead_zone_list);
    INIT_LIST_HEAD(&grip_info->large_zone_list);
    INIT_LIST_HEAD(&grip_info->condition_zone_list);
    INIT_LIST_HEAD(&grip_info->elimination_zone_list);

    if (kernel_grip_init_V2(grip_info, dev) < 0) {
        // kernel_grip_init_V2 releases grip_info on its own allocation
        // failures.  Do not free it a second time here.
        return nullptr;
    }

    return grip_info;
}

}  // namespace oplus_prevention

namespace {

using ConfigMap = std::map<std::string, std::vector<uint32_t>>;

bool parseCsvU32(const char *text, std::vector<uint32_t> *out)
{
    out->clear();
    if (text == nullptr) {
        return false;
    }

    const char *cursor = text;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' ||
               *cursor == '\r' || *cursor == ',') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        char *end = nullptr;
        const unsigned long value = std::strtoul(cursor, &end, 0);
        if (end == cursor) {
            return false;
        }
        out->push_back(static_cast<uint32_t>(value));
        cursor = end;
    }

    return !out->empty();
}

void normalizePreventionKey(const std::string &name, std::string *key)
{
    if (name == "panel-coords") {
        *key = "touchpanel,panel-coords";
    } else if (name == "tx-rx-num") {
        *key = "touchpanel,tx-rx-num";
    } else if (name == "grip_disable_level") {
        *key = "prevention,grip_disable_level";
    } else {
        *key = name;
    }
}

bool parsePreventionPanel(const tinyxml2::XMLElement *panel, ConfigMap *config)
{
    bool found = false;

    for (const tinyxml2::XMLElement *modules = panel->FirstChildElement("modules");
         modules != nullptr; modules = modules->NextSiblingElement("modules")) {
        const char *module_id = modules->Attribute("id");
        if (module_id == nullptr || std::strcmp(module_id, "PreventionConfig") != 0) {
            continue;
        }
        found = true;

        for (const tinyxml2::XMLElement *param = modules->FirstChildElement("param");
             param != nullptr; param = param->NextSiblingElement("param")) {
            const char *name = param->Attribute("name");
            const tinyxml2::XMLElement *value = param->FirstChildElement("value");
            if (name == nullptr || value == nullptr || value->GetText() == nullptr) {
                continue;
            }

            std::vector<uint32_t> values;
            if (!parseCsvU32(value->GetText(), &values)) {
                continue;
            }

            std::string key;
            normalizePreventionKey(name, &key);

            // These XML entries are boolean switches in the kernel interface.
            // The stock daemon uses a present/non-zero value to enable them.
            if (key == "prevention,curved_screen_V4" ||
                key == "prevention,curved_screen_v4_3" ||
                key == "prevention,curved_screen_v4_5" ||
                key == "prevention,corner_eliminate_point_support") {
                if (values.empty() || values[0] == 0) {
                    config->erase(key);
                } else {
                    (*config)[key] = values;
                }
                continue;
            }

            (*config)[key] = values;
        }
    }

    // The stock daemon always instantiates the V2 curved-screen engine; the
    // XML does not contain that selector because the userspace code calls
    // curved_large_handle_V2() directly.
    (*config)["prevention,curved_screen_V2"] = {};

    return found;
}

bool loadPreventionConfig(const std::string &path, const std::string &panel_hint,
                          ConfigMap *config)
{
    config->clear();
    if (path.empty()) {
        return false;
    }

    tinyxml2::XMLDocument document;
    if (document.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS) {
        return false;
    }

    const tinyxml2::XMLElement *root = document.RootElement();
    if (root == nullptr) {
        return false;
    }

    const tinyxml2::XMLElement *first_panel = nullptr;
    for (const tinyxml2::XMLElement *device = root->FirstChildElement("device");
         device != nullptr; device = device->NextSiblingElement("device")) {
        for (const tinyxml2::XMLElement *panel = device->FirstChildElement("panel");
             panel != nullptr; panel = panel->NextSiblingElement("panel")) {
            if (first_panel == nullptr) {
                first_panel = panel;
            }

            const char *panel_name = panel->Attribute("name");
            if (!panel_hint.empty() && panel_name != nullptr &&
                std::strstr(panel_name, panel_hint.c_str()) != nullptr) {
                return parsePreventionPanel(panel, config);
            }
        }
    }

    if (first_panel != nullptr) {
        return parsePreventionPanel(first_panel, config);
    }

    return false;
}

}  // namespace

struct TouchPrevention::Impl {
    ConfigMap config;
    oplus_prevention::touchpanel_data ts = {};
    oplus_prevention::device dev = {};
    oplus_prevention::device_node node = {};
    oplus_prevention::kernel_grip_info *grip = nullptr;

    struct GeometryBase {
        uint16_t corner_width;
        uint16_t corner_height;
        uint16_t top_width;
        uint16_t top_height;
        uint16_t top_middle_width;
        uint16_t top_middle_height;
        uint16_t hor_top_width;
        uint16_t hor_top_height;
        uint16_t corner_hor_width;
        uint16_t corner_hor_height;
    } base_geometry = {};
    uint16_t base_disable_level = 0;
    int strength = 3;

    std::atomic<bool> running{false};
    std::thread worker;
    std::mutex callback_mutex;
    TouchPrevention::MakeupPoint on_point;
    TouchPrevention::MakeupRelease on_release;

    bool start(const std::string &panel_hint, const std::string &config_path)
    {
        if (!loadPreventionConfig(config_path, panel_hint, &config)) {
            return false;
        }
        if (config.find("touchpanel,panel-coords") == config.end() ||
            config.find("touchpanel,tx-rx-num") == config.end()) {
            return false;
        }

        oplus_prevention::g_config = &config;
        std::memset(&oplus_prevention::g_makeup_state, 0,
                    sizeof(oplus_prevention::g_makeup_state));
        std::memset(&ts, 0, sizeof(ts));
        std::memset(&dev, 0, sizeof(dev));
        std::memset(&node, 0, sizeof(node));

        oplus_prevention::mutex_init(&ts.report_mutex);
        ts.tp_index = 0;
        ts.max_num = 10;
        ts.health_monitor_support = false;
        ts.algo_info = nullptr;
        ts.input_dev = nullptr;

        dev.of_node = &node;
        dev.driver_data = &ts;

        grip = oplus_prevention::kernel_grip_init(&dev);
        if (grip == nullptr) {
            oplus_prevention::g_config = nullptr;
            return false;
        }

        ts.grip_info = grip;
        captureBaseGeometry();
        running.store(true);
        worker = std::thread([this] { workerLoop(); });
        return true;
    }

    void stop()
    {
        running.store(false);
        if (worker.joinable()) {
            worker.join();
        }

        if (grip != nullptr) {
            oplus_prevention::mutex_lock(&grip->grip_mutex);
            for (int i = 0; i < TOUCH_MAX_NUM; ++i) {
                oplus_prevention::g_makeup_state.pending[i] = false;
                oplus_prevention::g_makeup_state.deadline_ms[i] = 0;
            }
            oplus_prevention::mutex_unlock(&grip->grip_mutex);

            oplus_prevention::kernel_grip_release(grip);
            grip = nullptr;
        }

        ts.grip_info = nullptr;
        if (oplus_prevention::g_config == &config) {
            oplus_prevention::g_config = nullptr;
        }
    }

    void reset()
    {
        if (grip == nullptr) {
            return;
        }

        oplus_prevention::mutex_lock(&grip->grip_mutex);
        for (int i = 0; i < TOUCH_MAX_NUM; ++i) {
            oplus_prevention::g_makeup_state.pending[i] = false;
            oplus_prevention::g_makeup_state.deadline_ms[i] = 0;
        }
        oplus_prevention::kernel_grip_reset(grip);
        oplus_prevention::mutex_unlock(&grip->grip_mutex);
    }

    void captureBaseGeometry()
    {
        if (grip == nullptr) {
            return;
        }

        base_geometry.corner_width = grip->large_corner_width;
        base_geometry.corner_height = grip->large_corner_height;
        base_geometry.top_width = grip->large_top_width;
        base_geometry.top_height = grip->large_top_height;
        base_geometry.top_middle_width = grip->large_top_middle_width;
        base_geometry.top_middle_height = grip->large_top_middle_height;
        base_geometry.hor_top_width = grip->large_hor_top_x_width;
        base_geometry.hor_top_height = grip->large_hor_top_y_height;
        base_geometry.corner_hor_width = grip->large_corner_hor_x_width;
        base_geometry.corner_hor_height = grip->large_corner_hor_y_height;
        base_disable_level = grip->grip_disable_level;
    }

    static uint16_t scaleDimension(uint16_t value, double scale)
    {
        if (value == 0) {
            return 0;
        }
        long scaled = static_cast<long>(static_cast<double>(value) * scale + 0.5);
        if (scaled < 1) {
            scaled = 1;
        } else if (scaled > 0xFFFF) {
            scaled = 0xFFFF;
        }
        return static_cast<uint16_t>(scaled);
    }

    void applyGeometryScale(double scale)
    {
        grip->large_corner_width =
                scaleDimension(base_geometry.corner_width, scale);
        grip->large_corner_height =
                scaleDimension(base_geometry.corner_height, scale);
        grip->large_top_width = scaleDimension(base_geometry.top_width, scale);
        grip->large_top_height = scaleDimension(base_geometry.top_height, scale);
        grip->large_top_middle_width =
                scaleDimension(base_geometry.top_middle_width, scale);
        grip->large_top_middle_height =
                scaleDimension(base_geometry.top_middle_height, scale);
        grip->large_hor_top_x_width =
                scaleDimension(base_geometry.hor_top_width, scale);
        grip->large_hor_top_y_height =
                scaleDimension(base_geometry.hor_top_height, scale);
        grip->large_corner_hor_x_width =
                scaleDimension(base_geometry.corner_hor_width, scale);
        grip->large_corner_hor_y_height =
                scaleDimension(base_geometry.corner_hor_height, scale);
    }

    void setStrength(int level)
    {
        if (level < 0) {
            level = 0;
        } else if (level > 5) {
            level = 5;
        }
        strength = level;

        if (grip == nullptr) {
            return;
        }

        oplus_prevention::mutex_lock(&grip->grip_mutex);
        for (int i = 0; i < TOUCH_MAX_NUM; ++i) {
            oplus_prevention::g_makeup_state.pending[i] = false;
            oplus_prevention::g_makeup_state.deadline_ms[i] = 0;
        }

        if (level == 0) {
            grip->grip_disable_level =
                    (1 << oplus_prevention::GRIP_DISABLE_LARGE) |
                    (1 << oplus_prevention::GRIP_DISABLE_ELI) |
                    (1 << oplus_prevention::GRIP_DISABLE_UP2CANCEL);
        } else {
            grip->grip_disable_level = base_disable_level;
            // Match the firmware level ratios: level 3 is 0x0E / 0x0E.
            const double scale = (10.0 + 2.0 * (level - 1)) / 14.0;
            applyGeometryScale(scale);
        }
        oplus_prevention::kernel_grip_reset(grip);
        oplus_prevention::mutex_unlock(&grip->grip_mutex);
    }

    uint32_t process(const std::array<TouchPreventionPoint, 10> &in, uint32_t mask,
                     std::array<TouchPreventionPoint, 10> *out)
    {
        if (out != nullptr) {
            *out = in;
        }
        if (grip == nullptr) {
            return mask & 0x3FF;
        }

        oplus_prevention::point_info points[TOUCH_MAX_NUM] = {};
        for (int i = 0; i < TOUCH_MAX_NUM; ++i) {
            points[i].x = in[i].x;
            points[i].y = in[i].y;
            points[i].z = in[i].z;
            points[i].width_major = in[i].width_major;
            points[i].touch_major = in[i].touch_major;
            points[i].status = in[i].status;
            points[i].tx_press = in[i].tx_press;
            points[i].rx_press = in[i].rx_press;
            points[i].tx_er = in[i].tx_er;
            points[i].rx_er = in[i].rx_er;
            points[i].type = oplus_prevention::AREA_NORMAL;
        }

        const int result = oplus_prevention::notify_prevention_handle(
                grip, static_cast<int>(mask & 0x3FF), points);

        if (out != nullptr) {
            for (int i = 0; i < TOUCH_MAX_NUM; ++i) {
                (*out)[i].x = points[i].x;
                (*out)[i].y = points[i].y;
                (*out)[i].status = points[i].status;
                (*out)[i].tx_press = points[i].tx_press;
                (*out)[i].rx_press = points[i].rx_press;
                (*out)[i].tx_er = points[i].tx_er;
                (*out)[i].rx_er = points[i].rx_er;
            }
        }

        return static_cast<uint32_t>(result) & 0x3FF;
    }

    void setCallbacks(TouchPrevention::MakeupPoint point,
                      TouchPrevention::MakeupRelease release)
    {
        std::lock_guard<std::mutex> lock(callback_mutex);
        on_point = std::move(point);
        on_release = std::move(release);
    }

    void workerLoop()
    {
        while (running.load()) {
            usleep(2000);
            if (!running.load() || grip == nullptr) {
                continue;
            }

            const int64_t now = oplus_prevention::ktime_to_ms(oplus_prevention::ktime_get());
            for (int slot = 0; slot < TOUCH_MAX_NUM; ++slot) {
                oplus_prevention::mutex_lock(&grip->grip_mutex);
                const bool due = oplus_prevention::g_makeup_state.pending[slot] &&
                                 now >= oplus_prevention::g_makeup_state.deadline_ms[slot];
                if (due) {
                    TouchPrevention::MakeupPoint point;
                    TouchPrevention::MakeupRelease release;
                    {
                        std::lock_guard<std::mutex> lock(callback_mutex);
                        point = on_point;
                        release = on_release;
                    }
                    oplus_prevention::run_makeup(grip, slot, point, release);
                }
                oplus_prevention::mutex_unlock(&grip->grip_mutex);
            }
        }
    }
};

TouchPrevention::TouchPrevention() = default;

TouchPrevention::~TouchPrevention()
{
    stop();
}

bool TouchPrevention::start(const std::string &panel_hint, const std::string &config_path)
{
    stop();
    mImpl = std::make_unique<Impl>();
    if (!mImpl->start(panel_hint, config_path)) {
        mImpl.reset();
        return false;
    }
    return true;
}

void TouchPrevention::stop()
{
    if (mImpl != nullptr) {
        mImpl->stop();
        mImpl.reset();
    }
}

bool TouchPrevention::valid() const
{
    return mImpl != nullptr && mImpl->grip != nullptr;
}

uint32_t TouchPrevention::process(const std::array<TouchPreventionPoint, 10> &in, uint32_t in_mask,
                                  std::array<TouchPreventionPoint, 10> *out)
{
    if (mImpl == nullptr) {
        if (out != nullptr) {
            *out = in;
        }
        return in_mask & 0x3FF;
    }
    return mImpl->process(in, in_mask, out);
}

void TouchPrevention::reset()
{
    if (mImpl != nullptr) {
        mImpl->reset();
    }
}

void TouchPrevention::setStrength(int level)
{
    if (mImpl != nullptr) {
        mImpl->setStrength(level);
    }
}

void TouchPrevention::set_makeup_callbacks(MakeupPoint on_point, MakeupRelease on_release)
{
    if (mImpl != nullptr) {
        mImpl->setCallbacks(std::move(on_point), std::move(on_release));
    }
}
