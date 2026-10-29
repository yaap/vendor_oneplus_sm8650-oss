/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/oplus/hardware/oplusSensor/BnSensorFeature.h>

#include <cstdint>
#include <string>
#include <vector>

namespace aidl::vendor::oplus::hardware::oplusSensor {

class SensorFeature : public BnSensorFeature {
  public:
    SensorFeature();
    ~SensorFeature() override = default;

    ndk::ScopedAStatus calibrateSensor(int32_t sensor_type, int32_t para,
                                       bool* _aidl_return) override;
    ndk::ScopedAStatus getSensorCalibrationData(int32_t sensor_type,
                                                std::string* _aidl_return) override;
    ndk::ScopedAStatus getSensorConfig(std::string* _aidl_return) override;
    ndk::ScopedAStatus getSensorRawData(int32_t sensor_type, float* _aidl_return) override;
    ndk::ScopedAStatus saveCalibrationData(int32_t sensor_type, int32_t type,
                                           const std::vector<uint8_t>& cali_data,
                                           int32_t* _aidl_return) override;
    ndk::ScopedAStatus sensorRawDataEnableDisable(int32_t sensor_type, int32_t para,
                                                  bool* _aidl_return) override;
    ndk::ScopedAStatus sensorReadNodeFile(const std::string& node_name, int32_t length,
                                          std::string* _aidl_return) override;
    ndk::ScopedAStatus sensorWriteNodeFile(const std::string& node_name,
                                           const std::string& info,
                                           int32_t* _aidl_return) override;
    ndk::ScopedAStatus setPhoneMode(int8_t type, int8_t on, int32_t* _aidl_return) override;
};

}  // namespace aidl::vendor::oplus::hardware::oplusSensor
