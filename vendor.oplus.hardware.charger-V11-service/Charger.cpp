/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Source replacement for vendor.oplus.hardware.charger-V11-service.
 *
 * The stock service is a thin sysfs/proc wrapper around the OPLUS charger
 * kernel driver.  Every AIDL method is implemented here against the same
 * nodes.  No proprietary charger library is linked.
 */

#define LOG_TAG "oplus-charger"

#include "Charger.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <android-base/properties.h>
#include <android/binder_manager.h>
#include <android/log.h>

#include <pthread.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <fstream>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

#include "ChargerInfo.h"
#include "NodeControl.h"
#include "TestKit.h"

namespace aidl::vendor::oplus::hardware::charger {
namespace {

constexpr char kBatteryStatus[] = "/sys/class/power_supply/battery/status";
constexpr char kUsbOnline[] = "/sys/class/power_supply/usb/online";
constexpr char kUsbOnlineAlt[] = "/sys/class/oplus_chg/usb/online";
constexpr char kAcOnline[] = "/sys/class/power_supply/ac/online";
constexpr char kAcOnlineAlt[] = "/sys/class/oplus_chg/ac/online";
constexpr char kAcType[] = "/sys/class/power_supply/ac/type";
constexpr char kAcTypeAlt[] = "/sys/class/oplus_chg/ac/type";
constexpr char kPcPortOnline[] = "/sys/class/power_supply/pc_port/online";
constexpr char kBatteryTemp[] = "/sys/class/power_supply/battery/temp";
constexpr char kBatteryCurrentNow[] =
        "/sys/class/power_supply/battery/current_now";
constexpr char kBatteryVoltageNow[] =
        "/sys/class/power_supply/battery/voltage_now";
constexpr char kBatteryVoltageMin[] =
        "/sys/class/power_supply/battery/voltage_min";
constexpr char kBatteryCapacity[] = "/sys/class/power_supply/battery/capacity";
constexpr char kBatteryChargeNow[] =
        "/sys/class/power_supply/battery/charge_now";
constexpr char kBatteryShortStatus[] =
        "/sys/class/power_supply/battery/short_c_hw_status";
constexpr char kBatteryShortStatusAlt[] =
        "/sys/class/oplus_chg/battery/short_c_hw_status";
constexpr char kBatteryShortFeature[] =
        "/sys/class/power_supply/battery/short_c_hw_feature";
constexpr char kBatteryShortFeatureAlt[] =
        "/sys/class/oplus_chg/battery/short_c_hw_feature";
constexpr char kBatterySubCurrent[] =
        "/sys/class/power_supply/battery/sub_current";
constexpr char kBatterySubCurrentAlt[] =
        "/sys/class/oplus_chg/battery/sub_current";
constexpr char kBatteryAuthenticate[] =
        "/sys/class/power_supply/battery/authenticate";
constexpr char kBatteryAuthenticateAlt[] =
        "/sys/class/oplus_chg/battery/authenticate";
constexpr char kBatteryShortIcOtp[] =
        "/sys/class/power_supply/battery/short_ic_otp_status";
constexpr char kBatteryShortIcOtpAlt[] =
        "/sys/class/oplus_chg/battery/short_ic_otp_status";
constexpr char kBatteryVoocIng[] =
        "/sys/class/power_supply/battery/voocchg_ing";
constexpr char kBatteryVoocIngAlt[] =
        "/sys/class/oplus_chg/battery/voocchg_ing";
constexpr char kBatteryChargeTech[] =
        "/sys/class/power_supply/battery/charge_technology";
constexpr char kBatteryChargeTechAlt[] =
        "/sys/class/oplus_chg/battery/charge_technology";
constexpr char kBatteryChargerIdVolt[] =
        "/sys/class/power_supply/battery/chargerid_volt";
constexpr char kBatteryChargerIdVoltAlt[] =
        "/sys/class/oplus_chg/battery/chargerid_volt";
constexpr char kBatteryChipSoc[] = "/sys/class/power_supply/battery/chip_soc";
constexpr char kBatteryChipSocAlt[] =
        "/sys/class/oplus_chg/battery/chip_soc";
constexpr char kBatteryCc[] = "/sys/class/power_supply/battery/batt_cc";
constexpr char kBatteryCcAlt[] = "/sys/class/oplus_chg/battery/battery_cc";
constexpr char kBatteryCoolDown[] =
        "/sys/class/power_supply/battery/cool_down";
constexpr char kBatteryCoolDownAlt[] =
        "/sys/class/oplus_chg/battery/cool_down";
constexpr char kBatteryNormalCoolDown[] =
        "/sys/class/oplus_chg/battery/normal_cool_down";
constexpr char kBatteryPkgName[] = "/sys/class/oplus_chg/battery/pkg_name";
constexpr char kBatteryShipMode[] =
        "/sys/class/power_supply/battery/ship_mode";
constexpr char kBatteryShipModeAlt[] =
        "/sys/class/oplus_chg/battery/ship_mode";
constexpr char kBatteryEmMode[] = "/sys/class/power_supply/battery/em_mode";
constexpr char kBatteryEmModeAlt[] = "/sys/class/oplus_chg/battery/em_mode";
constexpr char kBatteryMmiEn[] =
        "/sys/class/power_supply/battery/mmi_charging_enable";
constexpr char kBatteryMmiEnAlt[] =
        "/sys/class/oplus_chg/battery/mmi_charging_enable";
constexpr char kBatterySocAdjust[] =
        "/sys/class/power_supply/battery/soc_ajust";
constexpr char kBatterySocAdjustAlt[] =
        "/sys/class/oplus_chg/battery/soc_ajust";
constexpr char kBatteryPpsIng[] = "/sys/class/oplus_chg/battery/ppschg_ing";
constexpr char kBatteryPpsPower[] =
        "/sys/class/oplus_chg/battery/ppschg_power";
constexpr char kBatteryBqfs[] = "/sys/class/oplus_chg/battery/bqfs_status";
constexpr char kBatteryParallelMos[] =
        "/sys/class/oplus_chg/battery/parallel_chg_mos_status";
constexpr char kBatteryParallelMosTest[] =
        "/sys/class/oplus_chg/battery/parallel_chg_mos_test";
constexpr char kBatteryDualChan[] =
        "/sys/class/oplus_chg/battery/dual_chan_info";
constexpr char kBatteryType[] = "/sys/class/oplus_chg/battery/battery_type";
constexpr char kBatteryChemId[] =
        "/sys/class/oplus_chg/battery/battery_chem_id";
constexpr char kBatterySn[] = "/sys/class/oplus_chg/battery/battery_sn";
constexpr char kBatteryGaugeInfo[] =
        "/sys/class/oplus_chg/battery/gauge_info";
constexpr char kBatteryGaugeType[] =
        "/sys/class/oplus_chg/battery/gauge_type";
constexpr char kBatteryUiSoh[] =
        "/sys/class/oplus_chg/battery/battery_ui_soh";
constexpr char kBatterySoh[] = "/sys/class/oplus_chg/battery/battery_soh";
constexpr char kBatteryUiCc[] = "/sys/class/oplus_chg/battery/battery_ui_cc";
constexpr char kBatteryEcoDesign[] =
        "/sys/class/oplus_chg/battery/eco_design_status";
constexpr char kBatteryManuDate[] =
        "/sys/class/oplus_chg/battery/battery_manu_date";
constexpr char kBatteryFirstUsageDate[] =
        "/sys/class/oplus_chg/battery/battery_first_usage_date";
constexpr char kBatteryRechgSoc[] =
        "/sys/class/oplus_chg/battery/rechg_soc";
constexpr char kBatteryGsmCall[] =
        "/sys/class/oplus_chg/battery/gsm_call_ongoing";
constexpr char kBatterySlowChgEn[] =
        "/sys/class/oplus_chg/battery/slow_chg_en";
constexpr char kBatterySecIcTest[] =
        "/sys/class/oplus_chg/battery/sec_ic_test";
constexpr char kBatteryBmsTemp[] =
        "/sys/class/oplus_chg/battery/bms_heat_temp_compensation";
constexpr char kBatteryBccCurrent[] =
        "/sys/class/oplus_chg/battery/bcc_current";
constexpr char kBatteryBccException[] =
        "/sys/class/oplus_chg/battery/bcc_exception";

constexpr char kUsbFastChgType[] =
        "/sys/class/power_supply/usb/fast_chg_type";
constexpr char kUsbFastChgTypeAlt[] =
        "/sys/class/oplus_chg/usb/fast_chg_type";
constexpr char kUsbInputCurrent[] =
        "/sys/class/power_supply/usb/input_current_now";
constexpr char kUsbInputCurrentAlt[] =
        "/sys/class/oplus_chg/usb/input_current_now";
constexpr char kUsbOtgSwitch[] =
        "/sys/class/power_supply/usb/otg_switch";
constexpr char kUsbOtgSwitchAlt[] =
        "/sys/class/oplus_chg/usb/otg_switch";
constexpr char kUsbPrimalType[] =
        "/sys/class/power_supply/usb/primal_type";
constexpr char kUsbPrimalTypeAlt[] =
        "/sys/class/oplus_chg/usb/primal_type";
constexpr char kUsbTypeOrientation[] =
        "/sys/class/power_supply/usb/typec_cc_orientation";
constexpr char kUsbTypeOrientationAlt[] =
        "/sys/class/oplus_chg/usb/typec_cc_orientation";
constexpr char kUsbVdmInfo[] = "/sys/class/oplus_chg/usb/vdm_info";

constexpr char kWirelessOnline[] =
        "/sys/class/power_supply/wireless/online";
constexpr char kWirelessRealType[] =
        "/sys/class/power_supply/wireless/real_type";
constexpr char kWirelessRealTypeAlt[] =
        "/sys/class/oplus_chg/wireless/real_type";
constexpr char kWirelessCurrent[] =
        "/sys/class/power_supply/wireless/current_now";
constexpr char kWirelessCurrentAlt[] =
        "/sys/class/oplus_chg/wireless/current_now";
constexpr char kWirelessVoltage[] =
        "/sys/class/power_supply/wireless/voltage_now";
constexpr char kWirelessVoltageAlt[] =
        "/sys/class/oplus_chg/wireless/voltage_now";
constexpr char kWirelessCapacity[] =
        "/sys/class/power_supply/wireless/capacity";
constexpr char kWirelessCapacityAlt[] =
        "/sys/class/oplus_chg/wireless/capacity";
constexpr char kWirelessPtmcId[] =
        "/sys/class/power_supply/wireless/ptmc_id";
constexpr char kWirelessIconDelay[] =
        "/sys/class/power_supply/wireless/icon_delay";
constexpr char kWirelessIconDelayAlt[] =
        "/sys/class/oplus_chg/wireless/icon_delay";

constexpr char kProcChargerCycle[] = "/proc/charger_cycle";
constexpr char kProcChargerCriticalLog[] = "/proc/charger_critical_log";
constexpr char kProcChargerLog[] = "/proc/charger_log";
constexpr char kProcChargerControl[] = "/dev/oplus_chg";
constexpr char kProcChargerCtl[] = "/proc/charger/chg_ctl";
constexpr char kProcChargerHmac[] = "/proc/charger/hmac";
constexpr char kProcInputCurrent[] = "/proc/charger/input_current_now";
constexpr char kProcPassedChg[] = "/proc/charger/passedchg";
constexpr char kProcPassedChgReset[] =
        "/proc/charger/passedchg_reset_count";
constexpr char kProcQgVbatDeviation[] = "/proc/charger/qg_vbat_deviation";
constexpr char kProcCpVbatDeviation[] = "/proc/charger/cp_vbat_deviation";
constexpr char kProcFactoryMode[] =
        "/proc/charger/charger_factorymode_test";
constexpr char kProcGaugeIbatDeviation[] =
        "/proc/charger/gauge_ibat_deviation";
constexpr char kProcReserveSocDebug[] =
        "/proc/charger/reserve_soc_debug";
constexpr char kProcUiSocDecimal[] = "/proc/ui_soc_decimal";
constexpr char kProcBattParamNoplug[] = "/proc/batt_param_noplug";
constexpr char kProcDevinfoFastchg[] = "/proc/devinfo/fastchg";
constexpr char kProcFastchgFwUpdate[] = "/proc/fastchg_fw_update";
constexpr char kProcTbattPwrOff[] = "/proc/tbatt_pwroff";
constexpr char kProcWirelessRxPower[] = "/proc/wireless/rx_power";
constexpr char kProcWirelessTxPower[] = "/proc/wireless/tx_power";
constexpr char kProcWirelessRxVersion[] = "/proc/wireless/rx_version";
constexpr char kProcWirelessTxVersion[] = "/proc/wireless/tx_version";
constexpr char kProcWirelessIdtAdcTest[] = "/proc/wireless/idt_adc_test";
constexpr char kProcWirelessEnableTx[] = "/proc/wireless/enable_tx";
constexpr char kProcWirelessEnableRx[] = "/proc/wireless/enable_rx";
constexpr char kProcWirelessChargePumpEn[] =
        "/proc/wireless/charge_pump_en";
constexpr char kProcWirelessDeviated[] = "/proc/wireless/deviated";
constexpr char kProcWirelessFtmMode[] = "/proc/wireless/ftm_mode";
constexpr char kProcWirelessUserSleep[] =
        "/proc/wireless/user_sleep_mode";
constexpr char kProcWiredOtgOnline[] = "/proc/wireless/wired_otg_online";
constexpr char kProcWirelessMac[] = "/proc/wireless/mac_addr";


constexpr char kPmFtmVph[] =
        "/proc/device-tree/soc/qcom,pmic_glink/qcom,battery_charger/"
        "qcom,pmic_ftm_vph";
constexpr char kPmFtmVbat[] =
        "/proc/device-tree/soc/qcom,pmic_glink/qcom,battery_charger/"
        "qcom,pmic_ftm_vbat";
constexpr char kVphPwr[] = "/sys/class/power_supply/battery/vph_pwr";
constexpr char kVbatSns[] = "/sys/class/power_supply/battery/vbat_sns_qbg";

constexpr char kTrackHidl[] = "/sys/class/oplus_chg/common/track_hidl";
constexpr char kChgUpLimit[] = "/sys/class/oplus_chg/common/chg_up_limit";
constexpr char kPlc[] = "/sys/class/oplus_chg/common/plc";
constexpr char kDevicePower[] =
        "/sys/class/oplus_chg/common/device_power";
constexpr char kAdapterPower[] =
        "/sys/class/oplus_chg/common/adapter_power";
constexpr char kDeepDischgCounts[] =
        "/sys/class/oplus_chg/common/deep_dischg_counts";
constexpr char kReverseChgInfo[] =
        "/sys/class/oplus_chg/common/reverse_chg_info";
constexpr char kSuperEnduranceStatus[] =
        "/sys/class/oplus_chg/common/super_endurance_mode_status";
constexpr char kSuperEnduranceCount[] =
        "/sys/class/oplus_chg/common/super_endurance_mode_count";
constexpr char kBobStatus[] = "/sys/class/oplus_chg/common/bob_status";
constexpr char kBobStatusReg[] =
        "/sys/class/oplus_chg/common/bob_status_reg";
constexpr char kChgOlcConfig[] =
        "/sys/class/oplus_chg/common/chg_olc_config";
constexpr char kBattLogPush[] =
        "/sys/class/oplus_chg/common/battlog_push_config";

constexpr char kRxDisable[] =
        "/sys/class/oplus_chg/wireless/rx_disable";
constexpr char kBtInfo[] = "/sys/class/oplus_chg/wireless/bt_info";
constexpr char kWlsPenInfo[] =
        "/sys/class/oplus_chg/wireless/wlspen_info";
constexpr char kWlsPenDischgSoc[] =
        "/sys/class/oplus_chg/wireless/wlspen_dischg_soc";
constexpr char kWlsPenSoc[] =
        "/sys/class/oplus_chg/wireless/wlspen_soc";
constexpr char kWlsPenPingTime[] =
        "/sys/class/oplus_chg/wireless/ping_time";

constexpr char kPencilRxSoc[] = "/sys/class/oplus_wireless/pencil/rx_soc";
constexpr char kPencilPresent[] =
        "/sys/class/oplus_wireless/pencil/present";
constexpr char kPencilQcali[] =
        "/sys/class/oplus_wireless/pencil/q_cali";
constexpr char kPencilTxStatus[] =
        "/sys/class/oplus_wireless/pencil/tx_status";
constexpr char kPencilTxCurrent[] =
        "/sys/class/oplus_wireless/pencil/tx_current";
constexpr char kPencilTxVoltage[] =
        "/sys/class/oplus_wireless/pencil/tx_voltage";
constexpr char kPencilFwVersion[] =
        "/sys/class/oplus_wireless/pencil/fw_version";
constexpr char kPencilWirelessIcId[] =
        "/sys/class/oplus_wireless/pencil/wireless_ic_id";
constexpr char kPencilBleMac[] =
        "/sys/class/oplus_wireless/pencil/ble_mac_addr";
constexpr char kPencilPingTime[] =
        "/sys/class/oplus_wireless/pencil/ping_time";
constexpr char kPencilTxChargeDisable[] =
        "/sys/class/oplus_wireless/pencil/tx_charge_disable";

constexpr char kSohPartition[] = "/dev/block/by-name/oplusreserve1";
constexpr char kUfsInfo[] = "/proc/devinfo/ufs";
constexpr off_t kWlsThirdOffsetUfs = 0x48a01a;
constexpr off_t kWlsThirdOffsetEmmc = 0x467c1a;
constexpr off_t kGaugeCalibOffsetUfs = 0x48a4eb;
constexpr off_t kGaugeCalibOffsetEmmc = 0x4e6500;
constexpr off_t kDecCvOffsetUfs = 0x48a51f;
constexpr off_t kDecCvOffsetEmmc = 0x4e6534;
constexpr off_t kPrechgROffsetUfs = 0x51a30e;
constexpr off_t kPrechgROffsetEmmc = 0x4dc70e;
constexpr off_t kDischgBoostOffsetUfs = 0x51a38e;
constexpr off_t kDischgBoostOffsetEmmc = 0x4dc78e;
constexpr off_t kRusConfigOffsetUfs = 0x48a260;
constexpr off_t kRusConfigOffsetEmmc = 0x467e60;
constexpr size_t kWlsThirdSize = 0x100;
constexpr size_t kGaugeCalibSize = 0x34;
constexpr size_t kDecCvSize = 0x1c;
constexpr size_t kPrechgRSize = 0x40;
constexpr size_t kDischgBoostSize = 0x80;
constexpr size_t kRusConfigSize = 0xfc;
constexpr char kPrjName[] = "/proc/oplusVersion/prjName";
constexpr off_t kSohOffsetUfs = 0x48a11a;
constexpr off_t kSohOffsetEmmc = 0x467d1a;


ndk::ScopedAStatus ok() {
    return ndk::ScopedAStatus::ok();
}

std::string readTrimmedOr(const std::string& path,
                          const std::string& fallback = "") {
    auto value = nodeReadTrimmed(path);
    return value.value_or(fallback);
}

std::string readStringOr(const std::string& path,
                         const std::string& fallback = "") {
    auto value = nodeReadString(path);
    return value.value_or(fallback);
}

int32_t readIntOr(const std::string& path, int32_t fallback = 0) {
    auto value = nodeReadInt(path);
    return value.value_or(fallback);
}

int32_t readIntFirstOr(const std::string& first, const std::string& second,
                       int32_t fallback = 0) {
    auto value = nodeReadFirstInt(first, second);
    return value.value_or(fallback);
}

std::string readTrimmedFirstOr(const std::string& first,
                               const std::string& second,
                               const std::string& fallback = "") {
    auto value = nodeReadFirstString(first, second);
    return value ? *value : fallback;
}

// Reads the raw contents of a power_supply node and classifies it like the
// stock queryChargeInfo() code.
std::string batteryStatusName() {
    return readTrimmedOr(kBatteryStatus);
}

int32_t batteryStatusValue() {
    const std::string status = batteryStatusName();
    if (status == "Charging") {
        return 2;
    }
    if (status == "Discharging") {
        return 3;
    }
    if (status == "Not charging") {
        return 4;
    }
    if (status == "Full") {
        return 5;
    }
    return 1;
}

int32_t enableTxValue() {
    const std::string value = readTrimmedOr(kProcWirelessEnableTx);
    if (value == "charging") {
        return 1;
    }
    if (value == "enable") {
        return 2;
    }
    if (value == "disable") {
        return 3;
    }
    return 4;
}

int32_t wirelessChargerType() {
    const std::string type = readTrimmedOr("/sys/class/power_supply/usb/type");
    if (type == "5" || type == "6") {
        return 5;
    }
    if (type == "USB_PD" || type == "USB_PD_DRP" ||
        type == "USB_HVDCP" || type == "USB_HVDCP_3") {
        return 6;
    }
    if (type == "9" || type == "10") {
        return 6;
    }
    return -1;
}

int32_t pmVph() {
    auto value = nodeReadInt(kPmFtmVph);
    if (value) {
        return *value;
    }
    return readIntOr(kVphPwr, kChargerError);
}

int32_t pmVbat() {
    auto value = nodeReadInt(kPmFtmVbat);
    if (value) {
        return *value;
    }
    return readIntOr(kVbatSns, kChargerError);
}

std::string currentPlatform() {
    const std::string model = android::base::GetProperty("ro.soc.model", "");
    if (model == "QTI") {
        return "qcom";
    }
    if (model == "Mediatek") {
        return "mtk";
    }
    return "unknown";
}

std::vector<std::string> parseEyeDiagram(const std::string& value) {
    std::vector<std::string> tokens;
    if (value.find(',') == std::string::npos) {
        tokens.emplace_back("unknown#");
        tokens.push_back(value);
        return tokens;
    }

    size_t start = 0;
    while (start <= value.size()) {
        const size_t next = value.find(',', start);
        tokens.push_back(value.substr(
                start, next == std::string::npos ? std::string::npos
                                                 : next - start));
        if (next == std::string::npos) {
            break;
        }
        start = next + 1;
    }
    return tokens;
}

int32_t qcomEyeDiagramSetting(const std::vector<std::string>& tokens,
                              bool isDefault) {
    std::string value;
    if (!isDefault) {
        if (tokens.size() != 2) {
            return -1;
        }
        value = tokens[1];
    }

    const int32_t rc = nodeWrite("/proc/usb/repeater_parameters", value);
    if (rc == kChargerWriteError) {
        return -1;
    }
    if (rc == kChargerError) {
        return 0;
    }
    return 1;
}

int32_t mtkEyeDiagramSetting(const std::vector<std::string>& tokens,
                             bool isDefault) {
    int32_t eusb2Result = 0;
    int32_t xsphyResult = 0;
    for (size_t i = 1; i < tokens.size(); ++i) {
        std::string value = isDefault ? "" : tokens[i];
        if (value.empty()) {
            continue;
        }
        if (i == 1) {
            eusb2Result = nodeWrite("/proc/oplus_usb_eusb2/eusb2", value);
        } else if (i == 2) {
            xsphyResult = nodeWrite("/proc/oplus_usb_xsphy/xsphy", value);
        }
    }

    if (eusb2Result == kChargerError && xsphyResult == kChargerError) {
        return 0;
    }
    if (eusb2Result == kChargerWriteError &&
        xsphyResult == kChargerWriteError) {
        return -1;
    }
    return 1;
}

std::string getQcomEyeDiagram() {
    auto value = nodeReadTrimmed("/proc/usb/repeater_parameters");
    if (!value) {
        return "unsupported";
    }
    if (value->empty() || *value == "default") {
        return "";
    }
    return *value;
}

std::string getMtkEyeDiagram() {
    std::string eusb2 = readTrimmedOr("/proc/oplus_usb_eusb2/eusb2");
    std::string xsphy = readTrimmedOr("/proc/oplus_usb_xsphy/xsphy");
    if (eusb2 == "default") {
        eusb2.clear();
    }
    if (xsphy == "default") {
        xsphy.clear();
    }
    if (eusb2.empty() && xsphy.empty()) {
        return "unsupported";
    }
    return "mtk," + eusb2 + "," + xsphy;
}

// The package list used by setSmartCoolDown().  The stock service maps known
// app packages to a stable pkg_NN label; unknown packages become pkg_xx.
struct PackageEntry {
    const char* label;
    const char* package;
};

constexpr PackageEntry kPackageTable[] = {
        {"pkg_01", "com.ss.android.ugc.aweme"},
        {"pkg_02", "com.ss.android.ugc.aweme.lite"},
        {"pkg_03", "com.kuaishou.nebula"},
        {"pkg_04", "com.tencent.mm"},
        {"pkg_05", "com.tencent.tmgp.sgame"},
        {"pkg_06", "com.dragon.read"},
        {"pkg_07", "io.legado.app.release"},
        {"pkg_08", "tv.danmaku.bili"},
        {"pkg_09", "com.tencent.jkchess"},
        {"pkg_10", "com.smile.gifmaker"},
        {"pkg_11", "com.sup.android.superb"},
        {"pkg_12", "com.ss.android.article.lite"},
        {"pkg_13", "com.tencent.lolm"},
        {"pkg_14", "com.kmxs.reader"},
        {"pkg_15", "com.chaozh.iReader.dj"},
        {"pkg_16", "cn.xiaochuankeji.zuiyouLite"},
        {"pkg_17", "com.yueyou.adreader"},
        {"pkg_18", "com.blizzard.wtcg.hearthstone"},
        {"pkg_19", "com.tencent.tmgp.pubgmhd"},
        {"pkg_20", "com.hypergryph.arknights"},
        {"pkg_21", "com.chaozh.iReaderFree"},
        {"pkg_22", "com.shuqi.controller"},
        {"pkg_23", "com.miHoYo.Yuanshen"},
        {"pkg_24", "com.tencent.KiHan"},
        {"pkg_25", "com.ss.android.article.news"},
        {"pkg_26", "com.qidian.QDReader"},
        {"pkg_27", "com.jjwxc.reader"},
        {"pkg_28", "com.tencent.tmgp.cf"},
        {"pkg_29", "cn.xiaochuankeji.tieba"},
        {"pkg_30", "cn.jj"},
        {"pkg_31", "com.ss.android.ugc.live"},
        {"pkg_32", "com.qq.reader"},
        {"pkg_33", "com.tencent.tmgp.cod"},
        {"pkg_34", "com.le123.ysdq"},
        {"pkg_35", "com.facebook.katana"},
        {"pkg_36", "com.happyelements.AndroidAnimal"},
        {"pkg_37", "com.duowan.kiwi"},
        {"pkg_38", "com.ss.android.article.video"},
        {"pkg_39", "com.google.android.youtube"},
        {"pkg_40", "com.netease.newsreader.activity"},
        {"pkg_41", "com.weico.international"},
        {"pkg_42", "air.tv.douyu.android"},
        {"pkg_43", "com.tencent.qqlive"},
        {"pkg_44", "com.taobao.live"},
        {"pkg_45", "com.tencent.tmgp.speedmobile"},
        {"pkg_46", "com.UCMobile"},
        {"pkg_47", "com.hupu.games"},
        {"pkg_48", "com.baidu.haokan"},
        {"pkg_49", "com.qqgame.happymj"},
        {"pkg_50", "com.xunmeng.merchant"},
        {"pkg_xx", "oplus.chg.other.application"},
};

std::string packageLabel(const std::string& package) {
    for (const auto& entry : kPackageTable) {
        if (package == entry.package) {
            return entry.label;
        }
    }
    return "pkg_xx";
}

// The stock string-only extraParse() returns the substring after the last
// '+' and reports an error when no separator is present.
std::string extraTail(const std::string& extra) {
    const size_t separator = extra.rfind('+');
    if (separator == std::string::npos) {
        return "";
    }
    return extra.substr(separator + 1);
}

std::string batterySecIcTestStatus(const std::string& extra) {
    std::string ignored;
    std::vector<int32_t> args;
    extraParse(extra, &ignored, &args);
    if (args.size() != 1 || args[0] < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "Invalid input parameter.");
        return "-2147483648";
    }

