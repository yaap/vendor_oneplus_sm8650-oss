/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <string>

namespace aidl::vendor::oplus::hardware::charger {

// Type bits accepted by GET_CHARGER_INFO / GET_CHARGER_INFO_UPDATE.  These
// match the stock getChargerInfoType() table.
constexpr uint32_t kChargerInfoUpdate = 1 << 0;
constexpr uint32_t kChargerInfoCommon = 1 << 1;
constexpr uint32_t kChargerInfoBattery = 1 << 2;
constexpr uint32_t kChargerInfoWired = 1 << 3;
constexpr uint32_t kChargerInfoWireless = 1 << 4;

// Returns the JSON document produced by ChargerInfo::getInfo().  `types` is a
// '+'-separated list of Update/Common/Battery/Wired/Wireless.
std::string getChargerInfoJson(const std::string& types);

// Individual getters also used by the ChgFuncFlag handlers.
int32_t getChargerInfoInt(const std::string& name);
std::string getChargerInfoString(const std::string& name);

}  // namespace aidl::vendor::oplus::hardware::charger
