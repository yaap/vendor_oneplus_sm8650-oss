/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/oplus/hardware/charger/BnCharger.h>
#include <aidl/vendor/oplus/hardware/charger/ChgFuncFlag.h>
#include <aidl/vendor/oplus/hardware/charger/ICharger.h>
#include <aidl/vendor/oplus/hardware/charger/testKitFeatureTestResult.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

namespace aidl::vendor::oplus::hardware::charger {

class Charger : public BnCharger {
  public:
    Charger();
    ~Charger() override;

    ndk::ScopedAStatus VolDividerIcWorkModeSet(
            const std::string& data, int32_t* _aidl_return) override;
    ndk::ScopedAStatus chgExchangeMesgInit(int32_t* _aidl_return) override;
    ndk::ScopedAStatus chgExchangeSohMesgInit(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getAcType(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBattAuthenticate(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBattPPSChgIng(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBattPPSChgPower(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBattParamNoplug(std::string* _aidl_return) override;
    ndk::ScopedAStatus getBattShortIcOtpStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBattSubCurrent(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBattVoocChgIng(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBatteryVoltageNow(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBccCsvData(std::string* _aidl_return) override;
    ndk::ScopedAStatus getBccExpStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getBmsHeatingRunningStatus(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus getBmsHeatingStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getChargerControl(std::string* _aidl_return) override;
    ndk::ScopedAStatus getChargerCoolDown(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getChargerCriticalLog(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getChargerIdVolt(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getChargerLog(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getCustomSelectChgMode(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getDevinfoFastchg(std::string* _aidl_return) override;
    ndk::ScopedAStatus getFastCharge(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getParallelChgMosTestResult(
            int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyAcOnline(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryCC(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryCurrentNow(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryFcc(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryHmac(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryLevel(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryNotify(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryPchg(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryPchgResetCount(
            int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryRm(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryShortFeature(
            int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryShortStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryStatus(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatteryTemp(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyChargeTech(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyFastChgType(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyInputCurrent(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyOtgOnline(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyOtgSwitch(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyPcPortOnline(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyQGVbatDeviation(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyTypeOrientation(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyUsbOnline(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyUsbStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyWirelessRX(std::string* _aidl_return) override;
    ndk::ScopedAStatus getPsyWirelessRxVersion(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus getPsyWirelessTX(std::string* _aidl_return) override;
    ndk::ScopedAStatus getPsyWirelessTxVersion(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus getQgVbatDeviation(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getQuickModeGain(std::string* _aidl_return) override;
    ndk::ScopedAStatus getReserveSocDebug(std::string* _aidl_return) override;
    ndk::ScopedAStatus getSmartChgMode(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getUIsohValue(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getUisohDebugParameterInfo(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus getUsbInputCurrentNow(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getUsbPrimalType(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWiredOtgOnline(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessAdapterPower(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessCapacity(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessChargePumpEn(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessCurrentNow(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessDeviated(std::string* _aidl_return) override;
    ndk::ScopedAStatus getWirelessOnline(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessPenPresent(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessPtmcId(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessRXEnable(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessRealType(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessTXEnable(std::string* _aidl_return) override;
    ndk::ScopedAStatus getWirelessUserSleepMode(
            int32_t* _aidl_return) override;
    ndk::ScopedAStatus getWirelessVoltageNow(int32_t* _aidl_return) override;
    ndk::ScopedAStatus healthd_update_ui_soc_decimal(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus nightstandby(int32_t status,
                                    int32_t* _aidl_return) override;
    ndk::ScopedAStatus notifyScreenStatus(int32_t status,
                                          int32_t* _aidl_return) override;
    ndk::ScopedAStatus queryChargeInfo(std::string* _aidl_return) override;
    ndk::ScopedAStatus setChargeEMMode(const std::string& data,
                                       int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChargerControl(const std::string& data,
                                         int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChargerCoolDown(const std::string& data,
                                          int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChargerCriticalLog(const std::string& data,
                                             int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChargerCycle(const std::string& data,
                                       int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChargerFactoryModeTest(
            const std::string& data, int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChargerLog(const std::string& data,
                                     int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChgStatusToBcc(int32_t status,
                                         int32_t* _aidl_return) override;
    ndk::ScopedAStatus setCustomSelectChgMode(int32_t mode, bool enable,
                                              int32_t* _aidl_return) override;
    ndk::ScopedAStatus setFastchgFwUpdate(const std::string& data,
                                          int32_t* _aidl_return) override;
    ndk::ScopedAStatus setPsyMmiChgEn(const std::string& data,
                                      int32_t* _aidl_return) override;
    ndk::ScopedAStatus setPsyOtgSwitch(const std::string& data,
                                       int32_t* _aidl_return) override;
    ndk::ScopedAStatus setReserveSocDebug(const std::string& data,
                                          int32_t* _aidl_return) override;
    ndk::ScopedAStatus setShipMode(const std::string& data,
                                   int32_t* _aidl_return) override;
    ndk::ScopedAStatus setSmartChgMode(const std::string& data,
                                       int32_t* _aidl_return) override;
    ndk::ScopedAStatus setSmartCoolDown(int32_t coolDown,
                                        int32_t normalCoolDown,
                                        const std::string& pkgName,
                                        int32_t* _aidl_return) override;
    ndk::ScopedAStatus setTbattPwrOff(const std::string& data,
                                      int32_t* _aidl_return) override;
    ndk::ScopedAStatus setUisohDebugInfo(const std::string& data,
                                         int32_t* _aidl_return) override;
    ndk::ScopedAStatus setUsbPrimalType(const std::string& data,
                                        int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessChargePumpEn(const std::string& data,
                                               int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessFtmMode(const std::string& data,
                                          int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessIconDelay(const std::string& data,
                                            int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessIdtAdcTest(const std::string& data,
                                             int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessPenSoc(const std::string& data,
                                         int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessRXEnable(const std::string& data,
                                           int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessTXEnable(const std::string& data,
                                           int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWirelessUserSleepMode(
            const std::string& data, int32_t* _aidl_return) override;
    ndk::ScopedAStatus setWlsThirdPartitionInfo(
            const std::string& data, int32_t* _aidl_return) override;
    ndk::ScopedAStatus testKitFeatureTest(
            int32_t index, testKitFeatureTestResult* _aidl_return) override;
    ndk::ScopedAStatus testKitGetFeatureList(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus testKitGetFeatureName(
            int32_t index, std::string* _aidl_return) override;
    ndk::ScopedAStatus testKitGetFeatureNum(int32_t* _aidl_return) override;
    ndk::ScopedAStatus updateUiSohToPartion(int32_t* _aidl_return) override;
    ndk::ScopedAStatus queryWlsPencilInfo(
            std::string* _aidl_return) override;
    ndk::ScopedAStatus getChgOlcConfig(std::string* _aidl_return) override;
    ndk::ScopedAStatus setChgOlcConfig(const std::string& data,
                                       int32_t* _aidl_return) override;
    ndk::ScopedAStatus setSuperEnduranceStatus(const std::string& data,
                                               int32_t* _aidl_return) override;
    ndk::ScopedAStatus setSuperEnduranceCount(const std::string& data,
                                              int32_t* _aidl_return) override;
    ndk::ScopedAStatus setBobStatus(const std::string& data,
                                    int32_t* _aidl_return) override;
    ndk::ScopedAStatus setPsySlowChgEn(const std::string& data,
                                       int32_t* _aidl_return) override;
    ndk::ScopedAStatus getCpVbatDeviation(int32_t* _aidl_return) override;
    ndk::ScopedAStatus setBatteryLogPush(const std::string& data,
                                         int32_t* _aidl_return) override;
    ndk::ScopedAStatus getChargingModeInGsmCall(
            int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChargingModeInGsmCall(
            const std::string& data, int32_t* _aidl_return) override;
    ndk::ScopedAStatus setChgRusConfig(const std::string& data,
                                       int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPsyBatterySN(std::string* _aidl_return) override;
    ndk::ScopedAStatus getBattGaugeInfo(std::string* _aidl_return) override;
    ndk::ScopedAStatus setChgConfig(ChgFuncFlag flag, const std::string& extra,
                                    int32_t callname,
                                    int32_t* _aidl_return) override;
    ndk::ScopedAStatus getChgConfig(ChgFuncFlag flag, const std::string& extra,
                                    int32_t callname,
                                    std::string* _aidl_return) override;
    ndk::ScopedAStatus setUsbEyeDiagram(
            int32_t model, const std::string& eyeDiagram,
            bool isDefaultEyeDiagram, int32_t* _aidl_return) override;
    ndk::ScopedAStatus getUsbCurrentEyeDiagram(
            int32_t model, std::string* _aidl_return) override;

  private:
    // Internal initialization hooks used by the stock registerChargerService().
    int32_t chgFuncMapInit();
    int32_t chgDualCellsProtInit();

    // ChgFuncFlag handlers.  `extra` is the AIDL extra string, `callname` is
    // the caller identifier propagated by the framework.
    int32_t handleSetChgConfig(ChgFuncFlag flag, const std::string& extra,
                               int32_t callname, std::string* error);
    int32_t handleGetChgConfig(ChgFuncFlag flag, const std::string& extra,
                               int32_t callname, std::string* out,
                               std::string* error);

    // Persistent state that the stock implementation keeps in globals.
    void initializeBmsConfig();

    std::mutex lock_;
    int32_t smartChgMode_ = 0;
    int32_t chgUserMode_ = 0;
    int32_t bccExceptionScene_ = 0;
    int32_t bccStatus_ = 0;
    int32_t uiSoh_ = 100;
    int32_t screenStatus_ = 0;
    int32_t bmsStatus_ = 0;
    int32_t bmsCpuMask_ = 0;
    int32_t bmsBackgroundCpuMask_ = 0;
    bool bmsEnabled_ = false;
    bool bmsBackgroundSupported_ = false;
    bool bmsConfigLoaded_ = false;
    std::string reserveSocDebug_;
    std::array<int32_t, 12> uiSohDebugValues_ = {};
    bool uiSohDebugValid_ = false;
    int32_t chgUpLimit_[5] = {0, 0, 0, 0, 0};
};

// Registers the service as vendor.oplus.hardware.charger.ICharger/default.
int registerChargerService();

}  // namespace aidl::vendor::oplus::hardware::charger
