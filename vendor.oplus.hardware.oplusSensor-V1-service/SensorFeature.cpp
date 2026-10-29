/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Source implementation of vendor.oplus.hardware.oplusSensor.ISensorFeature.
 *
 * The stock HAL is an engineering/factory calibration service.  It keeps the
 * calibration data in the sensors registry, the camera directory, the OPLUS
 * reserve partition and a small set of proc/sysfs nodes.  This implementation
 * preserves those interfaces and the stock AIDL dispatch; see RE-NOTES.md for
 * the reverse-engineered layout.
 */

#define LOG_TAG "OplusSensorFeature"

#include "SensorFeature.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_status.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <json/json.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace aidl::vendor::oplus::hardware::oplusSensor {
namespace {

constexpr char kRegistryDir[] = "/mnt/vendor/persist/sensors/registry/registry";
constexpr char kCameraDir[] = "/mnt/vendor/persist/camera";
constexpr char kEngineerDir[] = "/mnt/vendor/persist/engineermode";
constexpr char kSpectrumDir[] = "/mnt/vendor/persist/engineermode/sensor/spectrum";
constexpr char kReservePath[] = "/dev/block/by-name/oplusreserve1";
constexpr char kConfigPath[] = "/odm/etc/sensor/sensor_config.json";
constexpr char kPressureNode[] = "/proc/sensor/pressure_cali/offset";
constexpr char kMagSoftParaNode[] = "/proc/sensor/mag_para/soft_para";
constexpr char kAlsRowCoeNode[] = "/proc/sensor/als_cali/row_coe";
constexpr char kPadAlsRowCoeNode[] = "/proc/sensor/pad_als/row_coe";

// Sensor type IDs used by the stock engineering AIDL.
constexpr int32_t kTypeGsensor = 1;
constexpr int32_t kTypeMag = 2;
constexpr int32_t kTypeGyro = 4;
constexpr int32_t kTypeLight = 5;
constexpr int32_t kTypePressure = 6;
constexpr int32_t kTypeProximity = 8;
constexpr int32_t kTypeGsensorSub = 0x0fff0001;
constexpr int32_t kTypeGyroSub = 0x0fff0004;
constexpr int32_t kTypeSpectrum6000k = 999999;
constexpr int32_t kTypeRgbA = 0x1fa2639;
constexpr int32_t kTypeHall = 0x1fa263a;
constexpr int32_t kTypeFakeProximityA = 0x1fa263d;
constexpr int32_t kTypeAngleHall = 0x1fa263e;
constexpr int32_t kTypeStructureProximity = 0x1fa263f;
constexpr int32_t kTypeFreeFall = 0x1fa2641;
constexpr int32_t kTypeScreenLightA = 0x1fa2653;
constexpr int32_t kTypeRgbB = 0x1fa2654;
constexpr int32_t kTypeHingeA = 0x1fa265e;
constexpr int32_t kTypeRgbC = 0x1fa2662;
constexpr int32_t kTypeRgbD = 0x1fa2663;
constexpr int32_t kTypeRgb = 0x1fa266c;
constexpr int32_t kTypeBackLight = 0x1fa266f;
constexpr int32_t kTypeSpectral = 0x1fa2673;
constexpr int32_t kTypeFakeProximityB = 0x1fa2676;
constexpr int32_t kTypeHingeB = 0x1fa2677;
constexpr int32_t kTypeRgbLeak = 0x1fa267e;
constexpr int32_t kTypePadLight = 0x1fa269d;
constexpr int32_t kTypeVicePadLight = 0x1fa269e;
constexpr int32_t kTypeScreenLightB = 0x1fa269f;
constexpr int32_t kTypeRgbGain = 0x1fa2a1a;
constexpr int32_t kTypeRgbPad = 0x1fa2a19;
constexpr int32_t kTypeScreenSn = 0x1fa2a1c;
constexpr int32_t kTypeConsumerIr = 0x1fa2a1f;

constexpr int32_t kSpectrumTestType = 0xf423c;
constexpr int32_t kSpectrumSnType = 0xf423d;
constexpr int32_t kSpectrumWavelengthType = 0xf423e;

// Stock reserve-partition layout.  The stock HAL selects one set from the
// storage type (UFS or eMMC); the offsets are byte offsets in the OPLUS
// reserve partition.
constexpr off_t kUfsPressure = 0x434600;
constexpr off_t kEmmcPressure = 0x41b200;
constexpr off_t kUfsFreeFall = 0x434604;
constexpr off_t kEmmcFreeFall = 0x41b204;
constexpr off_t kUfsConsumerIr = 0x434700;
constexpr off_t kEmmcConsumerIr = 0x41b300;
constexpr off_t kUfsScreenLight = 0x434400;
constexpr off_t kEmmcScreenLight = 0x41ae00;
constexpr off_t kUfsPadLight = 0x434420;
constexpr off_t kEmmcPadLight = 0x41ae20;
constexpr off_t kUfsVicePadLight = 0x434440;
constexpr off_t kEmmcVicePadLight = 0x41ae40;
constexpr off_t kUfsScreenSn = 0x434480;
constexpr off_t kEmmcScreenSn = 0x41ae80;
constexpr off_t kUfsMainScreenSn = 0x4344c0;
constexpr off_t kEmmcMainScreenSn = 0x41aec0;
constexpr off_t kUfsStructureProximity = 0x434200;

// Stock stores every calibration object below this address.
constexpr size_t kMaxNodeValueLength = 0x1000;

struct SensorNode {
    const char* name;
    const char* dir;
    size_t read_size;
};

// The stock 23-entry node table.  sensorReadNodeFile/sensorWriteNodeFile
// reject every name outside this allowlist.
constexpr SensorNode kNodeTable[] = {
        {"red_max_lux", "/proc/sensor/als_cali", 16},
        {"green_max_lux", "/proc/sensor/als_cali", 16},
        {"blue_max_lux", "/proc/sensor/als_cali", 16},
        {"white_max_lux", "/proc/sensor/als_cali", 16},
        {"row_coe", "/proc/sensor/als_cali", 16},
        {"cali_coe", "/proc/sensor/als_cali", 16},
        {"offset", "/proc/sensor/pressure_cali", 16},
        {"prjName", "/proc/oplusVersion", 16},
        {"ps", "/sys/bus/platform/drivers/als_ps", 16},
        {"ps_raw", "/sys/bus/platform/drivers/als_ps", 16},
        {"ps_state", "/sys/bus/platform/drivers/als_ps", 16},
        {"cali", "/sys/bus/platform/drivers/als_ps", 16},
        {"gain_als", "/sys/bus/platform/drivers/als_ps", 16},
        {"als", "/sys/bus/platform/drivers/als_ps", 16},
        {"test_id", "/sys/bus/platform/drivers/msensor", 16},
        {"magnet_close", "/sys/bus/platform/drivers/msensor", 16},
        {"magnet_leave", "/sys/bus/platform/drivers/msensor", 16},
        {"get_ic_modle", "/sys/bus/platform/drivers/msensor", 16},
        {"factory_step_debounce", "/sys/bus/platform/drivers/gsensor", 16},
        {"hw_region_id", "/proc/devinfo", 16},
        {"lcd", "/proc/devinfo", 100},
        {"lcd_s", "/proc/devinfo", 100},
        {"ssc_screenshot_info", "/dev", 128},
};

constexpr const char* kSpectralKeys[] = {
        "R_cali", "G_cali", "B_cali", "C_cali", "W_cali", "F_cali",
        "ch5",    "ch6",    "ch7",    "ch8",    "ch9",    "ch10",
        "ch11",   "ch12",
};

constexpr const char* kSpectralOutputKeys[] = {
        "Z", "Y", "XL", "NIR", "VIS", "F", "F2",
        "F3", "F4", "F6", "F1", "F7", "F8", "F5",
};

constexpr const char* kRgbKeys[] = {
        "R_cali", "G_cali", "B_cali", "C_cali", "W_cali", "F_cali",
        "R_cali", "Gain_1024_R", "Gain_1024_G", "Gain_1024_B",
        "Gain_1024_C", "Gain_2048_R", "Gain_2048_G", "Gain_2048_B",
        "Gain_2048_C",
};

constexpr std::array<const char*, 4> kChannels = {"r", "g", "b", "c"};
constexpr std::array<const char*, 4> kChannelNames = {"R", "G", "B", "C"};
constexpr std::array<const char*, 4> kComponentGroups = {
        "R_comp", "G_comp", "B_comp", "C_comp",
};
constexpr std::array<const char*, 4> kComponentPrefixes = {
        "R_VIEW", "G_VIEW", "B_VIEW", "W_VIEW",
};

bool isEmmcStorage() {
    static const bool emmc = [] {
        const std::string bootDevice =
                android::base::GetProperty("ro.boot.bootdevice", "");
        if (!bootDevice.empty()) {
            return bootDevice.find("ufs") == std::string::npos;
        }
        return access("/sys/block/mmcblk0", F_OK) == 0;
    }();
    return emmc;
}

// The stock constructor calls getScreenLightSensorCaliData(), which sets this
// flag when the persist/engineermode max-lux files exist.  The flag selects
// the legacy "OOS storage mode" schemas for RGB, proximity and screen light.
bool isOosStorageMode() {
    static const bool mode = [] {
        constexpr const char* kFiles[] = {
                "/mnt/vendor/persist/engineermode/red_max_lux",
                "/mnt/vendor/persist/engineermode/green_max_lux",
                "/mnt/vendor/persist/engineermode/blue_max_lux",
                "/mnt/vendor/persist/engineermode/white_max_lux",
        };
        bool any = false;
        for (const char* path : kFiles) {
            const int fd = open(path, O_RDONLY | O_CLOEXEC);
            if (fd < 0) {
                continue;
            }
            char buffer[0x20];
            const ssize_t count = read(fd, buffer, sizeof(buffer));
            close(fd);
            if (count > 0) {
                any = true;
            }
        }
        const std::string soc =
                android::base::GetProperty("ro.soc.model", "");
        return any && soc != "SM4350";
    }();
    return mode;
}

off_t reserveOffset(off_t ufsOffset, off_t emmcOffset) {
    return isEmmcStorage() ? emmcOffset : ufsOffset;
}

int readInterface(const std::string& path, off_t offset, void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return -1;
    }
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    ssize_t result = pread(fd, data, size, offset);
    if (result < 0 && errno == ESPIPE) {
        if (lseek(fd, offset, SEEK_SET) >= 0) {
            result = read(fd, data, size);
        }
    }
    close(fd);
    return result < 0 ? -1 : static_cast<int>(result);
}

