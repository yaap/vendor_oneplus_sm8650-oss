/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "OplusSensorFeature"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_auto_utils.h>
#include <android/binder_interface_utils.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <string>

#include "SensorFeature.h"

using aidl::vendor::oplus::hardware::oplusSensor::ISensorFeature;
using aidl::vendor::oplus::hardware::oplusSensor::SensorFeature;

int main() {
    android::base::InitLogging(nullptr, android::base::LogdLogger());

    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto service = ndk::SharedRefBase::make<SensorFeature>();
    if (service == nullptr) {
        LOG(FATAL) << "failed to allocate SensorFeature";
        return 1;
    }

    const std::string instance = std::string(ISensorFeature::descriptor) + "/default";
    const bool lowMemory =
            android::base::GetBoolProperty("ro.vendor.oplus.sensor.lowmem", false);
    const binder_status_t status =
            lowMemory ? AServiceManager_registerLazyService(service->asBinder().get(),
                                                            instance.c_str())
                      : AServiceManager_addService(service->asBinder().get(),
                                                   instance.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << "failed to register " << instance << ": "
                   << static_cast<int>(status);
        return 1;
    }

    LOG(INFO) << "registered " << instance
              << (lowMemory ? " (lazy)" : " (normal)");
    ABinderProcess_joinThreadPool();
    return 0;
}
