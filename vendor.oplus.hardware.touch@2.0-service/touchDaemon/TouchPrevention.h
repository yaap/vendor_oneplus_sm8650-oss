/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Userspace port of the OPLUS touch daemon prevention layer.  The algorithm
 * and configuration are the same as the stock prebuilt daemon; the code is
 * kept behind this small interface so the HBP pipeline can use it without
 * exposing the kernel-derived data structures.
 */

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

struct TouchPreventionPoint {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t z = 0;
    uint8_t width_major = 0;
    uint8_t touch_major = 0;
    uint8_t status = 0;
    uint8_t tx_press = 0;
    uint8_t rx_press = 0;
    uint8_t tx_er = 0;
    uint8_t rx_er = 0;
};

class TouchPrevention {
  public:
    using MakeupPoint = std::function<void(int slot, uint16_t x, uint16_t y)>;
    using MakeupRelease = std::function<void(int slot)>;

    TouchPrevention();
    ~TouchPrevention();

    TouchPrevention(const TouchPrevention&) = delete;
    TouchPrevention& operator=(const TouchPrevention&) = delete;

    // panel_hint is "TIANMA" or "BOE" (or empty).  config_path is the
    // sys_touch_function_feature_config.xml used by the stock daemon.
    bool start(const std::string& panel_hint, const std::string& config_path);
    void stop();

    bool valid() const;

    // in_mask uses the vendor point-slot bits.  out_mask receives the stock
    // prevention result.  The point array is indexed by vendor slot.
    uint32_t process(const std::array<TouchPreventionPoint, 10>& in, uint32_t in_mask,
                     std::array<TouchPreventionPoint, 10>* out);

    void reset();

    // 0 disables the userspace engine (raw points pass through). 1..5 scale
    // the stock edge/corner geometry; level 3 is the stock baseline.
    void setStrength(int level);

    void set_makeup_callbacks(MakeupPoint on_point, MakeupRelease on_release);

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