int writeInterface(const std::string& path, off_t offset, const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return -1;
    }
    int flags = O_WRONLY | O_CLOEXEC;
    if (isOosStorageMode() && path.find("calicoe_bak") != std::string::npos) {
        flags |= O_CREAT;
    }
    const int fd = open(path.c_str(), flags, 0664);
    if (fd < 0) {
        return -1;
    }
    ssize_t result = pwrite(fd, data, size, offset);
    if (result < 0 && (errno == ESPIPE || errno == EINVAL)) {
        if (lseek(fd, offset, SEEK_SET) >= 0) {
            result = write(fd, data, size);
        }
    }
    close(fd);
    return result < 0 ? -1 : static_cast<int>(result);
}

bool readTextFile(const std::string& path, std::string* text) {
    if (text == nullptr) {
        return false;
    }
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    *text = buffer.str();
    return true;
}

bool writeTextFile(const std::string& path, const std::string& text) {
    std::ofstream output(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return output.good();
}

std::string trimNul(std::string value) {
    const size_t nul = value.find('\0');
    if (nul != std::string::npos) {
        value.resize(nul);
    }
    return value;
}

std::string readNodeText(const std::string& path, size_t size) {
    if (size == 0) {
        return {};
    }
    std::vector<char> buffer(size + 1, '\0');
    const int count = readInterface(path, 0, buffer.data(), size);
    if (count <= 0) {
        return {};
    }
    return trimNul(std::string(buffer.data(), static_cast<size_t>(count)));
}

int writeNodeText(const std::string& path, const std::string& value) {
    return writeInterface(path, 0, value.data(), value.size());
}

bool parseJson(const std::string& text, Json::Value* root) {
    if (root == nullptr) {
        return false;
    }
    Json::Reader reader;
    return reader.parse(text, *root, false);
}

std::optional<Json::Value> parseJsonFile(const std::string& path) {
    std::string text;
    if (!readTextFile(path, &text) || text.empty()) {
        return std::nullopt;
    }
    Json::Value root;
    if (!parseJson(text, &root)) {
        return std::nullopt;
    }
    return root;
}

bool writeJsonFile(const std::string& path, const Json::Value& root) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return writeTextFile(path, Json::writeString(builder, root));
}

std::optional<Json::Value> registryNode(const std::string& key) {
    std::optional<Json::Value> root =
            parseJsonFile(std::string(kRegistryDir) + "/" + key);
    if (!root || !root->isObject()) {
        return std::nullopt;
    }
    if (!root->isMember(key)) {
        return std::nullopt;
    }
    return (*root)[key];
}

std::optional<Json::Value> registryFile(const std::string& name, int type = 0) {
    const std::string dir = type == 0 ? kRegistryDir : kSpectrumDir;
    return parseJsonFile(dir + "/" + name);
}

std::string calFileName(const std::string& key) {
    std::optional<Json::Value> node = registryNode(key);
    if (!node || !node->isObject()) {
        return {};
    }
    const Json::Value& calPath = (*node)["cal_path"];
    if (!calPath.isObject() || !calPath["data"].isString()) {
        return {};
    }
    return calPath["data"].asString();
}

std::optional<Json::Value> calData(const std::string& key) {
    const std::string fileName = calFileName(key);
    if (fileName.empty()) {
        return std::nullopt;
    }
    std::optional<Json::Value> file = registryFile(fileName);
    if (!file || !file->isObject()) {
        return std::nullopt;
    }
    return file;
}

bool nodeIntValue(const Json::Value& node, int* value) {
    if (value == nullptr || !node.isObject()) {
        return false;
    }
    const Json::Value& data = node["data"];
    if (!data.isString()) {
        return false;
    }
    *value = std::atoi(data.asString().c_str());
    return true;
}

std::string jsonVersion(const Json::Value& node) {
    const Json::Value& version = node["ver"];
    return version.isString() ? version.asString() : std::string();
}

std::string jsonString(const Json::Value& node, const char* key) {
    const Json::Value& value = node[key];
    return value.isString() ? value.asString() : std::string();
}

int analyzeNodeInt(const Json::Value& node) {
    if (!node.isObject()) {
        return 0;
    }
    const Json::Value& data = node["data"];
    if (!data.isString()) {
        return 0;
    }
    return std::atoi(data.asString().c_str());
}

void addStringField(Json::Value* result, const char* key,
                    const Json::Value& node) {
    if (result == nullptr || !node.isObject()) {
        return;
    }
    const std::string version = jsonVersion(node);
    if (!version.empty()) {
        (*result)[key] = version;
    }
}

bool addLightScaleFields(Json::Value* result, const Json::Value& cal) {
    if (result == nullptr || !cal.isObject()) {
        return false;
    }
    const Json::Value& scale = cal["scale"];
    if (!scale.isObject()) {
        return false;
    }
    const Json::Value& data = scale["data"];
    if (!data.isString()) {
        return false;
    }
    const int value = std::atoi(data.asString().c_str());
    if (value == 0) {
        return false;
    }
    (*result)["cali_lux"] = std::to_string(1000000 / value);
    (*result)["cali_para"] = std::to_string(value);
    (*result)["cali_para_extra"] = data.asString();
    const std::string version = jsonVersion(scale);
    if (!version.empty()) {
        (*result)["cali_para_ver"] = version;
    }
    return true;
}

std::string valueText(const Json::Value& value) {
    if (value.isString()) {
        return value.asString();
    }
    if (value.isBool()) {
        return value.asBool() ? "true" : "false";
    }
    if (value.isInt64()) {
        return std::to_string(value.asInt64());
    }
    if (value.isUInt64()) {
        return std::to_string(value.asUInt64());
    }
    if (value.isDouble()) {
        std::ostringstream output;
        output << value.asDouble();
        return output.str();
    }
    if (value.isObject() || value.isArray()) {
        return value.toStyledString();
    }
    return {};
}

