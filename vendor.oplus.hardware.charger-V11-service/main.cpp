/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.oplus.hardware.charger-V1-service"

#include <android/binder_process.h>
#include <android/log.h>

#include "Charger.h"

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    const int status =
            aidl::vendor::oplus::hardware::charger::registerChargerService();
    if (status != 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "registerChargerService failed: %d", status);
    }

    ABinderProcess_joinThreadPool();
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "oplus charger service start");
    return 0;
}
