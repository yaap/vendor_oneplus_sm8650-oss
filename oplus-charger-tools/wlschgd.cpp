/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Source replacement for the OPLUS /odm/bin/hw/wlschgd helper.
 *
 * The daemon waits for command 0 on /dev/wls_dev.  For each command it reads
 * eight random bytes from /dev/random, runs the OPLUS two-stage DES challenge
 * and sends the 16-byte {random, encoded} record back to the wireless charger
 * driver through ioctl(0x4010f101).
 */

#define LOG_TAG "wlschgd"

#include <cutils/klog.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr char kDevice[] = "/dev/wls_dev";
constexpr char kRandomDevice[] = "/dev/random";
constexpr int kWlsAuthIoctl = 0x4010f101;
constexpr size_t kCommandSize = 0x88;
constexpr int kOpenRetries = 15;

struct WlsCommand {
    int32_t cmd;
    uint8_t reserved[kCommandSize - sizeof(int32_t)];
};

static_assert(sizeof(WlsCommand) == kCommandSize);

// Standard DES tables used by the stock implementation.
constexpr std::array<uint8_t, 64> kIp = {
        58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
        62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
        57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
        61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7,
};

constexpr std::array<uint8_t, 64> kFp = {
        40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
        38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
        36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
        34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9, 49, 17, 57, 25,
};

constexpr std::array<uint8_t, 48> kE = {
        32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 8, 9, 10, 11,
        12, 13, 12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21,
        22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1,
};

constexpr std::array<uint8_t, 32> kP = {
        16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10,
        2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25,
};

constexpr std::array<uint8_t, 56> kPc1 = {
        57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18, 10, 2,
        59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36, 63, 55, 47, 39,
        31, 23, 15, 7, 62, 54, 46, 38, 30, 22, 14, 6, 61, 53, 45, 37,
        29, 21, 13, 5, 28, 20, 12, 4,
};

constexpr std::array<uint8_t, 48> kPc2 = {
        14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10, 23, 19, 12, 4,
        26, 8, 16, 7, 27, 20, 13, 2, 41, 52, 31, 37, 47, 55, 30, 40,
        51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32,
};

constexpr std::array<uint8_t, 16> kShifts = {
        1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1,
};

constexpr std::array<std::array<uint8_t, 64>, 8> kS = {{
        {{
                14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
                0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
                4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
                15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13,
        }},
        {{
                15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
                3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
                0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
                13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9,
        }},
        {{
                10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
                13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
                13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
                1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12,
        }},
        {{
                7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
                13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
                10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
                3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14,
        }},
        {{
                2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
                14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
                4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
                11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3,
        }},
        {{
                12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
                10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
                9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
                4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13,
        }},
        {{
                4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
                13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
                1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
                6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12,
        }},
        {{
                13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
                1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
                7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
                2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11,
        }},
}};

// The stock key schedules are built from these 64-byte bit arrays.  Index 0
// is a dummy byte; indices 1..64 are the DES key bits, MSB first.
constexpr std::array<uint8_t, 65> kKey1Bits = {
        0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1,
        1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
        1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0,
        0,
};

constexpr std::array<uint8_t, 65> kKey2Bits = {
        0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1,
        0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1,
        1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,
};

using RoundKeys = std::array<std::array<uint8_t, 48>, 16>;

RoundKeys buildRoundKeys(const std::array<uint8_t, 65>& key) {
    std::array<uint8_t, 128> schedule = {};
    for (size_t i = 0; i < kPc1.size(); ++i) {
        schedule[1 + i] = key[kPc1[i]];
    }

    RoundKeys roundKeys = {};
    size_t base = 0;
    for (size_t round = 0; round < kShifts.size(); ++round) {
        const size_t shift = kShifts[round];
        for (size_t bit = 0; bit < shift; ++bit) {
            schedule[base + 0x39 + bit] = schedule[base + 0x1c + bit];
            schedule[base + 0x1c + bit] = schedule[base + 1 + bit];
        }
        base += shift;
        for (size_t i = 0; i < kPc2.size(); ++i) {
            roundKeys[round][i] = schedule[base + kPc2[i]];
        }
    }
    return roundKeys;
}