Json::Value makeGsensorLike(const std::string& key) {
    std::optional<Json::Value> cal = calData(key);
    if (!cal || !cal->isObject()) {
        return Json::Value(Json::objectValue);
    }
    const Json::Value& root = *cal;
    static constexpr std::array<const char*, 3> kAxes = {"x", "y", "z"};
    static constexpr std::array<const char*, 3> kDataKeys = {
            "offset_x", "offset_y", "offset_z",
    };
    static constexpr std::array<const char*, 3> kVersionKeys = {
            "x_ver", "y_ver", "z_ver",
    };
    Json::Value result(Json::objectValue);
    for (size_t i = 0; i < kAxes.size(); ++i) {
        const Json::Value& item = root[kAxes[i]];
        if (item.isObject()) {
            if (item["data"].isString()) {
                result[kDataKeys[i]] = item["data"].asString();
            }
            if (item["ver"].isString()) {
                result[kVersionKeys[i]] = item["ver"].asString();
            }
        } else if (!item.isNull()) {
            result[kDataKeys[i]] = valueText(item);
        }
    }
    return result;
}

int analyzeRgbValue(const Json::Value& cal, int index) {
    if (index < 1 || index > 15) {
        return -1;
    }
    const Json::Value& item = cal[kRgbKeys[index - 1]];
    if (!item.isObject()) {
        return -1;
    }
    const Json::Value& data = item["data"];
    if (!data.isString()) {
        return 0;
    }
    return std::atoi(data.asString().c_str());
}

int analyzeSpectralValue(const Json::Value& cal, int index) {
    if (index < 1 || index > 14) {
        return -1;
    }
    const Json::Value& item = cal[kSpectralKeys[index - 1]];
    if (!item.isObject()) {
        return -1;
    }
    const Json::Value& data = item["data"];
    if (!data.isString()) {
        return 0;
    }
    return std::atoi(data.asString().c_str());
}

void addRgbGains(const Json::Value& cal, Json::Value* result) {
    static constexpr std::array<std::pair<int, const char*>, 8> kGains = {{
            {8, "Gain_1024_R"},
            {9, "Gain_1024_G"},
            {10, "Gain_1024_B"},
            {11, "Gain_1024_C"},
            {12, "Gain_2048_R"},
            {13, "Gain_2048_G"},
            {14, "Gain_2048_B"},
            {15, "Gain_2048_C"},
    }};
    for (const auto& gain : kGains) {
        const int value = analyzeRgbValue(cal, gain.first);
        if (value != -1) {
            (*result)[gain.second] = std::to_string(value);
        }
    }
}

void addRgbLeakFields(bool pad, Json::Value* result) {
    const char* key = pad ? "pad_wise_rgb_comp_scale" : "wise_rgb_comp_scale";
    std::optional<Json::Value> node = registryNode(key);
    if (!node || !node->isObject()) {
        return;
    }
    for (int level = 0; level <= 8; ++level) {
        const std::string comp = "comp" + std::to_string(level);
        const Json::Value& group = (*node)[comp];
        if (!group.isObject()) {
            continue;
        }
        const Json::Value& cComp = group["C_comp"];
        if (cComp.isObject()) {
            for (size_t channel = 0; channel < kChannels.size(); ++channel) {
                const Json::Value& value = cComp[kChannels[channel]];
                if (!value.isNull()) {
                    (*result)[std::string("W_VIEW_") + kChannelNames[channel] +
                              "_" + std::to_string(level)] = value;
                }
            }
        }
        if (level > 2) {
            continue;
        }
        const char* suffix = level == 0 ? "" : (level == 1 ? "_L" : "_M");
        for (size_t groupIndex = 0; groupIndex < kComponentGroups.size();
             ++groupIndex) {
            const Json::Value& component = group[kComponentGroups[groupIndex]];
            if (!component.isObject()) {
                continue;
            }
            for (size_t channel = 0; channel < kChannels.size(); ++channel) {
                const Json::Value& value = component[kChannels[channel]];
                if (!value.isNull()) {
                    (*result)[std::string(kComponentPrefixes[groupIndex]) + "_" +
                              kChannelNames[channel] + "_MAX" + suffix] = value;
                }
            }
        }
    }
}

Json::Value makeRgbLike(int32_t sensorType) {
    const char* key = nullptr;
    switch (sensorType) {
        case kTypeRgbA:
        case kTypeRgbB:
            key = "devinfo.rgb";
            break;
        case kTypeRgbC:
            key = "devinfo.pad_rgb";
            break;
        case kTypeRgbD:
            key = "devinfo.vice_rgb";
            break;
        case kTypeRgb:
            key = "devinfo.rgb_rear";
            break;
        case kTypeRgbPad:
        case kTypeRgbGain:
            key = "devinfo.pad_rgb";
            break;
        default:
            return Json::Value(Json::objectValue);
    }

    std::optional<Json::Value> cal = calData(key);
    if (!cal || !cal->isObject()) {
        return Json::Value(Json::objectValue);
    }
    const Json::Value& root = *cal;
    Json::Value result(Json::objectValue);

    if (sensorType == kTypeRgbGain) {
        addRgbGains(root, &result);
        return result;
    }

    static constexpr std::array<const char*, 6> kChannelsOut = {
            "R_cali", "G_cali", "B_cali", "C_cali", "W_cali", "F_cali",
    };
    for (size_t i = 0; i < kChannelsOut.size(); ++i) {
        result[kChannelsOut[i]] =
                std::to_string(analyzeRgbValue(root, static_cast<int>(i) + 1));
    }

    const bool oos = isOosStorageMode();
    if (!oos) {
        const Json::Value& scale = root["scale"];
        if (!scale.isObject()) {
            const Json::Value& rCali = root["R_cali"];
            const std::string version = jsonVersion(rCali);
            if (!version.empty()) {
                result["cali_para_ver"] = version;
            }
            const Json::Value& clockScale = root["clock_scale"];
            if (clockScale.isObject() && clockScale["data"].isString()) {
                result["clock"] = std::to_string(
                        std::atoi(clockScale["data"].asString().c_str()));
            }
        } else {
            const std::string version = jsonVersion(scale);
            if (!version.empty()) {
                result["cali_para_ver"] = version;
            }
            if (scale["data"].isString()) {
                result["scale"] = scale["data"].asString();
            }
        }

        switch (sensorType) {
            case kTypeRgbC:
                addRgbGains(root, &result);
                addRgbLeakFields(true, &result);
                break;
            case kTypeRgbB:
                addRgbLeakFields(false, &result);
                break;
            case kTypeRgbA:
                addRgbGains(root, &result);
                break;
            default:
                break;
        }
        return result;
    }

    switch (sensorType) {
        case kTypeRgbA: {
            const Json::Value& scale = root["scale"];
            if (scale.isObject()) {
                const std::string version = jsonVersion(scale);
                if (!version.empty()) {
                    result["cali_para_ver"] = version;
                }
                if (scale["data"].isString()) {
                    result["scale"] = scale["data"].asString();
                }
            } else {
                const std::string version = jsonVersion(root["R_cali"]);
                if (!version.empty()) {
                    result["cali_para_ver"] = version;
                }
            }
            const Json::Value& bias = root["bias"];
            if (bias.isObject() && bias["data"].isString()) {
                result["R_cali"] = bias["data"].asString();
            }
            addRgbGains(root, &result);
            break;
        }
        case kTypeRgbB:
            addRgbLeakFields(false, &result);
            break;
        case kTypeRgbC:
            addRgbGains(root, &result);
            addRgbLeakFields(true, &result);
            break;
        case kTypeRgb: {
            const Json::Value& scaleL = root["scale_l"];
            if (scaleL.isObject()) {
                const std::string version = jsonVersion(scaleL);
                if (!version.empty()) {
                    result["cali_para_ver"] = version;
                }
                if (scaleL["data"].isString()) {
                    result["scale"] = scaleL["data"].asString();
                }
            } else {
                const std::string version = jsonVersion(root["R_cali"]);
                if (!version.empty()) {
                    result["cali_para_ver"] = version;
                }
            }
            const Json::Value& scaleH = root["scale_h"];
            if (scaleH.isObject() && scaleH["data"].isString()) {
                result["R_cali"] = scaleH["data"].asString();
            }
            break;
        }
        default:
            break;
    }
    return result;
}

void addSpectralGainFields(const Json::Value& cal, const char* suffix,
                           Json::Value* result) {
    for (size_t i = 0; i < std::size(kSpectralOutputKeys); ++i) {
        const int value = analyzeSpectralValue(cal, static_cast<int>(i) + 1);
        (*result)[std::string(kSpectralOutputKeys[i]) + "_cali_" + suffix] =
                std::to_string(value);
    }
}