    const std::string input = std::to_string(args[0]) + " ";
    if (nodeWrite(kBatterySecIcTest, input) != 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "writeFile failed");
        return "-2147483648";
    }

    auto output = nodeReadTrimmed(kBatterySecIcTest);
    if (!output) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "readFile failed");
        return "-2147483648";
    }

    std::istringstream stream(*output);
    int32_t command = 0;
    int32_t value = 0;
    stream >> command >> value;
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "output arg_list[0]:%d, cmd:%d val:%d", args[0],
                        command, value);
    return std::to_string(value);
}


bool isUfsDevice() {
    return access(kUfsInfo, F_OK) == 0;
}

off_t partitionOffset(off_t ufsOffset, off_t emmcOffset) {
    return isUfsDevice() ? ufsOffset : emmcOffset;
}

bool readPartition(off_t offset, void* buffer, size_t size) {
    const int fd = open(kSohPartition, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    const bool ok = lseek(fd, offset, SEEK_SET) == offset &&
                    read(fd, buffer, size) == static_cast<ssize_t>(size);
    close(fd);
    return ok;
}

bool writePartition(off_t offset, const void* buffer, size_t size) {
    const int fd = open(kSohPartition, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    const bool ok = lseek(fd, offset, SEEK_SET) == offset &&
                    write(fd, buffer, size) == static_cast<ssize_t>(size);
    close(fd);
    return ok;
}

struct OplusChgCmd {
    int32_t cmd;
    int32_t data_size;
    uint8_t data[0x100];
};

static_assert(sizeof(OplusChgCmd) == 0x108);

std::atomic<bool> gExchangeThreadStarted{false};
std::atomic<bool> gUisohThreadStarted{false};
std::atomic<int32_t> gCachedUiSoh{100};

constexpr uint8_t kAesSbox[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67,
        0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59,
        0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7,
        0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1,
        0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05,
        0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83,
        0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29,
        0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
        0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa,
        0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c,
        0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc,
        0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
        0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19,
        0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee,
        0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49,
        0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4,
        0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6,
        0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70,
        0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9,
        0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e,
        0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1,
        0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0,
        0x54, 0xbb, 0x16,
};

constexpr uint8_t kAesRcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10,
                                   0x20, 0x40, 0x80, 0x1b, 0x36};

void aesExpandKey(const uint8_t key[16], uint8_t roundKey[176]) {
    memcpy(roundKey, key, 16);
    for (int i = 4; i < 44; ++i) {
        uint8_t temp[4];
        memcpy(temp, roundKey + (i - 1) * 4, 4);
        if (i % 4 == 0) {
            const uint8_t first = temp[0];
            temp[0] = kAesSbox[temp[1]] ^ kAesRcon[i / 4];
            temp[1] = kAesSbox[temp[2]];
            temp[2] = kAesSbox[temp[3]];
            temp[3] = kAesSbox[first];
        }
        for (int j = 0; j < 4; ++j) {
            roundKey[i * 4 + j] =
                    roundKey[(i - 4) * 4 + j] ^ temp[j];
        }
    }
}

void aesEncryptBlock(const uint8_t key[16], const uint8_t input[16],
                     uint8_t output[16]) {
    uint8_t roundKey[176];
    aesExpandKey(key, roundKey);

    uint8_t state[16];
    memcpy(state, input, sizeof(state));
    for (int i = 0; i < 16; ++i) {
        state[i] ^= roundKey[i];
    }

    for (int round = 1; round <= 10; ++round) {
        for (int i = 0; i < 16; ++i) {
            state[i] = kAesSbox[state[i]];
        }

        uint8_t shifted[16];
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                shifted[row + column * 4] =
                        state[row + ((column + row) % 4) * 4];
            }
        }
        memcpy(state, shifted, sizeof(state));

        if (round != 10) {
            for (int column = 0; column < 4; ++column) {
                uint8_t* c = state + column * 4;
                const uint8_t a0 = c[0];
                const uint8_t a1 = c[1];
                const uint8_t a2 = c[2];
                const uint8_t a3 = c[3];
                const uint8_t x0 = static_cast<uint8_t>(a0 ^ a1 ^ a2 ^ a3);
                auto xtime = [](uint8_t value) -> uint8_t {
                    return static_cast<uint8_t>((value << 1) ^
                                                ((value >> 7) * 0x1b));
                };
                c[0] ^= x0 ^ xtime(static_cast<uint8_t>(a0 ^ a1));
                c[1] ^= x0 ^ xtime(static_cast<uint8_t>(a1 ^ a2));
                c[2] ^= x0 ^ xtime(static_cast<uint8_t>(a2 ^ a3));
                c[3] ^= x0 ^ xtime(static_cast<uint8_t>(a3 ^ a0));
            }
        }

        for (int i = 0; i < 16; ++i) {
            state[i] ^= roundKey[round * 16 + i];
        }
    }
    memcpy(output, state, sizeof(state));
}

