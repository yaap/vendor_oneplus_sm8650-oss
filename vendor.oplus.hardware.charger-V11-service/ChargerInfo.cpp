/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ChargerInfo.h"

#include <json/json.h>

#include "NodeControl.h"

namespace aidl::vendor::oplus::hardware::charger {
namespace {

enum class ValueKind {
    Integer,
    String,
};

struct Entry {
    const char* name;
    uint32_t group;
    ValueKind kind;
    const char* first;
    const char* second;
};

const Entry kEntries[] = {
        // Wired
        {"usbStatus", kChargerInfoWired, ValueKind::Integer,
         "/sys/class/power_supply/usb/usb_status",
         "/sys/class/oplus_chg/usb/usb_status"},
        {"otgOnline", kChargerInfoWired, ValueKind::Integer,
         "/sys/class/power_supply/usb/otg_online",
         "/sys/class/oplus_chg/usb/otg_online"},

        // Battery
        {"batteryRm", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/power_supply/battery/batt_rm",
         "/sys/class/oplus_chg/battery/battery_rm"},
        {"batteryFcc", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/power_supply/battery/batt_fcc",
         "/sys/class/oplus_chg/battery/battery_fcc"},
        {"shortCHwStatus", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/power_supply/battery/short_c_hw_status",
         "/sys/class/oplus_chg/battery/short_c_hw_status"},
        {"shortIcOtpStatus", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/power_supply/battery/short_ic_otp_status",
         "/sys/class/oplus_chg/battery/short_ic_otp_status"},
        {"notifyCode", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/power_supply/battery/notify_code",
         "/sys/class/oplus_chg/battery/battery_notify_code"},
        {"ppsCharging", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/oplus_chg/battery/ppschg_ing",
         "/sys/class/oplus_chg/battery/ppschg_ing"},
        {"ppsChargePower", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/oplus_chg/battery/ppschg_power",
         "/sys/class/oplus_chg/battery/ppschg_power"},
        {"fastCharge", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/power_supply/battery/fast_charge",
         "/sys/class/oplus_chg/battery/fast_charge"},
        {"bmsStatus", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/oplus_chg/battery/bms_status",
         "/sys/class/oplus_chg/battery/bms_heating_status"},
        {"bddLeakCurr", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/oplus_chg/battery/bdd_leak_current",
         "/sys/class/oplus_chg/battery/bdd_leak_current"},
        {"CarC", kChargerInfoBattery, ValueKind::Integer,
         "/sys/class/oplus_chg/battery/gauge_car_c",
         "/sys/class/oplus_chg/battery/gauge_car_c"},

        // Wireless
        {"wlsAdapterPower", kChargerInfoWireless, ValueKind::Integer,
         "/sys/class/oplus_chg/wireless/max_w_power",
         "/sys/class/oplus_chg/wireless/max_w_power"},
        {"enableTx", kChargerInfoWireless, ValueKind::Integer,
         "/proc/wireless/enable_tx", "/proc/wireless/enable_tx"},

        // Common
        {"uiPower", kChargerInfoCommon, ValueKind::Integer,
         "/sys/class/oplus_chg/common/ui_power",
         "/sys/class/oplus_chg/common/ui_power"},
        {"uiIconType", kChargerInfoCommon, ValueKind::Integer,
         "/sys/class/oplus_chg/common/ui_icon_type",
         "/sys/class/oplus_chg/common/ui_icon_type"},
        {"cpaPower", kChargerInfoCommon, ValueKind::Integer,
         "/sys/class/oplus_chg/common/cpa_power",
         "/sys/class/oplus_chg/common/cpa_power"},
        {"isSupportSpeedCharge", kChargerInfoCommon, ValueKind::Integer,
         "/sys/class/oplus_chg/common/protocol_type",
         "/sys/class/oplus_chg/common/protocol_type"},
        {"bypassChgStatus", kChargerInfoCommon, ValueKind::Integer,
         "/sys/class/oplus_chg/common/plc",
         "/sys/class/oplus_chg/common/plc"},
        {"chargeTimeRemaining", kChargerInfoCommon, ValueKind::Integer,
         "/sys/class/power_supply/battery/time_to_full_now",
         "/sys/class/power_supply/battery/time_to_full_now"},
};

uint32_t typeMask(const std::string& name) {
    if (name == "Update") {
        return kChargerInfoUpdate;
    }
    if (name == "Common") {
        return kChargerInfoCommon;
    }
    if (name == "Battery") {
        return kChargerInfoBattery;
    }
    if (name == "Wired") {
        return kChargerInfoWired;
    }
    if (name == "Wireless") {
        return kChargerInfoWireless;
    }
    return 0;
}

const Entry* findEntry(const std::string& name) {
    for (const auto& entry : kEntries) {
        if (name == entry.name) {
            return &entry;
        }
    }
    return nullptr;
}

Json::Value readEntry(const Entry& entry) {
    if (entry.kind == ValueKind::String) {
        auto value = nodeReadFirstString(entry.first, entry.second);
        return value ? Json::Value(*value) : Json::Value("");
    }

    auto value = nodeReadFirstInt(entry.first, entry.second);
    return value ? Json::Value(*value) : Json::Value(kChargerError);
}

}  // namespace

std::string getChargerInfoJson(const std::string& types) {
    uint32_t mask = 0;
    for (const auto& type : splitPlus(types)) {
        mask |= typeMask(type);
    }

    Json::Value root(Json::objectValue);
    for (const auto& entry : kEntries) {
        // Every IntfInfo entry carries the Update bit, so asking for Update
        // selects the complete table.
        if ((mask & kChargerInfoUpdate) != 0 || (entry.group & mask) != 0) {
            root[entry.name] = readEntry(entry);
        }
    }

    Json::FastWriter writer;
    return writer.write(root);
}

int32_t getChargerInfoInt(const std::string& name) {
    const Entry* entry = findEntry(name);
    if (entry == nullptr) {
        return kChargerError;
    }
    if (entry->kind == ValueKind::String) {
        return kChargerError;
    }
    auto value = nodeReadFirstInt(entry->first, entry->second);
    return value.value_or(kChargerError);
}

std::string getChargerInfoString(const std::string& name) {
    const Entry* entry = findEntry(name);
    if (entry == nullptr) {
        return "";
    }

    if (entry->kind == ValueKind::String) {
        auto value = nodeReadFirstString(entry->first, entry->second);
        return value.value_or("");
    }

    auto value = nodeReadFirstInt(entry->first, entry->second);
    return value ? std::to_string(*value) : "";
}

}  // namespace aidl::vendor::oplus::hardware::charger