bool getSpectralSnMatchResult() {
    std::optional<Json::Value> root = registryFile("spectrum_sn.json", 1);
    if (!root || !root->isObject()) {
        return false;
    }
    std::optional<Json::Value> serial =
            registryNode("cct_rear_platform.spectral_serial.serial_num");
    if (!serial || !serial->isObject()) {
        return false;
    }
    int low = 0;
    int high = 0;
    nodeIntValue((*serial)["low_32bit"], &low);
    nodeIntValue((*serial)["high_8bit"], &high);
    std::string sn = jsonString(*root, "spectrum_sn");
    if (sn.size() > 10) {
        sn.resize(10);
    }
    const uint64_t fileValue =
            std::strtoull(("0x" + sn).c_str(), nullptr, 16);
    const uint64_t driverValue =
            (static_cast<uint64_t>(static_cast<uint32_t>(high)) << 32) |
            static_cast<uint32_t>(low);
    return fileValue == driverValue;
}

bool addSpectralPair(const char* gain1Key, const char* gain2Key,
                     const char* outputKey, Json::Value* output) {
    std::optional<Json::Value> gain1 = registryNode(gain1Key);
    if (!gain1 || !gain1->isObject()) {
        return false;
    }
    Json::Value entry(Json::objectValue);
    addSpectralGainFields(*gain1, "gain1", &entry);

    std::optional<Json::Value> gain2 = registryNode(gain2Key);
    if (!gain2 || !gain2->isObject()) {
        return false;
    }
    addSpectralGainFields(*gain2, "gain2", &entry);
    (*output)[outputKey] = entry;
    return true;
}

bool addSpectralGold(const char* key, const char* outputKey,
                     Json::Value* output) {
    std::optional<Json::Value> gold = registryNode(key);
    if (!gold || !gold->isObject()) {
        return false;
    }
    Json::Value entry(Json::objectValue);
    for (size_t i = 0; i < std::size(kSpectralOutputKeys); ++i) {
        const int value = analyzeSpectralValue(*gold, static_cast<int>(i) + 1);
        entry[std::string(kSpectralOutputKeys[i]) + "_cali"] =
                std::to_string(value);
    }
    (*output)[outputKey] = entry;
    return true;
}

bool setSpectrumCaliDataToPersist() {
    Json::Value data(Json::objectValue);
    if (!addSpectralPair("cct_rear_platform.sync_scale.512_3000k",
                         "cct_rear_platform.sync_scale.2048_3000k",
                         "cali_3000k_data", &data) ||
        !addSpectralPair("cct_rear_platform.sync_scale.512_6000k",
                         "cct_rear_platform.sync_scale.2048_6000k",
                         "cali_6000k_data", &data) ||
        !addSpectralPair("cct_rear_platform.ch_scale.fac_cal",
                         "cct_rear_platform.ch_scale.fac1_cal",
                         "cali_average_data", &data) ||
        !addSpectralGold("cct_rear_platform.cali_gold_data.gold_3000k",
                         "cali_gold_3000k", &data) ||
        !addSpectralGold("cct_rear_platform.cali_gold_data.gold_6000k",
                         "cali_gold_6000k", &data)) {
        return false;
    }
    Json::Value wrapper(Json::objectValue);
    wrapper["spectrum_cali_data"] = data;
    return writeJsonFile(std::string(kCameraDir) + "/spectrum_cali_data", wrapper);
}

Json::Value makeSpectralLike(int32_t sensorType) {
    Json::Value result(Json::objectValue);
    if (sensorType == kTypeSpectrum6000k) {
        std::optional<Json::Value> gain1 =
                registryNode("cct_rear_platform.sync_scale.512_6000k");
        if (!gain1 || !gain1->isObject()) {
            return Json::Value(Json::objectValue);
        }
        addSpectralGainFields(*gain1, "gain1", &result);
        std::optional<Json::Value> gain2 =
                registryNode("cct_rear_platform.sync_scale.2048_6000k");
        if (!gain2 || !gain2->isObject()) {
            return Json::Value(Json::objectValue);
        }
        addSpectralGainFields(*gain2, "gain2", &result);
        if (!setSpectrumCaliDataToPersist()) {
            return Json::Value(Json::objectValue);
        }
        return result;
    }

    std::optional<Json::Value> serial =
            registryNode("cct_rear_platform.spectral_serial.serial_num");
    if (serial && serial->isObject()) {
        int low = 0;
        int high = 0;
        if (nodeIntValue((*serial)["low_32bit"], &low)) {
            result["sn_low_32bit"] = std::to_string(low);
        }
        if (nodeIntValue((*serial)["high_8bit"], &high)) {
            result["sn_high_8bit"] = std::to_string(high);
        }
    }
    result["wavelength_cali"] = getSpectralSnMatchResult() ? "1" : "0";

    std::optional<Json::Value> gain1 = calData("devinfo.rgb_rear");
    if (gain1 && gain1->isObject()) {
        addSpectralGainFields(*gain1, "gain1", &result);
    }
    std::optional<Json::Value> gain2 = calData("devinfo.rgb_rear1");
    if (!gain2 || !gain2->isObject()) {
        return Json::Value(Json::objectValue);
    }
    addSpectralGainFields(*gain2, "gain2", &result);
    return result;
}

int mergeProximityOffset(int value) {
    if ((value >> 26 & 1) == 0) {
        return value;
    }
    int merged = 0;
    if ((value & 0x1000000) != 0) {
        merged = -(value & 0xff);
    } else {
        merged = value & 0xff;
    }
    merged += ((value >> 8) & 0x1f) * 0x1b;
    merged += ((((value >> 17) & 0x100) | ((value >> 16) & 0xff)) * 0xfa);
    return merged < 1 ? 0 : merged;
}

Json::Value makeProximityLike() {
    Json::Value result(Json::objectValue);
    if (!isOosStorageMode()) {
        std::optional<Json::Value> cal = calData("devinfo.ps");
        if (!cal || !cal->isObject()) {
            return Json::Value(Json::objectValue);
        }
        const Json::Value& root = *cal;
        const Json::Value& offset = root["offset"];
        const Json::Value& threshold3cm = root["3cm_threshold"];
        const Json::Value& delta = root["delta"];
        const Json::Value& caliGoal = root["cali_goal"];
        const Json::Value& offsetLp = root["offset_lp"];
        const Json::Value& threshold1cm = root["1cm_threshold"];
        const Json::Value& delta1cm = root["1cm_delta"];
        const Json::Value& caliPara = root["cali_para"];
        const Json::Value& caliFlag = root["cali_flag"];
        const Json::Value& multOffset = root["mult_offset"];
        const Json::Value& multOffsetLp = root["mult_offset_lp"];

        const int rawMultOffset = analyzeNodeInt(multOffset);
        const int rawMultOffsetLp = analyzeNodeInt(multOffsetLp);
        int offsetValue = analyzeNodeInt(offset);
        int offsetLpValue = analyzeNodeInt(offsetLp);
        offsetValue = mergeProximityOffset(offsetValue);
        offsetLpValue = mergeProximityOffset(offsetLpValue);
        if ((rawMultOffset & 0x4000000) != 0) {
            offsetValue = mergeProximityOffset(rawMultOffset);
        }
        if ((rawMultOffsetLp & 0x4000000) != 0) {
            offsetLpValue = mergeProximityOffset(rawMultOffsetLp);
        }

        result["ps0_offset"] = std::to_string(offsetValue);
        addStringField(&result, "offset_ver", offset);
        result["ps0_distance_value"] = std::to_string(analyzeNodeInt(threshold3cm));
        addStringField(&result, "threshold_3cm_ver", threshold3cm);
        result["ps0_distance_delta"] = std::to_string(analyzeNodeInt(delta));
        addStringField(&result, "delta_ver", delta);
        result["ps0_value"] = std::to_string(analyzeNodeInt(caliGoal));
        addStringField(&result, "cali_goal_ver", caliGoal);
        result["ps0_offset_lp"] = std::to_string(offsetLpValue);
        addStringField(&result, "offset_lp_ver", offsetLp);
        result["ps0_distance_1cm_value"] =
                std::to_string(analyzeNodeInt(threshold1cm));
        addStringField(&result, "threshold_1cm_ver", threshold1cm);
        result["ps0_distance_1cm_delta"] =
                std::to_string(analyzeNodeInt(delta1cm));
        addStringField(&result, "delta_1cm_ver", delta1cm);
        result["ps0_cali_para"] = std::to_string(analyzeNodeInt(caliPara));
        addStringField(&result, "cali_para_ver", caliPara);
        result["ps0_cali_flag"] = std::to_string(analyzeNodeInt(caliFlag));
        addStringField(&result, "cali_flag_ver", caliFlag);
        return result;
    }

    std::optional<Json::Value> root =
            registryNode("stk2232_0_platform.ps.fac_cal");
    if (root && root->isObject()) {
        const Json::Value& offset2 = (*root)["offset2"];
        const Json::Value& threshold3cm = (*root)["3cm_thrd"];
        result["ps0_offset"] = std::to_string(analyzeNodeInt(offset2));
        addStringField(&result, "offset_ver", offset2);
        result["ps0_distance_delta"] =
                std::to_string(analyzeNodeInt(threshold3cm));
        addStringField(&result, "delta_ver", threshold3cm);
    } else {
        result["ps0_offset"] = "0";
        result["ps0_distance_delta"] = "0";
    }
    result["ps0_value"] = "0";
    result["cali_goal_ver"] = "0";
    return result;
}