int exchangeWrite(int fd, OplusChgCmd* command) {
    const ssize_t written = write(fd, command, sizeof(*command));
    if (written != static_cast<ssize_t>(sizeof(*command))) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "write fd fail size:%d", static_cast<int>(written));
        return -1;
    }
    return 0;
}

int obtainGaugeCalibration(int fd, OplusChgCmd* command) {
    std::array<uint8_t, kGaugeCalibSize> buffer = {};
    if (!readPartition(partitionOffset(kGaugeCalibOffsetUfs,
                                       kGaugeCalibOffsetEmmc),
                       buffer.data(), buffer.size())) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "read from: %s failed", kSohPartition);
        return -1;
    }
    if (memcmp(buffer.data(), "gauge_calib", 12) != 0) {
        buffer.fill(0);
        memcpy(buffer.data(), "gauge_calib", 12);
    }
    command->data_size = static_cast<int32_t>(buffer.size());
    memcpy(command->data, buffer.data(), buffer.size());
    return exchangeWrite(fd, command);
}

int updateGaugeCalibration(int fd, OplusChgCmd* command) {
    if (command->data_size != static_cast<int32_t>(kGaugeCalibSize)) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "data size[%d] is error", command->data_size);
        return -1;
    }
    return writePartition(partitionOffset(kGaugeCalibOffsetUfs,
                                          kGaugeCalibOffsetEmmc),
                          command->data, kGaugeCalibSize)
                   ? 0
                   : -1;
}

int obtainDecCvDown(int fd, OplusChgCmd* command) {
    std::array<uint8_t, kDecCvSize> buffer = {};
    if (!readPartition(partitionOffset(kDecCvOffsetUfs, kDecCvOffsetEmmc),
                       buffer.data(), buffer.size())) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "read from: %s failed", kSohPartition);
        return -1;
    }
    command->data_size = static_cast<int32_t>(buffer.size());
    memcpy(command->data, buffer.data(), buffer.size());
    return exchangeWrite(fd, command);
}

int updateDecCvDown(int fd, OplusChgCmd* command) {
    if (command->data_size != static_cast<int32_t>(kDecCvSize)) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "data size[%d] is error", command->data_size);
        return -1;
    }
    return writePartition(partitionOffset(kDecCvOffsetUfs, kDecCvOffsetEmmc),
                          command->data, kDecCvSize)
                   ? 0
                   : -1;
}

int obtainPrechargeResistance(int fd, OplusChgCmd* command) {
    std::array<char, kPrechgRSize> buffer = {};
    const off_t offset = partitionOffset(kPrechgROffsetUfs,
                                         kPrechgROffsetEmmc);
    if (!readPartition(offset, buffer.data(), buffer.size())) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "read from: %s failed", kSohPartition);
        return -1;
    }
    command->data_size = static_cast<int32_t>(buffer.size());
    memcpy(command->data, buffer.data(), buffer.size());
    const int rc = exchangeWrite(fd, command);
    if (rc < 0) {
        return rc;
    }

    const std::string text(buffer.data(), buffer.size());
    if (text.find("prechg_r:") == std::string::npos) {
        __android_log_print(ANDROID_LOG_INFO, "chg_exchange_mesg",
                            "partion not has r_data info");
        return 0;
    }

    std::array<char, kPrechgRSize> cleared = {};
    if (!writePartition(offset, cleared.data(), cleared.size())) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "write to: %s failed", kSohPartition);
        return -1;
    }
    return 0;
}

