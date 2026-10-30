/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Source replacement for the OPLUS /odm/bin/hw/gauged helper.
 *
 * The daemon waits for commands on /dev/sec_dev.  Command 1 asks it to send
 * one of the gauge verification blobs back to the charger driver through
 * ioctl(0x40017301).  The stock implementation only accepts index 0.
 */

#define LOG_TAG "gauged"

#include <android/log.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr char kDevice[] = "/dev/sec_dev";
constexpr int kGaugeAuthIoctl = 0x40017301;
constexpr size_t kCommandSize = 0x88;
constexpr int kOpenRetries = 50;

struct GaugeCommand {
    int32_t cmd;
    int32_t size;
    int32_t index;
    uint8_t reserved[kCommandSize - 3 * sizeof(int32_t)];
};

static_assert(sizeof(GaugeCommand) == kCommandSize);

// The stock binary embeds a single 32-byte verification record in .data.
constexpr std::array<uint8_t, 32> kVerifyData = {
        0xd5, 0x0e, 0x1f, 0x5b, 0xba, 0x5e, 0xc9, 0xaa,
        0x7d, 0x2f, 0x4a, 0xc9, 0x84, 0x19, 0x53, 0x9c,
        0x8d, 0x06, 0x80, 0x26, 0x92, 0x3f, 0x84, 0xb2,
        0xa7, 0x28, 0x88, 0xa5, 0x63, 0xbf, 0x86, 0x73,
};

int gDeviceFd = -1;
bool gStarted = false;

int openDevice() {
    int fd = open(kDevice, O_RDWR | O_CLOEXEC);
    for (int retry = 0; fd < 0 && retry < kOpenRetries; ++retry) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "%s can't open, rc=%d", kDevice, fd);
        sleep(1);
        fd = open(kDevice, O_RDWR | O_CLOEXEC);
    }
    return fd;
}

int gaugeGetVerifyDataByIndex(int index, uint8_t* data, size_t size) {
    if (data == nullptr || index != 0) {
        return -EINVAL;
    }
    const size_t copySize = std::min(size, kVerifyData.size());
    std::memcpy(data, kVerifyData.data(), copySize);
    return 0;
}

void* monitorThread(void*) {
    if (gStarted) {
        return nullptr;
    }
    gStarted = true;
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "THREAD running");

    gDeviceFd = openDevice();
    if (gDeviceFd < 0) {
        return nullptr;
    }

    GaugeCommand command = {};
    for (;;) {
        std::memset(&command, 0, sizeof(command));
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "cmd read");
        const ssize_t count = read(gDeviceFd, &command, sizeof(command));
        if (count < 0) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                "failed to read, rc=%d", static_cast<int>(count));
            close(gDeviceFd);
            gDeviceFd = openDevice();
            if (gDeviceFd < 0) {
                return nullptr;
            }
            continue;
        }

        if (command.cmd == 0) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "exit");
            close(gDeviceFd);
            gDeviceFd = -1;
            return nullptr;
        }

        if (command.cmd == 1) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                "cmd(=%d) func run", 1);
            int rc = 0;
            std::array<uint8_t, 32> verify = {};
            if (command.size != 4) {
                __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                    "data_size error");
                rc = -EINVAL;
            } else {
                __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                    "GaugeGetVerifyDataByIndex %d",
                                    command.index);
                rc = gaugeGetVerifyDataByIndex(command.index, verify.data(),
                                               verify.size());
                if (rc < 0) {
                    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                        "can't get index[%d] verify data, rc=%d",
                                        command.index, rc);
                } else {
                    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                        "ioctl %d", command.index);
                    rc = ioctl(gDeviceFd, kGaugeAuthIoctl, verify.data());
                    if (rc < 0) {
                        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                            "can't write gauge auth data, rc=%d",
                                            rc);
                    }
                }
            }
            if (rc < 0) {
                __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                    "failed to cmd(=%u) run, rc=%d",
                                    static_cast<unsigned>(command.cmd), rc);
            }
            continue;
        }

        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "cmd(=%d) not support", command.cmd);
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "failed to cmd(=%u) run, rc=%d",
                            static_cast<unsigned>(command.cmd), -1);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "run %s ENGINE START!", argv[0]);
    }

    pthread_t thread = 0;
    const int rc = pthread_create(&thread, nullptr, monitorThread, nullptr);
    if (rc == 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "THREAD is created!");
        pthread_join(thread, nullptr);
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "service end");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "Failed to create monitor thread");
    }
    return rc;
}