Json::Value makeFakeProximityLike() {
    std::optional<Json::Value> cal = calData("devinfo.ps");
    if (!cal || !cal->isObject()) {
        return Json::Value(Json::objectValue);
    }
    const Json::Value& root = *cal;
    const Json::Value& offset1 = root["offset1"];
    const Json::Value& offset2 = root["offset2"];
    const Json::Value& threshold1 = root["1cm_thrd"];
    const Json::Value& threshold2 = root["3cm_thrd"];
    Json::Value result(Json::objectValue);
    result["offset_1"] = std::to_string(analyzeNodeInt(offset1));
    addStringField(&result, "offset_1_ver", offset1);
    result["offset_2"] = std::to_string(analyzeNodeInt(offset2));
    addStringField(&result, "offset_2_ver", offset2);
    result["threshold_1"] = std::to_string(analyzeNodeInt(threshold1));
    addStringField(&result, "threshold_1_ver", threshold1);
    result["threshold_2"] = std::to_string(analyzeNodeInt(threshold2));
    addStringField(&result, "threshold_2_ver", threshold2);
    return result;
}

Json::Value makeHallLike(const char* key,
                         const std::array<const char*, 3>& fields) {
    std::optional<Json::Value> cal = calData(key);
    if (!cal || !cal->isObject()) {
        return Json::Value(Json::objectValue);
    }
    Json::Value result(Json::objectValue);
    for (const char* field : fields) {
        const Json::Value& item = (*cal)[field];
        if (!item.isObject()) {
            continue;
        }
        result[field] = std::to_string(analyzeNodeInt(item));
        addStringField(&result, (std::string(field) + "_ver").c_str(), item);
    }
    return result;
}

Json::Value makeHingeLike(int32_t sensorType) {
    const char* key =
            sensorType == kTypeHingeB ? "devinfo.fold_state"
                                      : "devinfo.hinge_detect";
    std::optional<Json::Value> cal = calData(key);
    if (!cal || !cal->isObject()) {
        return Json::Value(Json::objectValue);
    }
    const Json::Value& root = *cal;
    const Json::Value& para0 = root["cali_para0"];
    const Json::Value& para1 = root["cali_para1"];
    const Json::Value& para2 = root["cali_para2"];
    Json::Value result(Json::objectValue);
    result["cali_para1"] = std::to_string(analyzeNodeInt(para0));
    addStringField(&result, "cali_para1_ver", para0);
    result["cali_para2"] = std::to_string(analyzeNodeInt(para1));
    addStringField(&result, "cali_para2_ver", para1);
    result["cali_para3"] = std::to_string(analyzeNodeInt(para2));
    addStringField(&result, "cali_para3_ver", para2);
    return result;
}

Json::Value makeLightLike(const char* key) {
    std::optional<Json::Value> cal = calData(key);
    Json::Value result(Json::objectValue);
    if (!cal || !addLightScaleFields(&result, *cal)) {
        return Json::Value(Json::objectValue);
    }
    return result;
}

void syncCalicoeBackup(const Json::Value& cal) {
    const Json::Value& scale = cal["scale"];
    if (!scale.isObject() || !scale["data"].isString()) {
        return;
    }
    const int value = std::atoi(scale["data"].asString().c_str());
    if (value == 0) {
        return;
    }
    const std::string current =
            readNodeText(std::string(kEngineerDir) + "/calicoe", 0x20);
    if (current.empty()) {
        return;
    }
    const int previous = std::atoi(current.c_str());
    if (previous == value) {
        return;
    }
    writeNodeText(std::string(kEngineerDir) + "/calicoe_bak",
                  std::to_string(previous));
}

Json::Value makeScreenLightLike() {
    Json::Value result(Json::objectValue);
    if (!isOosStorageMode()) {
        uint32_t values[4] = {};
        if (readInterface(kReservePath,
                          reserveOffset(kUfsScreenLight, kEmmcScreenLight),
                          values, sizeof(values)) > 0) {
            static constexpr std::array<const char*, 4> kKeys = {
                    "R_MAX", "G_MAX", "B_MAX", "W_MAX",
            };
            for (size_t i = 0; i < kKeys.size(); ++i) {
                result[kKeys[i]] = std::to_string(values[i]);
            }
        }
    } else {
        static constexpr std::array<std::pair<const char*, const char*>, 4>
                kValues = {{
                        {"red_max_lux", "R_MAX"},
                        {"green_max_lux", "G_MAX"},
                        {"blue_max_lux", "B_MAX"},
                        {"white_max_lux", "W_MAX"},
                }};
        for (const auto& value : kValues) {
            const std::string text =
                    readNodeText(std::string(kEngineerDir) + "/" + value.first,
                                 0x20);
            if (!text.empty()) {
                result[value.second] = text;
            }
        }
    }
    const std::string rowCoe = readNodeText(kAlsRowCoeNode, 0x20);
    if (!rowCoe.empty()) {
        result["row_coe"] = rowCoe;
    }
    std::optional<Json::Value> cal = calData("devinfo.als");
    if (cal && cal->isObject()) {
        addLightScaleFields(&result, *cal);
    }
    if (isOosStorageMode()) {
        std::optional<Json::Value> oosCal = calData("oplus_devinfo.als");
        if (oosCal && oosCal->isObject()) {
            addLightScaleFields(&result, *oosCal);
            syncCalicoeBackup(*oosCal);
        }
    }
    return result;
}

Json::Value makePadLightLike(off_t ufsOffset, off_t emmcOffset,
                             const char* registryKey, bool useSecondCoe) {
    Json::Value result(Json::objectValue);
    std::optional<Json::Value> cal = registryNode(registryKey);
    if (cal && cal->isObject()) {
        addLightScaleFields(&result, *cal);
    }

    uint32_t values[4] = {};
    if (readInterface(kReservePath, reserveOffset(ufsOffset, emmcOffset), values,
                      sizeof(values)) > 0) {
        static constexpr std::array<const char*, 4> kKeys = {
                "R_MAX", "G_MAX", "B_MAX", "W_MAX",
        };
        for (size_t i = 0; i < kKeys.size(); ++i) {
            result[kKeys[i]] = std::to_string(values[i]);
        }
    }

    const std::string rowCoe = readNodeText(kPadAlsRowCoeNode, 0x20);
    if (!rowCoe.empty()) {
        std::istringstream parser(rowCoe);
        int first = 0;
        int second = 0;
        parser >> first >> second;
        result["row_coe"] = std::to_string(useSecondCoe ? second : first);
    }
    return result;
}

Json::Value makeScreenSnLike() {
    std::array<char, 17> primary = {};
    std::array<char, 17> main = {};
    const int primaryCount = readInterface(
            kReservePath, reserveOffset(kUfsScreenSn, kEmmcScreenSn),
            primary.data(), 16);
    const int mainCount = readInterface(
            kReservePath, reserveOffset(kUfsMainScreenSn, kEmmcMainScreenSn),
            main.data(), 16);
    if ((primaryCount & mainCount) == -1) {
        return Json::Value(Json::objectValue);
    }
    Json::Value result(Json::objectValue);
    result["sreen_sn"] = trimNul(std::string(primary.data()));
    result["main_sreen_sn"] = trimNul(std::string(main.data()));
    return result;
}

Json::Value makePressureLike() {
    uint32_t value = 0xffffffff;
    if (readInterface(kReservePath,
                      reserveOffset(kUfsPressure, kEmmcPressure),
                      &value, sizeof(value)) < 1) {
        value = 0xffffffff;
    }
    Json::Value result(Json::objectValue);
    result["cali_offset"] = std::to_string(value);
    return result;
}