int obtainDischargeBoostError(int fd, OplusChgCmd* command) {
    std::array<char, kDischgBoostSize> buffer = {};
    const off_t offset = partitionOffset(kDischgBoostOffsetUfs,
                                         kDischgBoostOffsetEmmc);
    if (!readPartition(offset, buffer.data(), buffer.size())) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "read from: %s failed", kSohPartition);
        buffer.fill(0);
    }

    const std::string text(buffer.data(), buffer.size());
    if (buffer[0] != '\0' &&
        text.find("dischg_boost_err") == std::string::npos) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "read tag info fail, invalid dischg boost err "
                            "format");
        buffer.fill(0);
    }

    command->data_size = static_cast<int32_t>(buffer.size());
    memcpy(command->data, buffer.data(), buffer.size());
    return exchangeWrite(fd, command);
}

int authenticateWlsThirdParty(int fd, OplusChgCmd* command) {
    if (command->data_size != 4) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "data size[%d] is error", command->data_size);
        return -1;
    }

    int32_t vendorId = 0;
    memcpy(&vendorId, command->data, sizeof(vendorId));
    if (vendorId <= 0 || vendorId > 17) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "vendor_id[%d] is invalid", vendorId);
        return -1;
    }

    std::array<char, kWlsThirdSize> record = {};
    readPartition(partitionOffset(kWlsThirdOffsetUfs,
                                  kWlsThirdOffsetEmmc),
                  record.data(), record.size());
    const std::string text(record.data(),
                           strnlen(record.data(), record.size()));

    int32_t keyIndex = 1;
    size_t position = 0;
    while ((position = text.find("v=", position)) != std::string::npos) {
        int parsedVendor = 0;
        int parsedKey = 0;
        if (sscanf(text.c_str() + position, "v=%d,k=%d", &parsedVendor,
                   &parsedKey) == 2 &&
            parsedVendor == vendorId) {
            keyIndex = parsedKey;
            break;
        }
        position += 2;
    }

    int randomFd = -1;
    for (int attempt = 0; randomFd < 0 && attempt < 15; ++attempt) {
        randomFd = open("/dev/random", O_RDONLY | O_CLOEXEC);
        if (randomFd < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                                "can't open /dev/random, rc=%d", randomFd);
            sleep(1);
        }
    }
    if (randomFd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                            "get randoms fail");
        return -1;
    }

    uint8_t randomBytes[16] = {};
    for (size_t i = 0; i < sizeof(randomBytes); ++i) {
        if (read(randomFd, &randomBytes[i], 1) != 1) {
            __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                                "can't read /dev/random");
            close(randomFd);
            return -1;
        }
    }
    close(randomFd);

    // The stock service first tries the SE-derived key and falls back to the
    // built-in test key when lib_cryptoeng_api is unavailable.
    constexpr uint8_t kFallbackKey[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                          0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                          0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t encoded[16] = {};
    aesEncryptBlock(kFallbackKey, randomBytes, encoded);

    command->data_size = 0x24;
    memcpy(command->data, &keyIndex, sizeof(keyIndex));
    memcpy(command->data + 4, randomBytes, sizeof(randomBytes));
    memcpy(command->data + 20, encoded, sizeof(encoded));
    return exchangeWrite(fd, command);
}

int handleExchangeCommand(int fd, OplusChgCmd* command) {
    switch (command->cmd) {
        case 0:
            return authenticateWlsThirdParty(fd, command);
        case 1:
            return obtainGaugeCalibration(fd, command);
        case 2:
            return updateGaugeCalibration(fd, command);
        case 3:
            return obtainDecCvDown(fd, command);
        case 4:
            return updateDecCvDown(fd, command);
        case 5:
            return obtainPrechargeResistance(fd, command);
        case 6:
            return obtainDischargeBoostError(fd, command);
        default:
            __android_log_print(ANDROID_LOG_WARN, "chg_exchange_mesg",
                                "cmd(=%d) not support", command->cmd);
            return -1;
    }
}

void* chgExchangeThread(void*) {
    if (gExchangeThreadStarted.exchange(true)) {
        return nullptr;
    }
    __android_log_print(ANDROID_LOG_INFO, "chg_exchange_mesg",
                        "chg_exchange_mesg_thread runing.");

    int fd = -1;
    for (int attempt = 0; fd < 0 && attempt < 500; ++attempt) {
        fd = open("/sys/class/oplus_chg/common/mutual_cmd",
                  O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            fd = open("/sys/class/oplus_mutual/cmd", O_RDWR | O_CLOEXEC);
        }
        if (fd < 0) {
            if (attempt == 499) {
                return nullptr;
            }
            sleep(1);
        }
    }

    // Seed the WLS third-party key record with the stock default layout when
    // the partition does not contain a valid record yet.
    std::array<char, kWlsThirdSize> wls = {};
    const off_t wlsOffset = partitionOffset(kWlsThirdOffsetUfs,
                                            kWlsThirdOffsetEmmc);
    const bool partitionRead =
            readPartition(wlsOffset, wls.data(), wls.size());
    const std::string wlsText(wls.data(),
                              strnlen(wls.data(), wls.size()));
    const bool haveWls =
            partitionRead &&
            wlsText.find("wls aes key data is good") != std::string::npos;
    if (!haveWls) {
        const std::string defaults =
                "wls aes key data is good,v=1,k=1,v=2,k=1,v=3,k=1,v=4,k=1,"
                "v=5,k=1,v=6,k=1,v=7,k=1,v=8,k=1,v=9,k=1,v=10,k=1,v=11,k=1,"
                "v=12,k=1,v=13,k=1,v=14,k=1,v=15,k=1,v=16,k=1,v=17,k=1";
        std::array<char, kWlsThirdSize> record = {};
        memcpy(record.data(), defaults.data(),
               std::min(defaults.size(), record.size() - 1));
        writePartition(wlsOffset, record.data(), record.size());
    }

    for (;;) {
        lseek(fd, 0, SEEK_SET);
        OplusChgCmd command = {};
        const ssize_t count = read(fd, &command, sizeof(command));
        if (count < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                                "failed to read, rc=%d",
                                static_cast<int>(count));
            close(fd);
            return nullptr;
        }
        if (count == 0) {
            usleep(100000);
            continue;
        }
        if (command.cmd >= 0 && command.cmd < 7) {
            const int rc = handleExchangeCommand(fd, &command);
            if (rc < 0) {
                __android_log_print(ANDROID_LOG_ERROR, "chg_exchange_mesg",
                                    "failed to cmd(=%d) run, rc=%d",
                                    command.cmd, rc);
            }
        } else {
            __android_log_print(ANDROID_LOG_WARN, "chg_exchange_mesg",
                                "cmd(=%d) not support", command.cmd);
        }
    }
}

void* uisohUpdateThread(void*) {
    if (gUisohThreadStarted.exchange(true)) {
        return nullptr;
    }
    for (int attempt = 0; attempt < 15; ++attempt) {
        if (access(kBatterySoh, F_OK) == 0) {
            break;
        }
        __android_log_print(ANDROID_LOG_WARN, "chg_exchange_soh_mesg",
                            "[THREAD]%s can't open, access_count=%d",
                            kBatterySoh, attempt);
        sleep(1);
    }
    for (;;) {
        auto soh = nodeReadFirstInt(kBatteryUiSoh, kBatterySoh);
        if (soh && *soh > 0) {
            gCachedUiSoh.store(*soh);
            __android_log_print(ANDROID_LOG_INFO, "chg_exchange_soh_mesg",
                                "[THREAD]uisoh_update_thread_running");
        }
        sleep(1800);
    }
}

}  // namespace

Charger::Charger() {
    chgFuncMapInit();
    chgDualCellsProtInit();
}

Charger::~Charger() = default;

void Charger::initializeBmsConfig() {
    if (bmsConfigLoaded_) {
        return;
    }
    bmsConfigLoaded_ = true;

    std::string project;
    if (auto value = nodeReadRaw(kPrjName, 5)) {
        project = *value;
        const size_t end = project.find_first_of(" \t\r\n\0");
        if (end != std::string::npos) {
            project.resize(end);
        }
    }

    std::vector<std::string> candidates;
    if (!project.empty()) {
        candidates.push_back("/odm/firmware/fastchg/" + project +
                             "_bms_heating_config.txt");
        candidates.push_back("/odm/firmware/fastchg/" + project +
                             "/bms_heating_config.txt");
    }
    candidates.emplace_back("/odm/firmware/fastchg/bms_heating_config.txt");

    std::ifstream config;
    for (const auto& path : candidates) {
        config.open(path);
        if (config.is_open()) {
            break;
        }
        config.clear();
    }
    if (!config.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(config, line)) {
        const size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        key.erase(std::remove(key.begin(), key.end(), '='), key.end());
        while (!key.empty() && std::isspace(
                                       static_cast<unsigned char>(key.front()))) {
            key.erase(key.begin());
        }
        while (!key.empty() && std::isspace(
                                       static_cast<unsigned char>(key.back()))) {
            key.pop_back();
        }

        const long parsed = strtol(value.c_str(), nullptr, 0);
        if (key == "enable" || key == "bms_heating_support") {
            bmsEnabled_ = parsed != 0;
        } else if (key == "background_heating_support") {
            bmsBackgroundSupported_ = parsed != 0;
        } else if (key == "cpu_mask") {
            bmsCpuMask_ = static_cast<int32_t>(parsed);
        } else if (key == "background_heating_cpu_mask") {
            bmsBackgroundCpuMask_ = static_cast<int32_t>(parsed);
        }
    }
}

int32_t Charger::chgFuncMapInit() {
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,
                        "funMapInit load charger set function");
    return 0;
}

int32_t Charger::chgDualCellsProtInit() {
    return 0;
}

