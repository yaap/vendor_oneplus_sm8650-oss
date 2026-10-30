/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "TEST-KIT"

#include "TestKit.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <android/log.h>

#include <array>
#include <cstring>

namespace aidl::vendor::oplus::hardware::charger {
namespace {

constexpr char kDevice[] = "/dev/chg_test_kit";
constexpr size_t kBufferSize = 4096;

constexpr unsigned long kIoctlSetIndex = 0x4004f601;
constexpr unsigned long kIoctlGetName = 0x9000f603;
constexpr unsigned long kIoctlGetFeatureList = 0x9000f604;
constexpr unsigned long kIoctlGetFeatureNum = 0x8004f605;
constexpr unsigned long kIoctlRunTest = 0x9000f606;

}  // namespace

KernelTest::KernelTest() = default;

KernelTest::~KernelTest() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

int KernelTest::open() {
    fd_ = ::open(kDevice, O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        const int rc = fd_;
        fd_ = -1;
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "can't open /dev/chg_test_kit, rc=%d", rc);
        return rc;
    }
    return 0;
}

int KernelTest::getFeatureNum(uint32_t* number) const {
    if (fd_ < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "/dev/chg_test_kit not open");
        return -19;
    }

    const int rc = ioctl(fd_, kIoctlGetFeatureNum, number);
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "get feature num error, rc=%d", rc);
        return rc;
    }
    return 0;
}

std::string KernelTest::getName() const {
    std::array<char, kBufferSize> buffer = {};
    if (fd_ < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "/dev/chg_test_kit not open");
        return "";
    }

    const int rc = ioctl(fd_, kIoctlGetName, buffer.data());
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "get name error, rc=%d", rc);
        return "";
    }

    return std::string(buffer.data());
}

std::string KernelTest::getName(uint32_t index) const {
    if (fd_ < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "/dev/chg_test_kit not open");
        return "";
    }

    const int rc = ioctl(fd_, kIoctlSetIndex, index);
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "set index error, rc=%d", rc);
        return "";
    }
    return getName();
}

std::string KernelTest::getFeatureList() const {
    std::array<char, kBufferSize> buffer = {};
    if (fd_ < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "/dev/chg_test_kit not open");
        return "";
    }

    const int rc = ioctl(fd_, kIoctlGetFeatureList, buffer.data());
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "get feature list error, rc=%d", rc);
        return "";
    }

    return std::string(buffer.data());
}

int KernelTest::runTest(std::string* result) const {
    std::array<char, kBufferSize> buffer = {};
    if (fd_ < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "/dev/chg_test_kit not open");
        return -19;
    }

    const int rc = ioctl(fd_, kIoctlRunTest, buffer.data());
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "run test error, rc=%d", rc);
        return rc;
    }

    switch (rc) {
        case 0:
        case 1:
            if (result != nullptr) {
                *result = buffer.data();
            }
            return rc;
        case 2:
            if (result != nullptr) {
                result->clear();
            }
            return 2;
        default:
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "unknown test result, rc=%d", rc);
            return -14;
    }
}

int KernelTest::runTest(uint32_t index, std::string* result) const {
    if (fd_ < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "/dev/chg_test_kit not open");
        return -19;
    }

    const int rc = ioctl(fd_, kIoctlSetIndex, index);
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "set index error, rc=%d", rc);
        return rc;
    }
    return runTest(result);
}

}  // namespace aidl::vendor::oplus::hardware::charger