Json::Value makeFreeFallLike() {
    uint16_t value = 0;
    if (readInterface(kReservePath,
                      reserveOffset(kUfsFreeFall, kEmmcFreeFall),
                      &value, sizeof(value)) < 1) {
        return Json::Value(Json::objectValue);
    }
    Json::Value result(Json::objectValue);
    result["count"] = std::to_string(value);
    result["count1"] = std::to_string(value & 0xff);
    result["count2"] = std::to_string((value >> 8) & 0xff);
    return result;
}

Json::Value makeConsumerIrLike() {
    uint32_t value = 0xffffffff;
    if (readInterface(kReservePath,
                      reserveOffset(kUfsConsumerIr, kEmmcConsumerIr),
                      &value, sizeof(value)) < 1) {
        value = 0xffffffff;
    }
    Json::Value result(Json::objectValue);
    result["is_tested"] = std::to_string(value);
    return result;
}

Json::Value makeSoftMagLike() {
    // The stock node is a packed 180-byte table of 8-byte fields: the first
    // nine are M1 params, the next nine are M2 params.
    std::string text;
    if (readTextFile(kMagSoftParaNode, &text) && !text.empty()) {
        Json::Value root;
        if (parseJson(text, &root) && root.isObject()) {
            return root;
        }
    }

    std::array<char, 0xb4> buffer = {};
    const int count =
            readInterface(kMagSoftParaNode, 0, buffer.data(), buffer.size());
    Json::Value result(Json::objectValue);
    if (count >= 1) {
        auto slot = [&buffer](size_t index) {
            const size_t offset = index * 8;
            if (offset >= buffer.size()) {
                return std::string();
            }
            const size_t available =
                    std::min<size_t>(8, buffer.size() - offset);
            return trimNul(std::string(buffer.data() + offset, available));
        };
        result["M1_module"] = "First";
        for (int i = 0; i < 9; ++i) {
            result["M1_param" + std::to_string(i + 1)] =
                    slot(static_cast<size_t>(i));
        }
        result["M2_module"] = "second";
        for (int i = 0; i < 9; ++i) {
            result["M2_param" + std::to_string(i + 1)] =
                    slot(static_cast<size_t>(9 + i));
        }
        return result;
    }

    if (!text.empty()) {
        result["data"] = trimNul(text);
    }
    return result;
}

Json::Value makeStructureProximityLike() {
    std::optional<Json::Value> cal = calData("devinfo.structure_ps");
    if (!cal || !cal->isObject()) {
        return Json::Value(Json::objectValue);
    }
    const Json::Value& offset = (*cal)["offset"];
    int offsetValue = analyzeNodeInt(offset);
    Json::Value result(Json::objectValue);
    uint32_t far = 0;
    if (readInterface(kReservePath, kUfsStructureProximity, &far,
                      sizeof(far)) > 0) {
        result["ps_far"] = std::to_string(far);
    }
    result["ps_offset"] = std::to_string(offsetValue);
    addStringField(&result, "ps_offset_ver", offset);
    return result;
}