void desCrypt(const uint8_t input[8], const RoundKeys& roundKeys,
              uint8_t output[8]) {
    std::array<uint8_t, 64> bits = {};
    for (size_t byte = 0; byte < 8; ++byte) {
        for (size_t bit = 0; bit < 8; ++bit) {
            bits[byte * 8 + bit] = (input[byte] >> (7 - bit)) & 1;
        }
    }

    std::array<uint8_t, 64> block = {};
    for (size_t i = 0; i < kIp.size(); ++i) {
        block[i] = bits[kIp[i] - 1];
    }

    std::array<uint8_t, 32> left = {};
    std::array<uint8_t, 32> right = {};
    for (size_t i = 0; i < left.size(); ++i) {
        left[i] = block[i];
        right[i] = block[32 + i];
    }

    for (const auto& roundKey : roundKeys) {
        std::array<uint8_t, 48> expanded = {};
        for (size_t i = 0; i < kE.size(); ++i) {
            expanded[i] = right[kE[i] - 1] ^ roundKey[i];
        }

        std::array<uint8_t, 32> substituted = {};
        for (size_t box = 0; box < kS.size(); ++box) {
            const size_t offset = box * 6;
            const int row = (expanded[offset] << 1) | expanded[offset + 5];
            const int column = (expanded[offset + 1] << 3) |
                               (expanded[offset + 2] << 2) |
                               (expanded[offset + 3] << 1) |
                               expanded[offset + 4];
            const uint8_t value = kS[box][row * 16 + column];
            substituted[box * 4 + 0] = (value >> 3) & 1;
            substituted[box * 4 + 1] = (value >> 2) & 1;
            substituted[box * 4 + 2] = (value >> 1) & 1;
            substituted[box * 4 + 3] = value & 1;
        }

        std::array<uint8_t, 32> permuted = {};
        for (size_t i = 0; i < kP.size(); ++i) {
            permuted[i] = substituted[kP[i] - 1];
        }

        std::array<uint8_t, 32> nextRight = {};
        for (size_t i = 0; i < right.size(); ++i) {
            nextRight[i] = left[i] ^ permuted[i];
        }
        left = right;
        right = nextRight;
    }

    std::array<uint8_t, 64> preOutput = {};
    for (size_t i = 0; i < left.size(); ++i) {
        preOutput[i] = right[i];
        preOutput[32 + i] = left[i];
    }

    std::array<uint8_t, 64> finalBlock = {};
    for (size_t i = 0; i < kFp.size(); ++i) {
        finalBlock[i] = preOutput[kFp[i] - 1];
    }

    for (size_t byte = 0; byte < 8; ++byte) {
        uint8_t value = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            value = static_cast<uint8_t>((value << 1) |
                                         finalBlock[byte * 8 + bit]);
        }
        output[byte] = value;
    }
}

void wlsAuthenticate(const uint8_t random[8], uint8_t encoded[8]) {
    static const RoundKeys keys1 = buildRoundKeys(kKey1Bits);
    static const RoundKeys keys2 = buildRoundKeys(kKey2Bits);

    uint8_t middle[8] = {};
    desCrypt(random, keys1, middle);
    desCrypt(middle, keys2, encoded);

    klog_write(6,
               "<6>wls_auth: wls_auth::random_num: %02x %02x %02x %02x "
               "%02x %02x %02x %02x\n\r",
               static_cast<unsigned>(random[0]),
               static_cast<unsigned>(random[1]),
               static_cast<unsigned>(random[2]),
               static_cast<unsigned>(random[3]),
               static_cast<unsigned>(random[4]),
               static_cast<unsigned>(random[5]),
               static_cast<unsigned>(random[6]),
               static_cast<unsigned>(random[7]));
    klog_write(6,
               "<6>wls_auth: wls_auth::encode_num: %02x %02x %02x %02x "
               "%02x %02x %02x %02x\n\r",
               static_cast<unsigned>(encoded[0]),
               static_cast<unsigned>(encoded[1]),
               static_cast<unsigned>(encoded[2]),
               static_cast<unsigned>(encoded[3]),
               static_cast<unsigned>(encoded[4]),
               static_cast<unsigned>(encoded[5]),
               static_cast<unsigned>(encoded[6]),
               static_cast<unsigned>(encoded[7]));
}

