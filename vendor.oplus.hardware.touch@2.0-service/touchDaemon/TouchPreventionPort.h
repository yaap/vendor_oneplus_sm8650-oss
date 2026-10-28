/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal userspace compatibility layer for the OPLUS prevention algorithm.
 * The algorithm body is kept in TouchPreventionCore.h so it stays close to
 * the stock implementation; these definitions provide only the kernel-side
 * types and helpers that the algorithm expects.
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <pthread.h>
#include <unistd.h>

namespace oplus_prevention {

using std::memcpy;
using std::memset;
using std::snprintf;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define GFP_KERNEL 0
#define EINVAL 22
#define PAGE_SIZE 4096
#define TOUCH_BIT_CHECK 0x3FF
#define BIT(n) (1UL << (n))

#define abs(a) ((a) < 0 ? -(a) : (a))

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - __builtin_offsetof(type, member)))

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_add_tail(struct list_head *entry, struct list_head *head)
{
    entry->prev = head->prev;
    entry->next = head;
    head->prev->next = entry;
    head->prev = entry;
}

static inline void list_del(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = nullptr;
    entry->prev = nullptr;
}

#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define list_for_each_safe(pos, n, head) \
    for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); \
         (pos) = (n), (n) = (pos)->next)

struct mutex {
    pthread_mutex_t lock;
};

static inline void mutex_init(struct mutex *m)
{
    pthread_mutex_init(&m->lock, nullptr);
}

static inline void mutex_lock(struct mutex *m)
{
    pthread_mutex_lock(&m->lock);
}

static inline void mutex_unlock(struct mutex *m)
{
    pthread_mutex_unlock(&m->lock);
}

// The make-up FIFO only ever stores one byte point ids.  A fixed ring keeps
// the structure POD so it can live in a calloc'd grip_info.
struct kfifo {
    uint8_t data[256];
    unsigned int in;
    unsigned int out;
};

static inline int kfifo_alloc(struct kfifo *fifo, unsigned int, int)
{
    fifo->in = 0;
    fifo->out = 0;
    return 0;
}

static inline void kfifo_reset(struct kfifo *fifo)
{
    fifo->in = 0;
    fifo->out = 0;
}

template <typename T>
static inline int kfifo_get(struct kfifo *fifo, T *value)
{
    if (fifo->in == fifo->out) {
        return 0;
    }
    *value = static_cast<T>(fifo->data[fifo->out % sizeof(fifo->data)]);
    fifo->out++;
    return 1;
}

template <typename T>
static inline int kfifo_put(struct kfifo *fifo, T value)
{
    if ((fifo->in - fifo->out) >= sizeof(fifo->data)) {
        return 0;
    }
    fifo->data[fifo->in % sizeof(fifo->data)] = static_cast<uint8_t>(value);
    fifo->in++;
    return 1;
}

enum hrtimer_restart {
    HRTIMER_NORESTART = 0,
    HRTIMER_RESTART = 1,
};

#define HRTIMER_MODE_REL 0
#define CLOCK_MONOTONIC 1

struct hrtimer {
    enum hrtimer_restart (*function)(struct hrtimer *);
    int64_t expires_ns;
    bool active;
};

static inline void hrtimer_init(struct hrtimer *timer, int, int)
{
    timer->function = nullptr;
    timer->expires_ns = 0;
    timer->active = false;
}

static inline void hrtimer_start(struct hrtimer *timer, int64_t expires, int)
{
    timer->expires_ns = expires;
    timer->active = true;
}

static inline int hrtimer_active(const struct hrtimer *timer)
{
    return timer->active ? 1 : 0;
}

static inline int hrtimer_cancel(struct hrtimer *timer)
{
    timer->active = false;
    return 0;
}

struct work_struct {
    void (*func)(struct work_struct *);
};

struct workqueue_struct {
    int unused;
};

static inline void INIT_WORK(struct work_struct *work, void (*func)(struct work_struct *))
{
    work->func = func;
}

static inline int queue_work(struct workqueue_struct *, struct work_struct *)
{
    return 0;
}

static inline struct workqueue_struct *create_singlethread_workqueue(const char *)
{
    static struct workqueue_struct queue;
    return &queue;
}

static inline void destroy_workqueue(struct workqueue_struct *)
{
}

typedef int64_t ktime_t;

static inline ktime_t ktime_get()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
}

static inline int64_t ktime_to_ms(ktime_t time)
{
    return time / 1000000;
}

static inline ktime_t ktime_set(long sec, long nsec)
{
    return static_cast<ktime_t>(sec) * 1000000000LL + nsec;
}