bool buildCalibration(int32_t sensorType, Json::Value* result) {
    if (result == nullptr) {
        return false;
    }
    bool alwaysSucceed = false;
    switch (sensorType) {
        case kTypeGsensor:
            *result = makeGsensorLike("devinfo.gsensor");
            break;
        case kTypeGsensorSub:
            *result = makeGsensorLike("devinfo.gsensor_sub");
            break;
        case kTypeGyro:
            *result = makeGsensorLike("devinfo.gyro");
            break;
        case kTypeGyroSub:
            *result = makeGsensorLike("devinfo.gyro_sub");
            break;
        case kTypeMag:
            *result = makeSoftMagLike();
            break;
        case kTypeLight:
            *result = makeLightLike("devinfo.als");
            break;
        case kTypeBackLight:
        case 0x10062:
            *result = makeLightLike("devinfo.als_rear");
            break;
        case kTypePressure:
            *result = makePressureLike();
            break;
        case kTypeProximity:
            *result = makeProximityLike();
            break;
        case kTypeFakeProximityA:
        case kTypeFakeProximityB:
        case 0x1005a:
            *result = makeFakeProximityLike();
            break;
        case kTypeHall:
            *result = makeHallLike("devinfo.hall_effect",
                                   {"20deg_data", "0deg_data", "180deg_data"});
            break;
        case kTypeAngleHall:
            *result = makeHallLike("devinfo.angle_hall",
                                   {"60deg_data", "150deg_data", "180deg_data"});
            break;
        case kTypeStructureProximity:
            *result = makeStructureProximityLike();
            break;
        case kTypeFreeFall:
            *result = makeFreeFallLike();
            break;
        case kTypeScreenLightA:
        case kTypeScreenLightB:
        case 0x1005b:
            *result = makeScreenLightLike();
            alwaysSucceed = true;
            break;
        case kTypePadLight:
            *result = makePadLightLike(kUfsPadLight, kEmmcPadLight,
                                       "pad_als_platform.fac_cal", false);
            alwaysSucceed = true;
            break;
        case kTypeVicePadLight:
            *result = makePadLightLike(kUfsVicePadLight, kEmmcVicePadLight,
                                       "vice_pad_als_platform.fac_cal", true);
            alwaysSucceed = true;
            break;
        case kTypeHingeA:
        case kTypeHingeB:
            *result = makeHingeLike(sensorType);
            break;
        case kTypeRgbLeak: {
            Json::Value leak(Json::objectValue);
            addRgbLeakFields(false, &leak);
            *result = leak;
            alwaysSucceed = true;
            break;
        }
        case kTypeRgbA:
        case kTypeRgbB:
        case kTypeRgbC:
        case kTypeRgbD:
        case kTypeRgb:
        case kTypeRgbGain:
        case kTypeRgbPad:
            *result = makeRgbLike(sensorType);
            break;
        case kTypeSpectral:
        case kTypeSpectrum6000k:
            *result = makeSpectralLike(sensorType);
            break;
        case kTypeScreenSn:
            *result = makeScreenSnLike();
            break;
        case kTypeConsumerIr:
            *result = makeConsumerIrLike();
            break;
        default:
            return false;
    }
    if (alwaysSucceed) {
        return true;
    }
    return !result->isNull() && (!result->isObject() || !result->empty());
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------

int setScreenLight(const uint8_t* data, size_t length) {
    if (length != 0x10) {
        return -1;
    }
    if (!isOosStorageMode()) {
        return writeInterface(
                kReservePath, reserveOffset(kUfsScreenLight, kEmmcScreenLight),
                data, length);
    }

    std::array<int32_t, 4> values = {};
    std::memcpy(values.data(), data, sizeof(values));
    static constexpr std::array<const char*, 4> kNodes = {
            "red_max_lux",
            "green_max_lux",
            "blue_max_lux",
            "white_max_lux",
    };
    int result = -1;
    for (size_t i = 0; i < kNodes.size(); ++i) {
        result = writeNodeText(std::string(kEngineerDir) + "/" + kNodes[i],
                               std::to_string(values[i]));
        if (result <= 0) {
            return result;
        }
    }
    return result;
}

Json::Value makeZeroRgbChannels() {
    Json::Value group(Json::objectValue);
    for (const char* channel : kChannels) {
        group[channel] = "0";
    }
    return group;
}

Json::Value makeDefaultRgbComp(bool allGroups) {
    Json::Value comp(Json::objectValue);
    if (allGroups) {
        comp["R_comp"] = makeZeroRgbChannels();
        comp["G_comp"] = makeZeroRgbChannels();
        comp["B_comp"] = makeZeroRgbChannels();
    }
    comp["C_comp"] = makeZeroRgbChannels();
    return comp;
}

Json::Value makeDefaultSpectrumTestEntry() {
    Json::Value entry(Json::objectValue);
    static constexpr std::array<const char*, 16> kFields = {
            "xl_data",  "z_data",  "y_data",  "f1_data", "f2_data",
            "f3_data",  "f4_data", "f5_data", "f6_data", "f7_data",
            "f8_data",  "nir_data", "vis_data", "lux_mean", "cct_mean",
            "gain",
    };
    for (const char* field : kFields) {
        entry[field] = "0";
    }
    return entry;
}

int setWiseRgb(int32_t sensorType, const uint8_t* data, int32_t type,
               size_t length) {
    const bool pad = sensorType == kTypeRgbC;
    const char* key = pad ? "pad_wise_rgb_comp_scale" : "wise_rgb_comp_scale";
    const char* brightnessPath =
            "/sys/class/backlight/panel0-backlight/brightness";
    if (!pad && sensorType == kTypeRgbB && type == 1001) {
        brightnessPath = "/sys/class/backlight/panel1-backlight/brightness";
    }

    if (type >= 0 && type <= 8) {
        if (length != 0x40) {
            return 0;
        }
        std::array<int32_t, 16> values = {};
        std::memcpy(values.data(), data, sizeof(values));

        std::optional<Json::Value> parsed = registryNode(key);
        Json::Value node = parsed && parsed->isObject()
                                   ? *parsed
                                   : Json::Value(Json::objectValue);
        if (!parsed || !parsed->isObject()) {
            node["ver"] = "0";
            for (int i = 0; i <= 8; ++i) {
                node["comp" + std::to_string(i)] =
                        makeDefaultRgbComp(i <= 2);
            }
        }

        const std::string compName = "comp" + std::to_string(type);
        if (type == 0) {
            const int version = std::atoi(jsonVersion(node).c_str());
            node["ver"] = std::to_string(version + 1);
        }
        if (type <= 2) {
            for (size_t group = 0; group < kComponentGroups.size(); ++group) {
                for (size_t channel = 0; channel < kChannels.size(); ++channel) {
                    node[compName][kComponentGroups[group]][kChannels[channel]] =
                            std::to_string(values[group * 4 + channel]);
                }
            }
        } else {
            for (size_t channel = 0; channel < kChannels.size(); ++channel) {
                node[compName]["C_comp"][kChannels[channel]] =
                        std::to_string(values[12 + channel]);
            }
        }

        Json::Value wrapper(Json::objectValue);
        wrapper[key] = node;
        if (!writeJsonFile(std::string(kRegistryDir) + "/" + key, wrapper)) {
            return 0;
        }
        return 0x40;
    }

    if (type == 1000 || type == 1001) {
        if (length != 4) {
            return 0;
        }
        int32_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        if (writeNodeText(brightnessPath, std::to_string(value)) < 0) {
            return 0;
        }
        return 4;
    }
    return 0;
}

int setSpectrumTest(const uint8_t* data, int32_t type, size_t length) {
    if (type < 0 || type > 4 || length != 0x40) {
        return 0;
    }
    static constexpr std::array<const char*, 5> kNames = {
            "test_5000k_data",
            "test_6500k_data",
            "test_3000k_data",
            "test_4500k_data",
            "test_6000k_data",
    };
    std::array<int32_t, 16> values = {};
    std::memcpy(values.data(), data, sizeof(values));

    const std::string path = std::string(kCameraDir) + "/spectrum_test_data";
    std::optional<Json::Value> parsed = parseJsonFile(path);
    Json::Value root = parsed && parsed->isObject()
                               ? *parsed
                               : Json::Value(Json::objectValue);
    Json::Value& testData = root["spectrum_test_data"];
    if (!testData.isObject()) {
        testData = Json::Value(Json::objectValue);
        for (const char* name : kNames) {
            testData[name] = makeDefaultSpectrumTestEntry();
        }
        testData["ver"] = "0";
    }

    const int version = std::atoi(jsonVersion(testData).c_str());
    testData["ver"] = std::to_string(version + 1);

    Json::Value& entry = testData[kNames[type]];
    entry = Json::Value(Json::objectValue);
    static constexpr std::array<std::pair<int, const char*>, 16> kFields = {{
            {0, "xl_data"},
            {1, "y_data"},
            {2, "z_data"},
            {3, "f1_data"},
            {4, "f2_data"},
            {5, "f3_data"},
            {6, "f4_data"},
            {7, "f5_data"},
            {8, "f6_data"},
            {9, "f7_data"},
            {10, "f8_data"},
            {11, "nir_data"},
            {12, "vis_data"},
            {13, "lux_mean"},
            {14, "cct_mean"},
            {15, "gain"},
    }};
    for (const auto& field : kFields) {
        entry[field.second] = std::to_string(values[field.first]);
    }
    return writeJsonFile(path, root) ? 0x40 : 0;
}

int saveSpectrumSnNum(const uint8_t* data, int32_t type, size_t length) {
    if (length == 0) {
        return -1;
    }
    const size_t copyLength = std::min<size_t>(length, 10);
    const std::string sn =
            trimNul(std::string(reinterpret_cast<const char*>(data), copyLength));

    std::optional<Json::Value> parsed = registryFile("spectrum_sn.json", 1);
    Json::Value root = parsed && parsed->isObject()
                               ? *parsed
                               : Json::Value(Json::objectValue);
    if (!parsed || !parsed->isObject()) {
        root["spectrum_sn"] = "0";
    }
    root["spectrum_sn"] = sn;

    std::string path;
    if (type == 0) {
        path = std::string(kCameraDir) + "/spectrum_sn.json";
    } else if (type == 1) {
        path = std::string(kSpectrumDir) + "/spectrum_sn.json";
    } else {
        return static_cast<int>(length);
    }
    return writeJsonFile(path, root) ? static_cast<int>(length) : -1;
}

int copyFile(const std::string& source, const std::string& destination) {
    std::string text;
    if (!readTextFile(source, &text)) {
        return -1;
    }
    return writeTextFile(destination, text) ? 0 : -1;
}

int copySpectrumCmDataToCamera() {
    std::optional<Json::Value> root = registryFile("spectrum_sn.json", 1);
    if (!root || !root->isObject()) {
        return -1;
    }
    std::string sn = jsonString(*root, "spectrum_sn");
    if (sn.size() > 10) {
        sn.resize(10);
    }
    const std::string fileName = "cm_tcs3449_" + sn + ".txt";
    return copyFile(std::string(kSpectrumDir) + "/" + fileName,
                    std::string(kCameraDir) + "/" + fileName);
}

int copyNormalCmDataToCamera() {
    std::optional<Json::Value> root = registryFile("spectrum_sn.json", 1);
    if (!root || !root->isObject()) {
        return -1;
    }
    std::string sn = jsonString(*root, "spectrum_sn");
    if (sn.size() > 10) {
        sn.resize(10);
    }

    std::string cctName = calFileName("devinfo.cct_rear_name");
    if (cctName.size() > 7) {
        cctName.resize(7);
    }
    std::string source;
    if (!cctName.empty()) {
        source = std::string("/odm/etc/camera/") + cctName + "_golden";
    }
    std::string text;
    if (source.empty() || !readTextFile(source, &text)) {
        source = "/odm/etc/camera/tcs3449_golden";
        if (!readTextFile(source, &text)) {
            return -1;
        }
    }
    return writeTextFile(std::string(kCameraDir) + "/cm_tc" + sn + ".txt", text)
                   ? 0
                   : -1;
}

int setSpectrumWavelength(const uint8_t* data, size_t length) {
    if (length != 10) {
        return -1;
    }
    const std::string sn(reinterpret_cast<const char*>(data), length);
    const std::string input =
            std::string(kSpectrumDir) + "/OPCloud_data_" + sn + ".json";
    const std::string output =
            std::string(kSpectrumDir) + "/cm_tcs3449_" + sn + ".txt";
    const std::string xyz =
            std::string(kSpectrumDir) + "/xyz_l1_tcs3449.json";
    const std::string gain =
            std::string(kSpectrumDir) + "/gain_l1_tcs3449.json";

    void* handle = dlopen("/odm/lib64/libams.so", RTLD_LAZY);
    if (handle == nullptr) {
        std::string cctName = calFileName("devinfo.cct_rear_name");
        if (cctName.size() > 7) {
            cctName.resize(7);
        }
        if (!cctName.empty()) {
            const std::string library = "/odm/lib64/lib" + cctName + ".so";
            handle = dlopen(library.c_str(), RTLD_LAZY);
        }
        if (handle == nullptr) {
            return -1;
        }
    }

    using WavelengthCalImpl = int (*)(const char*, const char*, const char*,
                                      const char*, int, int, int);
    auto impl = reinterpret_cast<WavelengthCalImpl>(
            dlsym(handle, "wavelength_cal_impl"));
    if (impl == nullptr) {
        dlclose(handle);
        return -1;
    }
    int result = impl(input.c_str(), output.c_str(), xyz.c_str(), gain.c_str(),
                      0x15e, 0x28b, 1);
    if (result == 0) {
        if (!registryFile("xyz_l1_tcs3449.json", 1).has_value() ||
            !registryFile("gain_l1_tcs3449.json", 1).has_value()) {
            LOG(WARNING) << "wavelength calibration produced no xyz/gain data";
        }
        result = copySpectrumCmDataToCamera() < 0 ? -3 : 10;
    }
    const int closeResult = dlclose(handle);
    if (closeResult != 0 && result == 10) {
        result = -2;
    }
    return result;
}

bool saveScreenSn(const uint8_t* data, size_t length, int32_t type) {
    if (length < 1 || length > 16) {
        return false;
    }
    off_t offset = reserveOffset(kUfsScreenSn, kEmmcScreenSn);
    if (type == 1) {
        offset += 0x40;
    } else if (type != 0) {
        return false;
    }
    return writeInterface(kReservePath, offset, data, length) != -1;
}

}  // namespace