ndk::ScopedAStatus Charger::chgExchangeMesgInit(int32_t* _aidl_return) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "chgExchangeMesgInit start");
    pthread_t thread = 0;
    const int rc = pthread_create(&thread, nullptr, chgExchangeThread, nullptr);
    if (rc == 0) {
        pthread_detach(thread);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "create chg_exchange_mesg_thread success");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "can't create chg_exchange_mesg_thread: %s",
                            strerror(rc));
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::chgExchangeSohMesgInit(int32_t* _aidl_return) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "chgExchangeSohMesgInit start");
    pthread_t thread = 0;
    const int rc = pthread_create(&thread, nullptr, uisohUpdateThread, nullptr);
    if (rc == 0) {
        pthread_detach(thread);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "create chg_update_uisoh_thread success");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "can't create chg_update_uisoh_thread: %s",
                            strerror(rc));
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::VolDividerIcWorkModeSet(const std::string& data,
                                                    int32_t* _aidl_return) {
    int32_t rc = nodeWrite("/proc/da9313/work_mode", data);
    if (rc == kChargerError) {
        rc = nodeWrite("/proc/oplus_charge_pump/work_mode", data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::getAcType(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kAcType, kAcTypeAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getBattAuthenticate(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryAuthenticate,
                                   kBatteryAuthenticateAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getBattPPSChgIng(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("ppsCharging");
    return ok();
}

ndk::ScopedAStatus Charger::getBattPPSChgPower(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("ppsChargePower");
    return ok();
}

ndk::ScopedAStatus Charger::getBattParamNoplug(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kProcBattParamNoplug);
    return ok();
}

ndk::ScopedAStatus Charger::getBattShortIcOtpStatus(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryShortIcOtp,
                                   kBatteryShortIcOtpAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getBattSubCurrent(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatterySubCurrent,
                                   kBatterySubCurrentAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getBattVoocChgIng(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryVoocIng, kBatteryVoocIngAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getBatteryVoltageNow(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kBatteryVoltageNow, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getFastCharge(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("fastCharge");
    return ok();
}

ndk::ScopedAStatus Charger::getParallelChgMosTestResult(
        int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kBatteryParallelMosTest, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyAcOnline(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kAcOnline, kAcOnlineAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryCC(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryCc, kBatteryCcAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryCurrentNow(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kBatteryCurrentNow, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryFcc(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("batteryFcc");
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryHmac(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcChargerHmac, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryLevel(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryChipSoc, kBatteryChipSocAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryNotify(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("notifyCode");
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryPchg(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcPassedChg, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryPchgResetCount(
        int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcPassedChgReset, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryRm(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("batteryRm");
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryShortFeature(
        int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryShortFeature,
                                   kBatteryShortFeatureAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryShortStatus(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryShortStatus,
                                   kBatteryShortStatusAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryStatus(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kBatteryStatus);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatteryTemp(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kBatteryTemp, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyChargeTech(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryChargeTech,
                                   kBatteryChargeTechAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyFastChgType(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kUsbFastChgType, kUsbFastChgTypeAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyInputCurrent(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcInputCurrent, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyOtgOnline(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("otgOnline");
    return ok();
}

ndk::ScopedAStatus Charger::getPsyOtgSwitch(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kUsbOtgSwitch, kUsbOtgSwitchAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyPcPortOnline(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kPcPortOnline, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyQGVbatDeviation(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcQgVbatDeviation, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getQgVbatDeviation(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcQgVbatDeviation, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyTypeOrientation(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kUsbTypeOrientation,
                                   kUsbTypeOrientationAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyUsbOnline(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kUsbOnline, kUsbOnlineAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyUsbStatus(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("usbStatus");
    return ok();
}

ndk::ScopedAStatus Charger::getPsyWirelessRX(std::string* _aidl_return) {
    *_aidl_return = readTrimmedOr(kProcWirelessRxPower);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyWirelessRxVersion(
        std::string* _aidl_return) {
    *_aidl_return = readTrimmedOr(kProcWirelessRxVersion);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyWirelessTX(std::string* _aidl_return) {
    *_aidl_return = readTrimmedOr(kProcWirelessTxPower);
    return ok();
}

ndk::ScopedAStatus Charger::getPsyWirelessTxVersion(
        std::string* _aidl_return) {
    *_aidl_return = readTrimmedOr(kProcWirelessTxVersion);
    return ok();
}

ndk::ScopedAStatus Charger::getQuickModeGain(std::string* _aidl_return) {
    const int32_t timeGain =
            readIntOr("/sys/class/oplus_chg/battery/"
                      "get_quick_mode_time_gain",
                      kChargerError);
    const int32_t percentGain =
            readIntOr("/sys/class/oplus_chg/battery/"
                      "get_quick_mode_percent_gain",
                      kChargerError);
    *_aidl_return = std::to_string(timeGain) + "+" + std::to_string(percentGain);
    return ok();
}

ndk::ScopedAStatus Charger::getReserveSocDebug(std::string* _aidl_return) {
    std::string value = readStringOr(kProcReserveSocDebug);
    if (value.empty()) {
        std::lock_guard<std::mutex> guard(lock_);
        value = reserveSocDebug_;
    }
    *_aidl_return = value;
    return ok();
}

ndk::ScopedAStatus Charger::getSmartChgMode(int32_t* _aidl_return) {
    std::lock_guard<std::mutex> guard(lock_);
    *_aidl_return = smartChgMode_;
    return ok();
}

ndk::ScopedAStatus Charger::getUIsohValue(int32_t* _aidl_return) {
    int32_t value = 0;
    {
        std::lock_guard<std::mutex> guard(lock_);
        value = uiSoh_;
    }

    auto soh = nodeReadFirstInt(kBatteryUiSoh, kBatterySoh);
    if (!soh || *soh <= 0) {
        soh = nodeReadFirstInt(kBatterySoh, kBatteryUiSoh);
    }
    if (soh && *soh > 0) {
        value = *soh;
        std::lock_guard<std::mutex> guard(lock_);
        uiSoh_ = value;
    } else if (value == 0) {
        value = gCachedUiSoh.load();
    }
    if (value == 0) {
        value = 100;
    }

    *_aidl_return = value;
    return ok();
}

ndk::ScopedAStatus Charger::getUisohDebugParameterInfo(
        std::string* _aidl_return) {
    std::lock_guard<std::mutex> guard(lock_);
    const int32_t uiSoh = uiSoh_ == 0 ? 100 : uiSoh_;
    const int32_t fcc = uiSohDebugValid_ ? uiSohDebugValues_[5]
                                         : getChargerInfoInt("batteryFcc");
    const int32_t cc = uiSohDebugValid_ ? uiSohDebugValues_[6]
                                        : readIntOr(kBatteryUiCc, 0);
    const int32_t temp = uiSohDebugValid_ ? uiSohDebugValues_[8]
                                          : readIntOr(kBatteryTemp, 0);
    const int32_t t1 = uiSohDebugValid_ ? uiSohDebugValues_[0] : 0;
    const int32_t t2 = uiSohDebugValid_ ? uiSohDebugValues_[1] : 0;
    const int32_t t3 = uiSohDebugValid_ ? uiSohDebugValues_[2] : 0;
    const int32_t wSoh = uiSohDebugValid_ ? uiSohDebugValues_[10] : uiSoh;
    const int32_t uiCc = uiSohDebugValid_ ? uiSohDebugValues_[11] : cc;
    std::ostringstream out;
    out << "ui_soh parameter data good,ui_soh=" << uiSoh << ",t1=" << t1
        << ",t2=" << t2 << ",t3=" << t3 << ",b_soh=" << uiSoh
        << ",b_fcc=" << fcc << ",soh_filter=0,b_cyclecount=" << cc
        << ",b_temp=" << temp << ",soh_buffer:"
        << ",fcc_buffer:"
        << "t4=0,w_soh=" << wSoh << ",ui_cc=" << uiCc;
    *_aidl_return = out.str();
    return ok();
}

ndk::ScopedAStatus Charger::getUsbInputCurrentNow(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kUsbInputCurrent, kUsbInputCurrentAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getUsbPrimalType(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kUsbPrimalType, kUsbPrimalTypeAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWiredOtgOnline(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcWiredOtgOnline, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessAdapterPower(int32_t* _aidl_return) {
    *_aidl_return = getChargerInfoInt("wlsAdapterPower");
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessCapacity(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kWirelessCapacity, kWirelessCapacityAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessChargePumpEn(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcWirelessChargePumpEn, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessCurrentNow(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kWirelessCurrent, kWirelessCurrentAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessDeviated(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kProcWirelessDeviated);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessOnline(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kWirelessOnline, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessPenPresent(int32_t* _aidl_return) {
    const int32_t value = readIntOr(kPencilPresent, kChargerError);
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,
                        "wireless pencil present status %d", value);
    *_aidl_return = value;
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessPtmcId(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kWirelessPtmcId, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessRXEnable(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcWirelessEnableRx, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessRealType(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kWirelessRealType, kWirelessRealTypeAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessTXEnable(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kProcWirelessEnableTx);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessUserSleepMode(
        int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcWirelessUserSleep, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getWirelessVoltageNow(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kWirelessVoltage, kWirelessVoltageAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::healthd_update_ui_soc_decimal(
        std::string* _aidl_return) {
    *_aidl_return = readStringOr(kProcUiSocDecimal);
    return ok();
}

ndk::ScopedAStatus Charger::nightstandby(int32_t status,
                                        int32_t* _aidl_return) {
    const std::string data = intWithNewline(status);
    int32_t rc = nodeWrite(kBatterySocAdjust, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kBatterySocAdjustAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::notifyScreenStatus(int32_t status,
                                               int32_t* _aidl_return) {
    {
        std::lock_guard<std::mutex> guard(lock_);
        screenStatus_ = status;
    }
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "notifyScreenStatus: %d",
                        status);
    *_aidl_return = 0;
    return ok();
}

ndk::ScopedAStatus Charger::setChargeEMMode(const std::string& data,
                                            int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kBatteryEmMode, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kBatteryEmModeAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setChargerControl(const std::string& data,
                                              int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcChargerControl, data);
    return ok();
}

ndk::ScopedAStatus Charger::setChargerCoolDown(const std::string& data,
                                               int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kBatteryCoolDown, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kBatteryCoolDownAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setChargerCriticalLog(
        const std::string& data, int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcChargerCriticalLog, data);
    return ok();
}

ndk::ScopedAStatus Charger::setChargerCycle(const std::string& data,
                                            int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcChargerCycle, data);
    return ok();
}

ndk::ScopedAStatus Charger::setChargerFactoryModeTest(
        const std::string& data, int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcFactoryMode, data);
    return ok();
}

ndk::ScopedAStatus Charger::setChargerLog(const std::string& data,
                                          int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcChargerLog, data);
    return ok();
}

ndk::ScopedAStatus Charger::setChgStatusToBcc(int32_t status,
                                              int32_t* _aidl_return) {
    {
        std::lock_guard<std::mutex> guard(lock_);
        bccStatus_ = status;
    }
    *_aidl_return = 0;
    return ok();
}

ndk::ScopedAStatus Charger::setCustomSelectChgMode(int32_t mode, bool enable,
                                                   int32_t* _aidl_return) {
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (enable) {
            chgUserMode_ = (mode & 1) != 0 ? 3 : 0;
            if ((mode & 2) != 0) {
                chgUserMode_ = 4;
            }
        } else {
            chgUserMode_ = 0;
        }
    }

    // The stock service sends a 0x208-byte tracking record.  Preserve the
    // header that identifies the "user selected charging mode" event.
    struct TrackCmd {
        int32_t cmd;
        int32_t type;
        uint8_t mode;
        uint8_t reserved[0x208 - 9];
    } __attribute__((packed));
    TrackCmd cmd = {};
    cmd.cmd = 4;
    cmd.type = 1;
    cmd.mode = static_cast<uint8_t>(enable ? 1 : 0);
    nodeWrite(kTrackHidl, std::string(reinterpret_cast<const char*>(&cmd),
                                      sizeof(cmd)));

    *_aidl_return = 0;
    return ok();
}

ndk::ScopedAStatus Charger::getCustomSelectChgMode(int32_t* _aidl_return) {
    std::lock_guard<std::mutex> guard(lock_);
    *_aidl_return = chgUserMode_;
    return ok();
}

ndk::ScopedAStatus Charger::setFastchgFwUpdate(const std::string& data,
                                               int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcFastchgFwUpdate, data);
    return ok();
}

ndk::ScopedAStatus Charger::setPsyMmiChgEn(const std::string& data,
                                           int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kBatteryMmiEn, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kBatteryMmiEnAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setPsyOtgSwitch(const std::string& data,
                                            int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kUsbOtgSwitch, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kUsbOtgSwitchAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setReserveSocDebug(const std::string& data,
                                               int32_t* _aidl_return) {
    {
        std::lock_guard<std::mutex> guard(lock_);
        reserveSocDebug_ = data;
    }
    *_aidl_return = nodeWrite(kProcReserveSocDebug, data);
    return ok();
}

ndk::ScopedAStatus Charger::setShipMode(const std::string& data,
                                        int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kBatteryShipMode, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kBatteryShipModeAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setSmartChgMode(const std::string& data,
                                            int32_t* _aidl_return) {
    const int32_t mode = atoi(data.c_str());
    {
        std::lock_guard<std::mutex> guard(lock_);
        if ((mode & 2) != 0) {
            smartChgMode_ = (smartChgMode_ & ~1) | (mode & 1) | 2;
        } else {
            smartChgMode_ = (smartChgMode_ & ~3) | (mode & 1);
        }
    }

    if ((mode & 2) != 0) {
        nodeWrite(kBatteryCurrentNow, std::string("0\n"));
    }

    // Stock returns 0x80000000 unconditionally from this path, even after a
    // successful update.  Keep that observable behavior.
    *_aidl_return = kChargerError;
    return ok();
}

ndk::ScopedAStatus Charger::setSmartCoolDown(int32_t coolDown,
                                             int32_t normalCoolDown,
                                             const std::string& pkgName,
                                             int32_t* _aidl_return) {
    if (coolDown < 0 || normalCoolDown < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "set cooldown is err");
        *_aidl_return = -1;
        return ok();
    }

    // The stock code caches normal_cool_down for the thermal controller and
    // writes the per-app cool_down value.  The node is tried first in the
    // power_supply class, then in oplus_chg.
    nodeWriteFirst(kBatteryCoolDown, kBatteryCoolDownAlt,
                   intWithNewline(coolDown));
    nodeWrite(kBatteryNormalCoolDown, intWithNewline(normalCoolDown));
    nodeWrite(kBatteryPkgName, packageLabel(pkgName));

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "set cooldown:%d,  normalCoolDown:%d", coolDown,
                        normalCoolDown);
    *_aidl_return = 0;
    return ok();
}

ndk::ScopedAStatus Charger::setTbattPwrOff(const std::string& data,
                                           int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcTbattPwrOff, data);
    return ok();
}

ndk::ScopedAStatus Charger::setUisohDebugInfo(const std::string& data,
                                              int32_t* _aidl_return) {
    if (data.size() > 256 || data.find("ui_soh debug data is good") ==
                                     std::string::npos) {
        *_aidl_return = data.size() > 256 ? -1 : -2;
        return ok();
    }

    std::array<int32_t, 12> values = {};
    const char* cursor = data.c_str();
    size_t index = 0;
    while (index < values.size()) {
        const char* separator = strchr(cursor, '=');
        if (separator == nullptr || separator[1] == '\0') {
            break;
        }
        cursor = separator + 1;
        values[index++] = atoi(cursor);
    }

    {
        std::lock_guard<std::mutex> guard(lock_);
        uiSohDebugValues_ = values;
        uiSohDebugValid_ = true;
        if (values[3] > 0 && values[3] <= 100) {
            uiSoh_ = values[3];
            gCachedUiSoh.store(values[3]);
        }
    }

    __android_log_print(
            ANDROID_LOG_INFO, "chg_exchange_soh_mesg",
            "[SET]debug_t1=%d, debug_t2=%d, debug_t3=%d, debug_uisoh=%d, "
            "debug_soh=%d, debug_fcc=%d, debug_cc=%d, debug_des=%d, "
            "debug_temp=%d, update_buffer_t=%d, w_soh=%d, debug_ui_cc=%d",
            values[0], values[1], values[2], values[3], values[4], values[5],
            values[6], values[7], values[8], values[9], values[10],
            values[11]);
    *_aidl_return = 0;
    return ok();
}

ndk::ScopedAStatus Charger::setUsbPrimalType(const std::string& data,
                                             int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kUsbPrimalType, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kUsbPrimalTypeAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessChargePumpEn(
        const std::string& data, int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcWirelessChargePumpEn, data);
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessFtmMode(const std::string& data,
                                               int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcWirelessFtmMode, data);
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessIconDelay(const std::string& data,
                                                 int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kWirelessIconDelay, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kWirelessIconDelayAlt, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessIdtAdcTest(const std::string& data,
                                                  int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcWirelessIdtAdcTest, data);
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessPenSoc(const std::string& data,
                                              int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kPencilRxSoc, data);
    if (rc == kChargerError) {
        rc = nodeWrite(kWlsPenSoc, data);
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessRXEnable(const std::string& data,
                                                int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcWirelessEnableRx, data);
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessTXEnable(const std::string& data,
                                                int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcWirelessEnableTx, data);
    return ok();
}

ndk::ScopedAStatus Charger::setWirelessUserSleepMode(
        const std::string& data, int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kProcWirelessUserSleep, data);
    return ok();
}

ndk::ScopedAStatus Charger::setWlsThirdPartitionInfo(
        const std::string& data, int32_t* _aidl_return) {
    if (data.size() > 255) {
        *_aidl_return = -1;
        return ok();
    }
    std::array<char, kWlsThirdSize> record = {};
    memcpy(record.data(), data.data(), data.size());
    const off_t offset = partitionOffset(kWlsThirdOffsetUfs,
                                         kWlsThirdOffsetEmmc);
    *_aidl_return = writePartition(offset, record.data(), record.size())
                            ? 0
                            : -1;
    return ok();
}

ndk::ScopedAStatus Charger::setPsySlowChgEn(const std::string& data,
                                            int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kBatterySlowChgEn, data);
    return ok();
}

ndk::ScopedAStatus Charger::setBatteryLogPush(const std::string& data,
                                              int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kBattLogPush, data);
    return ok();
}

ndk::ScopedAStatus Charger::setChargingModeInGsmCall(
        const std::string& data, int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kBatteryGsmCall, data);
    return ok();
}

ndk::ScopedAStatus Charger::setChgRusConfig(const std::string& data,
                                            int32_t* _aidl_return) {
    if (data.size() > kRusConfigSize - 4) {
        __android_log_print(ANDROID_LOG_ERROR, "chg_rus_config",
                            "len %d too large", static_cast<int>(data.size()));
        *_aidl_return = 0;
        return ok();
    }
    std::array<char, kRusConfigSize> record = {};
    memcpy(record.data(), "crus", 4);
    memcpy(record.data() + 4, data.data(), data.size());
    const off_t offset = partitionOffset(kRusConfigOffsetUfs,
                                         kRusConfigOffsetEmmc);
    writePartition(offset, record.data(), record.size());
    *_aidl_return = 0;
    return ok();
}

ndk::ScopedAStatus Charger::setChgOlcConfig(const std::string& data,
                                            int32_t* _aidl_return) {
    if (data.size() > 272) {
        *_aidl_return = -1;
        return ok();
    }
    *_aidl_return = nodeWrite(kChgOlcConfig, data);
    return ok();
}

ndk::ScopedAStatus Charger::setSuperEnduranceStatus(
        const std::string& data, int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kSuperEnduranceStatus, data);
    return ok();
}

ndk::ScopedAStatus Charger::setSuperEnduranceCount(
        const std::string& data, int32_t* _aidl_return) {
    *_aidl_return = nodeWrite(kSuperEnduranceCount, data);
    return ok();
}

ndk::ScopedAStatus Charger::setBobStatus(const std::string& data,
                                         int32_t* _aidl_return) {
    int32_t rc = nodeWrite(kBobStatus, data);
    if (rc == 0) {
        // BoostBypassInfo::setVout() is a no-op when the bypass hardware is
        // not present; the status node is the authoritative interface.
        const int32_t vout = atoi(data.c_str());
        if (vout > 0) {
            nodeWrite("/sys/class/oplus_chg/common/byb_vout",
                      std::to_string(vout));
        }
    }
    *_aidl_return = rc;
    return ok();
}

ndk::ScopedAStatus Charger::getChargerControl(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kProcChargerControl);
    return ok();
}

ndk::ScopedAStatus Charger::getChargerCoolDown(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryCoolDown, kBatteryCoolDownAlt,
                                   kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getChargerCriticalLog(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcChargerCriticalLog, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getChargerIdVolt(int32_t* _aidl_return) {
    *_aidl_return = readIntFirstOr(kBatteryChargerIdVolt,
                                   kBatteryChargerIdVoltAlt, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getChargerLog(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcChargerLog, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getDevinfoFastchg(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kProcDevinfoFastchg);
    return ok();
}

ndk::ScopedAStatus Charger::getBattGaugeInfo(std::string* _aidl_return) {
    auto value = nodeReadRaw(kBatteryGaugeInfo, 0x3ff);
    *_aidl_return = value.value_or("");
    if (!value) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "getBatteryBalanceInfo: failed");
    }
    return ok();
}

ndk::ScopedAStatus Charger::getPsyBatterySN(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kBatterySn);
    return ok();
}

ndk::ScopedAStatus Charger::getChgOlcConfig(std::string* _aidl_return) {
    *_aidl_return = readStringOr(kChgOlcConfig);
    return ok();
}

ndk::ScopedAStatus Charger::getChargingModeInGsmCall(
        int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kBatteryGsmCall, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getCpVbatDeviation(int32_t* _aidl_return) {
    *_aidl_return = readIntOr(kProcCpVbatDeviation, kChargerError);
    return ok();
}

ndk::ScopedAStatus Charger::getBccExpStatus(int32_t* _aidl_return) {
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (bccExceptionScene_ == 0) {
            bccExceptionScene_ = readIntOr(kBatteryBccException, 0);
        }
        *_aidl_return = bccExceptionScene_;
    }
    return ok();
}

ndk::ScopedAStatus Charger::getBccCsvData(std::string* _aidl_return) {
    if (access("/sys/class/oplus_chg/battery/bcc_parms", F_OK) != 0) {
        __android_log_print(ANDROID_LOG_WARN, "BatteryChargeControl",
                            "bcc_get_csv_data: bcc is NULL");
        *_aidl_return = "";
        return ok();
    }
    const time_t now = time(nullptr);
    struct tm tmValue = {};
    localtime_r(&now, &tmValue);

    const int32_t current0 = readIntOr(kBatteryCurrentNow, 0);
    const int32_t current1 =
            readIntFirstOr(kBatterySubCurrent, kBatterySubCurrentAlt, 0);
    const int32_t coolDown =
            readIntFirstOr(kBatteryCoolDown, kBatteryCoolDownAlt, 0);
    const int32_t shellTemp = readIntOr(
            "/sys/class/oplus_chg/battery/shell_temp", 0);
    const int32_t chipSoc =
            readIntFirstOr(kBatteryChipSoc, kBatteryChipSocAlt, 0);
    const int32_t capacity = readIntOr(kBatteryCapacity, 0);
    const int32_t voltage0 = readIntOr(kBatteryVoltageNow, 0);
    const int32_t temperature0 = readIntOr(kBatteryTemp, 0);
    const int32_t bccCurrent = readIntOr(kBatteryBccCurrent, 0);
    const std::string status = readTrimmedOr(kBatteryStatus);

    char nowText[64] = {};
    snprintf(nowText, sizeof(nowText), "%d_%02d_%02d-%02d:%02d:%02d",
             tmValue.tm_year + 1900, tmValue.tm_mon + 1, tmValue.tm_mday,
             tmValue.tm_hour, tmValue.tm_min, tmValue.tm_sec);

    std::ostringstream out;
    out << "bcc_time_now=" << nowText << "\n";
    out << "current_0=" << current0 << "\n";
    out << "current_1=" << current1 << "\n";
    out << "cool_down_level=" << coolDown << "\n";
    out << "shell_temp=" << (shellTemp / 100) << "\n";
    out << "chip_soc=" << chipSoc << "\n";
    out << "capacity=" << capacity << "\n";
    out << "voltage_0=" << voltage0 << "\n";
    out << "temperature_0=" << temperature0 << "\n";
    out << "soc_ext_0=0\n";
    out << "max_current_0=0\n";
    out << "min_current_0=0\n";
    out << "dod0_0=0\n";
    out << "dod0_passed_q_0=0\n";
    out << "qmax_0=0\n";
    out << "error_code_0=0\n";
    out << "anode_potential_0=0\n";
    out << "bcc_current=" << bccCurrent << "\n";
    out << "status=" << status << "\n";
    out << "voltage_1=0\n";
    out << "temperature_1=0\n";
    out << "soc_ext_1=0\n";
    out << "max_current_1=0\n";
    out << "min_current_1=0\n";
    out << "dod0_1=0\n";
    out << "dod0_passed_q_1=0\n";
    out << "qmax_1=0\n";
    out << "error_code_1=0\n";
    out << "anode_potential_1=0\n";
    out << "charging_flag=" << (batteryStatusValue() == 2 ? 1 : 0) << "\n";

    std::string value = out.str();
    if (value.size() >= 0x400) {
        value.resize(0x3ff);
    }
    *_aidl_return = value;
    return ok();
}

ndk::ScopedAStatus Charger::getBmsHeatingStatus(int32_t* _aidl_return) {
    initializeBmsConfig();

    // bms_heat_temp_compensation is non-zero while the BMS heating path is
    // actively compensating the gauge temperature.
    const int32_t compensation = readIntOr(kBatteryBmsTemp, 0);
    int32_t status = 0;
    if (bmsEnabled_ && compensation > 0) {
        bool screenOff = false;
        {
            std::lock_guard<std::mutex> guard(lock_);
            screenOff = screenStatus_ == 0;
        }
        status = (screenOff && bmsBackgroundSupported_) ? 3 : 1;
    }
    {
        std::lock_guard<std::mutex> guard(lock_);
        bmsStatus_ = status;
    }
    *_aidl_return = status;
    return ok();
}

ndk::ScopedAStatus Charger::getBmsHeatingRunningStatus(
        std::string* _aidl_return) {
    initializeBmsConfig();
    int32_t status = 0;
    getBmsHeatingStatus(&status);
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!bmsEnabled_ || status == 0) {
            *_aidl_return = "";
            return ok();
        }
        const int32_t mask = (status == 3 && bmsBackgroundCpuMask_ != 0)
                                     ? bmsBackgroundCpuMask_
                                     : bmsCpuMask_;
        char buffer[64] = {};
        snprintf(buffer, sizeof(buffer), "cpu_mask: 0x%x\ncpu_thread:\n",
                 static_cast<unsigned>(mask));
        *_aidl_return = buffer;
    }
    return ok();
}

ndk::ScopedAStatus Charger::testKitGetFeatureNum(int32_t* _aidl_return) {
    KernelTest test;
    const int openRc = test.open();
    if (openRc < 0) {
        *_aidl_return = openRc;
        return ok();
    }
    uint32_t number = 0;
    const int rc = test.getFeatureNum(&number);
    *_aidl_return = rc < 0 ? rc : static_cast<int32_t>(number);
    return ok();
}

ndk::ScopedAStatus Charger::testKitGetFeatureName(
        int32_t index, std::string* _aidl_return) {
    KernelTest test;
    if (test.open() < 0) {
        *_aidl_return = "";
        return ok();
    }
    *_aidl_return = test.getName(static_cast<uint32_t>(index));
    return ok();
}

ndk::ScopedAStatus Charger::testKitGetFeatureList(
        std::string* _aidl_return) {
    KernelTest test;
    if (test.open() < 0) {
        *_aidl_return = "";
        return ok();
    }
    *_aidl_return = test.getFeatureList();
    return ok();
}

ndk::ScopedAStatus Charger::testKitFeatureTest(
        int32_t index, testKitFeatureTestResult* _aidl_return) {
    KernelTest test;
    const int openRc = test.open();
    if (openRc < 0) {
        _aidl_return->ret = openRc;
        _aidl_return->str = "";
        return ok();
    }
    std::string result;
    _aidl_return->ret =
            test.runTest(static_cast<uint32_t>(index), &result);
    _aidl_return->str = result;
    return ok();
}

ndk::ScopedAStatus Charger::updateUiSohToPartion(int32_t* _aidl_return) {
    int32_t soh = 0;
    getUIsohValue(&soh);
    if (soh <= 0) {
        *_aidl_return = -1;
        return ok();
    }

    const off_t offset = partitionOffset(kSohOffsetUfs, kSohOffsetEmmc);
    std::vector<char> record(0x100, 0);
    const std::string text = "ui_soh=" + std::to_string(soh);
    std::memcpy(record.data(), text.data(),
                std::min(text.size(), record.size()));
    const bool written = writePartition(offset, record.data(), record.size());
    if (!written) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                            "[SAVE] ui_soh partition write failed");
    }
    *_aidl_return = written ? 0 : -1;
    return ok();
}

ndk::ScopedAStatus Charger::queryWlsPencilInfo(
        std::string* _aidl_return) {
    std::ostringstream out;

    auto appendIfReadable = [&](const char* path, const char* key,
                                bool newline) -> bool {
        if (access(path, R_OK) != 0) {
            return false;
        }
        auto value = nodeReadTrimmed(path);
        if (!value) {
            return false;
        }
        out << key << "=" << *value;
        if (newline) {
            out << "\n";
        }
        return true;
    };

    if (!appendIfReadable(kPencilBleMac, "ble_pencil_addr", true)) {
        appendIfReadable(kProcWirelessMac, "ble_pencil_addr", true);
    }

    if (!appendIfReadable(kPencilPingTime, "ping_time", true)) {
        appendIfReadable(kWlsPenPingTime, "ping_time", true);
    }

    if (!appendIfReadable(kPencilTxStatus, "tx_status", true)) {
        appendIfReadable(kProcWirelessEnableTx, "tx_status", true);
    }

    if (!appendIfReadable(kPencilTxCurrent, "tx_current", true)) {
        appendIfReadable(kWirelessCurrent, "tx_current", true);
    }

    if (!appendIfReadable(kPencilTxVoltage, "tx_voltage", true)) {
        appendIfReadable(kWirelessVoltage, "tx_voltage", true);
    }

    appendIfReadable(kPencilFwVersion, "fm_version", true);
    appendIfReadable(kPencilWirelessIcId, "wireless_ic_id", false);

    *_aidl_return = out.str();
    return ok();
}

ndk::ScopedAStatus Charger::queryChargeInfo(std::string* _aidl_return) {
    std::ostringstream out;

    const int32_t bccExp = bccExceptionScene_ != 0
                                   ? bccExceptionScene_
                                   : readIntOr(kBatteryBccException, 0);
    out << "bcc_exp_status=" << bccExp << "\n";
    out << "battery_capacity=" << readIntOr(kBatteryCapacity, 0) << "\n";
    out << "battery_voltage_now=" << readIntOr(kBatteryVoltageNow, 0) << "\n";
    out << "battery_voltage_min=" << readIntOr(kBatteryVoltageMin, 0) << "\n";
    out << "battery_temp=" << readIntOr(kBatteryTemp, 0) << "\n";
    out << "battery_current_now=" << readIntOr(kBatteryCurrentNow, 0) << "\n";
    out << "battery_charge_now=" << readIntOr(kBatteryChargeNow, 0) << "\n";
    out << "battery_sub_current="
        << readIntFirstOr(kBatterySubCurrent, kBatterySubCurrentAlt, 0)
        << "\n";
    out << "usb_fast_chg_type="
        << readIntFirstOr(kUsbFastChgType, kUsbFastChgTypeAlt, 0) << "\n";
    out << "battery_voocchg_ing="
        << readIntFirstOr(kBatteryVoocIng, kBatteryVoocIngAlt, 0) << "\n";
    out << "battery_ppschg_ing=" << readIntOr(kBatteryPpsIng, 0) << "\n";
    out << "battery_ppschg_power=" << readIntOr(kBatteryPpsPower, 0) << "\n";
    out << "usb_input_current_now="
        << readIntFirstOr(kUsbInputCurrent, kUsbInputCurrentAlt, 0) << "\n";
    out << "battery_short_ic_otp_status="
        << readIntFirstOr(kBatteryShortIcOtp, kBatteryShortIcOtpAlt, 0)
        << "\n";
    out << "battery_authenticate="
        << readIntFirstOr(kBatteryAuthenticate, kBatteryAuthenticateAlt, 0)
        << "\n";
    out << "battery_bqfs_status=" << readIntOr(kBatteryBqfs, 0) << "\n";
    out << "gauge_ibat_deviation="
        << readIntOr(kProcGaugeIbatDeviation, 0) << "\n";
    out << "battery_charge_technology="
        << readIntFirstOr(kBatteryChargeTech, kBatteryChargeTechAlt, 0)
        << "\n";
    out << "parallel_chg_mos_status=" << readIntOr(kBatteryParallelMos, 0)
        << "\n";
    out << "wireless_current_now="
        << readIntFirstOr(kWirelessCurrent, kWirelessCurrentAlt, 0) << "\n";
    out << "wireless_rx_version=" << readTrimmedOr(kProcWirelessRxVersion)
        << "\n";
    out << "wireless_tx_version=" << readTrimmedOr(kProcWirelessTxVersion)
        << "\n";
    out << "wireless_idt_adc_test="
        << readIntOr(kProcWirelessIdtAdcTest, 0) << "\n";
    out << "wireless_enable_tx=" << enableTxValue() << "\n";
    out << "battery_status=" << batteryStatusValue() << "\n";
    out << "wireless_voltage_now="
        << readTrimmedFirstOr(kWirelessVoltage, kWirelessVoltageAlt) << "\n";
    out << "wireless_real_type="
        << readTrimmedFirstOr(kWirelessRealType, kWirelessRealTypeAlt) << "\n";
    out << "wireless_charger_type=" << wirelessChargerType() << "\n";
    out << "wireless_charge_pump_en="
        << readTrimmedOr(kProcWirelessChargePumpEn) << "\n";
    out << "wireless_deviated=" << readTrimmedOr(kProcWirelessDeviated)
        << "\n";

    int32_t noplugTemp = 0;
    int32_t noplugVmax = 0;
    int32_t noplugVmin = 0;
    if (auto noplug = nodeReadTrimmed(kProcBattParamNoplug)) {
        sscanf(noplug->c_str(), "%d %d %d", &noplugTemp, &noplugVmax,
               &noplugVmin);
    }
    out << "battery_temp_not_plug=" << noplugTemp << "\n";
    out << "battery_voltage_max_not_plug=" << noplugVmax << "\n";
    out << "battery_voltage_min_not_plug=" << noplugVmin << "\n";

    int32_t dualSupport = 0;
    int32_t dualVbat = 0;
    int32_t dualBuck = 0;
    int32_t dualTemp = 0;
    if (auto dual = nodeReadTrimmed(kBatteryDualChan)) {
        sscanf(dual->c_str(), "%d %d %d %d", &dualSupport, &dualVbat,
               &dualBuck, &dualTemp);
    }
    out << "dual_chan_support=" << dualSupport << "\n";
    out << "dual_chan_vbat_status=" << dualVbat << "\n";
    out << "dual_chan_buck_status=" << dualBuck << "\n";
    out << "dual_chan_temp_range_status=" << dualTemp << "\n";

    // Enumerate the four power_supply classes that the framework cares about.
    if (DIR* dir = opendir("/sys/class/power_supply")) {
        while (dirent* entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (name != "ac" && name != "battery" && name != "usb" &&
                name != "wireless") {
                continue;
            }
            const std::string onlinePath =
                    "/sys/class/power_supply/" + name + "/online";
            auto online = nodeReadInt(onlinePath);
            if (!online || *online != 1) {
                continue;
            }
            const std::string typePath =
                    "/sys/class/power_supply/" + name + "/type";
            const std::string type = readTrimmedOr(typePath);
            if (type.empty()) {
                continue;
            }

            bool isAc = type == "UPS" || type == "Mains" ||
                        type == "USB_DCP" || type == "USB_HVDCP" ||
                        type == "USB_ACA" || type == "USB_C" ||
                        type == "USB_PD" || type == "USB" ||
                        type == "USB_PD_DRP";
            bool isUsb = type == "USB_CDP" || type == "USB_PD_DRP" ||
                         type == "USB_HVDCP" || type == "USB_HVDCP_3" ||
                         type == "9" || type == "10";
            bool isWireless = type == "Wireless";

            if (isAc) {
                out << "chargerAcOnline=true\n";
            } else if (isUsb) {
                out << "chargerUSBOnline=true\n";
            } else if (isWireless) {
                out << "chargerWirelessOnline=true\n";
            } else {
                continue;
            }
            out << "charger_type=" << type << "\n";
        }
        closedir(dir);
    }

    if (auto control = nodeReadString(kProcChargerControl)) {
        out << "oplus_chg=" << *control << "\n";
    }
    if (auto ctl = nodeReadString(kProcChargerCtl)) {
        out << "charge_ctl=" << *ctl << "\n";
    }

    out << "bob_status=" << readIntOr(kBobStatus, 0) << "\n";
    out << "bob_status_reg=" << readIntOr(kBobStatusReg, 0) << "\n";
    out << "ttf_info="
        << readIntOr("/sys/class/power_supply/battery/time_to_full_now", 0)
        << ",0\n";
    out << "anti_exp_info=0,0,0\n";
    out << "bsl_data=\n";
    out << "eis_data=\n";
    out << "battery_type_str=" << readTrimmedOr(kBatteryType) << "\n";
    out << "batt_chemID=" << readTrimmedOr(kBatteryChemId) << "\n";

    int32_t uiSoh = 0;
    getUIsohValue(&uiSoh);
    out << "battery_uisoh=" << uiSoh << "\n";
    out << "battery_uisoh_is_100=" << (uiSoh == 100 ? 1 : 0) << "\n";
    out << "battery_realsoh=" << readIntFirstOr(kBatterySoh, kBatteryUiSoh,
                                               100)
        << "\n";

    *_aidl_return = out.str();
    return ok();
}

namespace {
std::string gMtkEyeDiagramParams;
}

ndk::ScopedAStatus Charger::setChgConfig(ChgFuncFlag flag,
                                         const std::string& extra,
                                         int32_t callname,
                                         int32_t* _aidl_return) {
    const int32_t flagValue = static_cast<int32_t>(flag);
    std::string error;

    switch (flagValue) {
        case 5: {  // SET_CHG_UP_LIMIT_VALUE
            std::string text;
            std::vector<int32_t> args;
            extraParse(extra, &text, &args);
            if (args.size() < 4) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "---setChgUpLimitValue arg_arr error!!!!");
                *_aidl_return = 0;
                return ok();
            }
            {
                std::lock_guard<std::mutex> guard(lock_);
                for (size_t i = 0; i < 4; ++i) {
                    chgUpLimit_[i] = args[i];
                }
                chgUpLimit_[4] = callname;
            }
            char buffer[64] = {};
            snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", args[0],
                     args[1], args[2], args[3], callname);
            nodeWrite(kChgUpLimit, buffer);
            *_aidl_return = 0;
            return ok();
        }
        case 11: {  // SET_BATT_FIRST_USAGE_DATE
            const std::string date = extra;
            int year = 0;
            int month = 0;
            int day = 0;
            if (sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3 ||
                year < 2000 || year > 2100 || month < 1 || month > 12 ||
                day < 1 || day > 31) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "get an invalid date.");
                *_aidl_return = -1;
                return ok();
            }
            nodeWrite(kBatteryFirstUsageDate, date);
            *_aidl_return = 0;
            return ok();
        }
        case 12: {  // CLEAR_BATT_ECO_DATA
            std::lock_guard<std::mutex> guard(lock_);
            uiSoh_ = 100;
            *_aidl_return = 0;
            return ok();
        }
        case 13: {  // SET_RECHG_SOC_EN
            std::string text;
            std::vector<int32_t> args;
            extraParse(extra, &text, &args);
            if (args.size() < 2) {
                *_aidl_return = -1;
                return ok();
            }
            const std::string value =
                    std::to_string(args[0]) + std::to_string(args[1]);
            *_aidl_return = nodeWriteCount(kBatteryRechgSoc, value);
            return ok();
        }
        case 14: {  // SET_CHG_UP_LIMIT_STATE
            std::string text;
            std::vector<int32_t> args;
            extraParse(extra, &text, &args);
            if (args.size() < 2) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "---setChgUpLimitState arg_arr error!!!!");
                *_aidl_return = 0;
                return ok();
            }
            const int32_t effectiveCallname = callname == 2 ? 10 : callname;
            char buffer[64] = {};
            snprintf(buffer, sizeof(buffer), "%d,%d,1,%d,%d\n", args[0],
                     args[1], args[1], effectiveCallname);
            nodeWrite(kChgUpLimit, buffer);
            *_aidl_return = 0;
            return ok();
        }
        case 19: {  // SET_ANTI_EXPANSION_INFO
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "[ANTI_EXPANSION] %s", extra.c_str());
            *_aidl_return = 0;
            return ok();
        }
        case 22: {  // WIRELESS_PENCIL_QCALI
            nodeWrite(kPencilQcali, "1\n");
            *_aidl_return = 0;
            return ok();
        }
        case 25: {  // SET_CHG_BYPASS
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite(kPlc,
                                      payload + "|callname=" +
                                              std::to_string(callname));
            return ok();
        }
        case 27: {  // SET_RX_DISABLE
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite(kRxDisable,
                                      payload + "callname=" +
                                              std::to_string(callname));
            return ok();
        }
        case 32: {  // SET_CHG_PATH
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite("/sys/class/oplus_chg/battery/chg_path",
                                      payload + "callname=" +
                                              std::to_string(callname));
            return ok();
        }
        case 34: {  // SET_BT_INFO
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite(kBtInfo, payload);
            return ok();
        }
        case 36: {  // SET_REVERSE_CHG_INFO
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite(kReverseChgInfo, payload);
            return ok();
        }
        case 38: {  // SET_WLSPEN_INFO
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite(kWlsPenInfo,
                                      payload + "callname=" +
                                              std::to_string(callname));
            return ok();
        }
        case 39: {  // SET_WLSPEN_DISCHG_SOC
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite(kWlsPenDischgSoc,
                                      payload + "callname=" +
                                              std::to_string(callname));
            return ok();
        }
        case 41: {  // SET_PENCIL_TX_CHARGE_DISABLE
            const std::string payload = extraTail(extra);
            if (payload.empty()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                    "Parsed data is empty or invalid");
                *_aidl_return = kChargerError;
                return ok();
            }
            *_aidl_return = nodeWrite(kPencilTxChargeDisable,
                                      payload + "callname=" +
                                              std::to_string(callname));
            return ok();
        }
        default:
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "chgSetFuncMap ERR, Flag corresponding "
                                "function is not loaded");
            *_aidl_return = -1;
            (void)error;
            return ok();
    }
}

ndk::ScopedAStatus Charger::getChgConfig(ChgFuncFlag flag,
                                         const std::string& extra,
                                         int32_t callname,
                                         std::string* _aidl_return) {
    const int32_t flagValue = static_cast<int32_t>(flag);
    std::string value;
    (void)callname;
    (void)extra;

    switch (flagValue) {
        case 0:
            value = getChargerInfoString("uiIconType");
            if (value.empty()) {
                value = "0";
            }
            break;
        case 1:
            value = getChargerInfoString("uiPower");
            if (value.empty()) {
                value = "0";
            }
            break;
        case 2:
            value = std::to_string(readIntOr(kDevicePower, kChargerError));
            break;
        case 3:
            value = std::to_string(readIntOr(kAdapterPower, kChargerError));
            break;
        case 4:
            value = getChargerInfoString("cpaPower");
            if (value.empty()) {
                value = "0";
            }
            break;
        case 6:
            value = getChargerInfoString("isSupportSpeedCharge");
            if (value.empty()) {
                value = "0";
            }
            break;
        case 7:
            // The stock getter map does not contain GET_BATT_EXPANSION_STATE.
            value.clear();
            break;
        case 8:
            value = std::to_string(readIntOr(kBatteryEcoDesign, kChargerError));
            break;
        case 9:
            value = readTrimmedOr(kBatteryManuDate);
            break;
        case 10:
            value = readTrimmedOr(kBatteryFirstUsageDate);
            break;
        case 15:
            value = std::to_string(pmVph());
            break;
        case 16:
            value = std::to_string(pmVbat());
            break;
        case 17:
            value = std::to_string(readIntOr(kDeepDischgCounts, kChargerError));
            break;
        case 18:
            // Stock getAntiExpansionInfo() intentionally returns an empty
            // string; the real data is exported through the tracking node.
            value.clear();
            break;
        case 20:
            value = std::to_string(readIntOr(kBatteryUiCc, kChargerError));
            break;
        case 21:
            value = readTrimmedOr(kBatteryType);
            break;
        case 22:
            value = readTrimmedOr(kPencilQcali);
            break;
        case 23:
            value = std::to_string(readIntOr(kBatteryGaugeType, kChargerError));
            break;
        case 24:
            value = getChargerInfoString("bypassChgStatus");
            if (value.empty()) {
                value = "0";
            }
            break;
        case 26:
            value = std::to_string(readIntOr(kRxDisable, kChargerError));
            break;
        case 28:
            value = batterySecIcTestStatus(extra);
            break;
        case 29:
        case 30:
            value = getChargerInfoJson(extra);
            break;
        case 31: {
            char* end = nullptr;
            const long parsed = strtol(extra.c_str(), &end, 10);
            if (end == extra.c_str() || (end != nullptr && *end != '\0')) {
                *_aidl_return = "";
                return ndk::ScopedAStatus::fromExceptionCode(
                        EX_ILLEGAL_ARGUMENT);
            }
            const bool supported = parsed >= 0 && parsed <= 41 && parsed != 7;
            value = supported ? "true" : "false";
            break;
        }
        case 33:
            value = getChargerInfoString("CarC");
            if (value.empty()) {
                value = "0";
            }
            break;
        case 35:
            value = std::to_string(
                    readIntOr("/sys/class/oplus_chg/battery/bdd_leak_current",
                              0));
            break;
        case 37:
            value = readTrimmedOr(kReverseChgInfo);
            break;
        case 40:
            value = readTrimmedOr(kUsbVdmInfo);
            break;
        default:
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "chgGetFuncMap ERR, Flag corresponding "
                                "function is not loaded");
            value.clear();
            break;
    }

    *_aidl_return = value;
    return ok();
}

ndk::ScopedAStatus Charger::setUsbEyeDiagram(
        int32_t model, const std::string& eyeDiagram,
        bool isDefaultEyeDiagram, int32_t* _aidl_return) {
    if (model != 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "setUsbEyeDiagram, Currently in usb device mode, "
                            "the model is not supported");
        *_aidl_return = 0;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    const std::vector<std::string> tokens = parseEyeDiagram(eyeDiagram);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "setUsbEyeDiagram, usbEyeDiagrams size: %d",
                        static_cast<int>(tokens.size()));
    if (tokens.size() < 2) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "setUsbEyeDiagram, usb eye diagrams parse error");
        *_aidl_return = -1;
        return ok();
    }

    const std::string& first = tokens[0];
    if (first != "qcom" && first != "mtk" && first != "unknown#") {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "setUsbEyeDiagram, usb eye diagrams parse error");
        *_aidl_return = -1;
        return ok();
    }

    const std::string platform = currentPlatform();
    int32_t result = 0;
    if (platform == "qcom") {
        result = qcomEyeDiagramSetting(tokens, isDefaultEyeDiagram);
    } else {
        gMtkEyeDiagramParams = eyeDiagram;
        result = mtkEyeDiagramSetting(tokens, isDefaultEyeDiagram);
    }

    *_aidl_return = result;
    return ok();
}

ndk::ScopedAStatus Charger::getUsbCurrentEyeDiagram(
        int32_t model, std::string* _aidl_return) {
    if (model != 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "getUsbCurrentEyeDiagram, Currently in usb device "
                            "mode, the model is not supported");
        *_aidl_return = "";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (currentPlatform() == "qcom") {
        *_aidl_return = getQcomEyeDiagram();
    } else {
        std::string value = getMtkEyeDiagram();
        if (value != gMtkEyeDiagramParams) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "getMtkEyeDiagram is not equal curr_params");
            value = gMtkEyeDiagramParams;
        }
        *_aidl_return = value;
    }
    return ok();
}

int registerChargerService() {
    auto charger = ndk::SharedRefBase::make<Charger>();
    if (charger == nullptr) {
        return -1;
    }

    int32_t ignored = 0;
    charger->chgExchangeMesgInit(&ignored);
    charger->chgExchangeSohMesgInit(&ignored);
    charger->notifyScreenStatus(0, &ignored);
    charger->nightstandby(0, &ignored);

    const std::string instance =
            std::string(ICharger::descriptor) + "/default";
    return AServiceManager_addService(charger->asBinder().get(),
                                      instance.c_str());
}

}  // namespace aidl::vendor::oplus::hardware::charger