int gWlsFd = -1;
bool gStarted = false;

int openWithRetry(const char* path, int flags, int retries) {
    int fd = open(path, flags | O_CLOEXEC);
    for (int retry = 0; fd < 0 && retry < retries; ++retry) {
        klog_write(3, "<3>wlschgd: %s can't open, rc=%d\n", path, fd);
        sleep(1);
        fd = open(path, flags | O_CLOEXEC);
    }
    return fd;
}

int wlsChgAuthenticate() {
    const int randomFd = openWithRetry(kRandomDevice, O_RDONLY, kOpenRetries);
    if (randomFd < 0) {
        return -1;
    }

    uint8_t random[8] = {};
    for (size_t i = 0; i < sizeof(random); ++i) {
        const ssize_t count = read(randomFd, &random[i], 1);
        if (count != 1) {
            klog_write(3,
                       "<3>wlschgd: can't read /dev/random, rc=%d\n",
                       static_cast<int>(count));
            close(randomFd);
            return -1;
        }
    }
    close(randomFd);

    uint8_t auth[16] = {};
    std::memcpy(auth, random, sizeof(random));
    wlsAuthenticate(random, auth + sizeof(random));

    const int rc = ioctl(gWlsFd, kWlsAuthIoctl, auth);
    if (rc < 0) {
        klog_write(3, "<3>wlschgd: can't write wls_auth data, rc=%d\n", rc);
    }
    return rc;
}

void* monitorThread(void*) {
    if (gStarted) {
        return nullptr;
    }
    gStarted = true;
    klog_write(3, "<3>wlschgd: THREAD running\n");

    gWlsFd = openWithRetry(kDevice, O_RDWR, kOpenRetries);
    if (gWlsFd < 0) {
        return nullptr;
    }

    WlsCommand command = {};
    for (;;) {
        std::memset(&command, 0, sizeof(command));
        klog_write(3, "<3>wlschgd: cmd read\n");
        const ssize_t count = read(gWlsFd, &command, sizeof(command));
        if (count < 0) {
            klog_write(3, "<3>wlschgd: failed to read, rc=%d\n",
                       static_cast<int>(count));
            close(gWlsFd);
            gWlsFd = openWithRetry(kDevice, O_RDWR, kOpenRetries);
            if (gWlsFd < 0) {
                return nullptr;
            }
            continue;
        }

        if (command.cmd == 0) {
            const int rc = wlsChgAuthenticate();
            if (rc < 0) {
                klog_write(3,
                           "<3>wlschgd: failed to cmd(=%d) run, rc=%d\n",
                           0, rc);
            }
            continue;
        }

        klog_write(3, "<3>wlschgd: cmd(=%d) not support\n", command.cmd);
        klog_write(3, "<3>wlschgd: failed to cmd(=%d) run, rc=%d\n",
                   command.cmd, -1);
    }
}

}  // namespace

int main(int argc, char** argv) {
    klog_write(3, "<3>wlschgd: wlschgd enterd\n");
    if (argc > 0) {
        klog_write(3, "<3>wlschgd: run %s ENGINE START!\n", argv[0]);
    }

    pthread_t thread = 0;
    const int rc = pthread_create(&thread, nullptr, monitorThread, nullptr);
    if (rc == 0) {
        klog_write(3, "<3>wlschgd: THREAD is created!\n");
        pthread_join(thread, nullptr);
    } else {
        klog_write(3, "<3>wlschgd: Failed to create monitor thread\n");
    }
    return rc;
}