SensorFeature::SensorFeature() {
    // Match the stock constructor: it evaluates the storage mode and
    // publishes the current screen-light maxima to the ALS proc nodes.
    if (isOosStorageMode()) {
        LOG(INFO) << "OPLUS sensor calibration uses OOS storage mode";
    }
    const Json::Value screenLight = makeScreenLightLike();
    static constexpr std::array<std::pair<const char*, const char*>, 4> kNodes = {{
            {"R_MAX", "red_max_lux"},
            {"G_MAX", "green_max_lux"},
            {"B_MAX", "blue_max_lux"},
            {"W_MAX", "white_max_lux"},
    }};
    for (const auto& node : kNodes) {
        const Json::Value& value = screenLight[node.first];
        if (value.isString()) {
            writeNodeText(std::string("/proc/sensor/als_cali/") + node.second,
                          value.asString());
        }
    }
}

ndk::ScopedAStatus SensorFeature::calibrateSensor(int32_t sensor_type, int32_t para,
                                                  bool* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    LOG(DEBUG) << "calibrateSensor type=" << sensor_type << " para=" << para;

    if (sensor_type == kTypePressure) {
        const int32_t value = para == 0 ? 1 : para;
        writeNodeText(kPressureNode, std::to_string(value));
        const int written =
                writeInterface(kReservePath,
                               reserveOffset(kUfsPressure, kEmmcPressure),
                               &value, sizeof(value));
        *_aidl_return = written != -1;
        return ndk::ScopedAStatus::ok();
    }
    if (sensor_type == kTypeConsumerIr) {
        const int written =
                writeInterface(kReservePath,
                               reserveOffset(kUfsConsumerIr, kEmmcConsumerIr),
                               &para, sizeof(para));
        *_aidl_return = written != -1;
        return ndk::ScopedAStatus::ok();
    }

    switch (sensor_type) {
        case kTypeGsensor:
        case kTypeMag:
        case kTypeGyro:
        case kTypeLight:
        case kTypeProximity:
        case 0x13:
        case 0x10048:
        case 0x1005a:
        case 0x1005b:
        case 0x10062:
        case 0x10065:
        case 0x10067:
        case 0x10068:
        case 0x10069:
        case 0x1006a:
        case kTypeRgbA:
        case kTypeHall:
        case kTypeRgb:
        case kTypeBackLight:
        case kTypeSpectral:
        case kTypeRgbLeak:
        case kTypeScreenLightB:
            *_aidl_return = false;
            return ndk::ScopedAStatus::ok();
        default:
            *_aidl_return = false;
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
}

ndk::ScopedAStatus SensorFeature::getSensorCalibrationData(
        int32_t sensor_type, std::string* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    LOG(DEBUG) << "getSensorCalibrationData type=" << sensor_type;

    Json::Value result;
    if (!buildCalibration(sensor_type, &result)) {
        _aidl_return->clear();
        return ndk::ScopedAStatus::ok();
    }
    std::string text = result.toStyledString();
    if (text.size() > 0xfff) {
        text.resize(0xfff);
    }
    *_aidl_return = std::move(text);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SensorFeature::getSensorConfig(std::string* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::string text;
    if (!readTextFile(kConfigPath, &text) || text.empty()) {
        _aidl_return->clear();
        return ndk::ScopedAStatus::ok();
    }
    Json::Value root;
    if (!parseJson(text, &root) || !root.isObject()) {
        _aidl_return->clear();
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = root.toStyledString();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SensorFeature::getSensorRawData(int32_t sensor_type,
                                                   float* _aidl_return) {
    (void)sensor_type;
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    *_aidl_return = 0.0f;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SensorFeature::saveCalibrationData(
        int32_t sensor_type, int32_t type, const std::vector<uint8_t>& cali_data,
        int32_t* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (cali_data.empty()) {
        *_aidl_return = -1;
        return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    LOG(DEBUG) << "saveCalibrationData type=" << sensor_type
               << " subtype=" << type << " length=" << cali_data.size();

    const uint8_t* data = cali_data.data();
    const size_t length = cali_data.size();
    int32_t result = -1;

    switch (sensor_type) {
        case 0x1005b:
        case kTypeScreenLightA:
        case kTypeScreenLightB:
            result = setScreenLight(data, length);
            break;
        case kTypeRgbB:
        case kTypeRgbC:
        case kTypeRgbLeak:
            result = setWiseRgb(sensor_type, data, type, length);
            break;
        case kTypeSpectral:
            result = setSpectrumTest(data, type, length);
            break;
        case kTypePadLight:
            if (length == 0x10) {
                result = writeInterface(
                        kReservePath, reserveOffset(kUfsPadLight, kEmmcPadLight),
                        data, length);
            }
            break;
        case kTypeVicePadLight:
            if (length == 0x10) {
                result = writeInterface(
                        kReservePath,
                        reserveOffset(kUfsVicePadLight, kEmmcVicePadLight), data,
                        length);
            }
            break;
        case kTypeScreenSn:
            result = saveScreenSn(data, length, type) ? 1 : 0;
            break;
        case kTypeStructureProximity:
            if (length != 4) {
                result = writeInterface(kReservePath, kUfsStructureProximity,
                                        data, length);
            }
            break;
        case kSpectrumTestType:
            result = copyNormalCmDataToCamera();
            break;
        case kSpectrumSnType:
            result = saveSpectrumSnNum(data, type, length);
            break;
        case kSpectrumWavelengthType:
            result = setSpectrumWavelength(data, length);
            break;
        default:
            result = -1;
            break;
    }
    *_aidl_return = result;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SensorFeature::sensorRawDataEnableDisable(
        int32_t sensor_type, int32_t para, bool* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    (void)sensor_type;
    (void)para;
    *_aidl_return = false;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SensorFeature::sensorReadNodeFile(
        const std::string& node_name, int32_t length, std::string* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    (void)length;
    for (const SensorNode& node : kNodeTable) {
        if (node_name != node.name) {
            continue;
        }
        const std::string path = std::string(node.dir) + "/" + node.name;
        *_aidl_return = readNodeText(path, node.read_size);
        return ndk::ScopedAStatus::ok();
    }
    _aidl_return->clear();
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus SensorFeature::sensorWriteNodeFile(
        const std::string& node_name, const std::string& info,
        int32_t* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (info.size() > kMaxNodeValueLength) {
        *_aidl_return = -1;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    for (const SensorNode& node : kNodeTable) {
        if (node_name != node.name) {
            continue;
        }
        const std::string path = std::string(node.dir) + "/" + node.name;
        *_aidl_return = writeNodeText(path, info);
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = -2;
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus SensorFeature::setPhoneMode(int8_t type, int8_t on,
                                               int32_t* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const int kmsg = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    const int sysrq = open("/proc/sysrq-trigger", O_WRONLY | O_CLOEXEC);
    if (kmsg < 0 || sysrq < 0) {
        LOG(WARNING) << "setPhoneMode: open dump node(" << kmsg << ") or dmesg node("
                     << sysrq << ") failed";
    }
    if (type == 1 && on == 1 && kmsg >= 0 && sysrq >= 0) {
        static constexpr char kMessage[] =
                "oplus_sensor triggered dump by setPhoneMode aidl.";
        (void)write(kmsg, kMessage, sizeof(kMessage));
        (void)write(sysrq, "c", 1);
    }
    if (kmsg >= 0) {
        close(kmsg);
    }
    if (sysrq >= 0) {
        close(sysrq);
    }
    LOG(DEBUG) << "setPhoneMode type=" << static_cast<int>(type)
               << " on=" << static_cast<int>(on);
    *_aidl_return = 0;
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::vendor::oplus::hardware::oplusSensor