static inline void usleep_range(unsigned long min_us, unsigned long)
{
    usleep(static_cast<useconds_t>(min_us));
}

static inline void msleep(unsigned int ms)
{
    usleep(static_cast<useconds_t>(ms) * 1000);
}

struct KzAllocResult {
    void *ptr;
    template <typename T>
    operator T *() const
    {
        return static_cast<T *>(ptr);
    }
};

static inline KzAllocResult kzalloc(size_t size, int)
{
    return KzAllocResult{std::calloc(1, size)};
}

static inline void kfree(void *ptr)
{
    std::free(ptr);
}

typedef enum {
    AREA_NOTOUCH,
    AREA_EDGE,
    AREA_CRITICAL,
    AREA_NORMAL,
    AREA_CORNER,
} touch_area;

struct point_info {
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint8_t width_major;
    uint8_t touch_major;
    uint8_t status;
    uint8_t tx_press;
    uint8_t rx_press;
    uint8_t tx_er;
    uint8_t rx_er;
    touch_area type;
};

enum touch_direction {
    VERTICAL_SCREEN,
    LANDSCAPE_SCREEN_90,
    LANDSCAPE_SCREEN_270,
    VERTICAL_SCREEN_180,
};

struct device_node {
    int unused;
};

struct device {
    struct device_node *of_node;
    void *driver_data;
};

static inline void *dev_get_drvdata(struct device *dev)
{
    return dev != nullptr ? dev->driver_data : nullptr;
}

struct fw_grip_operations;
struct touchpanel_data;
struct proc_dir_entry;
struct seq_file;

extern std::map<std::string, std::vector<uint32_t>> *g_config;

template <typename T>
static inline int of_property_read_u32_array(struct device_node *, const char *name, T *out,
                                             size_t count)
{
    if (g_config == nullptr) {
        return -EINVAL;
    }
    auto it = g_config->find(name);
    if (it == g_config->end()) {
        return -EINVAL;
    }
    for (size_t i = 0; i < count; ++i) {
        out[i] = i < it->second.size() ? static_cast<T>(it->second[i]) : static_cast<T>(0);
    }
    return 0;
}

static inline bool of_property_read_bool(struct device_node *, const char *name)
{
    return g_config != nullptr && g_config->find(name) != g_config->end();
}

static inline int of_property_count_elems_of_size(struct device_node *, const char *name, int)
{
    if (g_config == nullptr) {
        return -EINVAL;
    }
    auto it = g_config->find(name);
    if (it == g_config->end()) {
        return -EINVAL;
    }
    return static_cast<int>(it->second.size());
}

#include "TouchPreventionStructs.h"

struct MakeupState {
    bool pending[TOUCH_MAX_NUM];
    int64_t deadline_ms[TOUCH_MAX_NUM];
};

extern MakeupState g_makeup_state;

void kernel_grip_release(struct kernel_grip_info *grip_info);
void run_makeup(struct kernel_grip_info *grip_info, int slot,
                const std::function<void(int, uint16_t, uint16_t)> &on_point,
                const std::function<void(int)> &on_release);

struct monitor_data {
    int unused;
};

struct algo_point_buf {
    struct {
        int x;
    } kal_x_last;
    int touch_time;
    int status;
};

struct algo_info {
    struct algo_point_buf *point_buf;
};

struct ts_ops {
    int unused;
};

struct input_dev {
    int unused;
};

// The stock prevention core only uses these fields from touchpanel_data.
// The timer/reporting helpers that used the rest are replaced by this port.
struct touchpanel_data {
    struct input_dev *input_dev;
    struct mutex report_mutex;
    struct monitor_data monitor_data;
    bool health_monitor_support;
    struct algo_info *algo_info;
    int tp_index;
    struct kernel_grip_info *grip_info;
    int view_area_touched;
    int touch_count;
    int irq_slot;
    bool loading_fw;
    void *chip_data;
    struct ts_ops *ts_ops;
    int max_num;
};

#define INVALID_POINT (-1)
#define NORMAL 0
#define HEALTH_GRIP_UP 0
#define HEALTH_GRIP_RECLINING 1

static inline void tp_healthinfo_report(...)
{
}

// Logging in the port is intentionally quiet; the daemon logs the high-level
// bridge state itself.  Keeping the macros as no-ops avoids per-frame log
// storms from the stock algorithm.
#define GRIP_TP_INFO(...) ((void)0)
#define TPD_INFO(...) ((void)0)
#define TPD_DETAIL(...) ((void)0)
#define TPD_DEBUG(...) ((void)0)
#define TP_DETAIL(...) ((void)0)
#define TP_INFO(...) ((void)0)

}  // namespace oplus_prevention
