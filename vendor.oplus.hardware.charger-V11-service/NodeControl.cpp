/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "oplus-charger"

#include "NodeControl.h"

#include <fcntl.h>
#include <unistd.h>

#include <android/log.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace aidl::vendor::oplus::hardware::charger {
namespace {

std::optional<std::string> readFile(const std::string& path, size_t limit) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return std::nullopt;
    }

    std::string buffer(limit, '\0');
    const ssize_t count = read(fd, buffer.data(), limit);
    close(fd);
    if (count < 0) {
        return std::nullopt;
    }
    buffer.resize(static_cast<size_t>(count));
    return buffer;
}

std::string trim(std::string value) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(),
                             [&](char c) { return !isSpace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](char c) { return !isSpace(c); })
                    .base(),
                value.end());
    return value;
}

}  // namespace

std::optional<int32_t> nodeReadInt(const std::string& path) {
    char buffer[16] = {};
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return std::nullopt;
    }
    const ssize_t count = read(fd, buffer, sizeof(buffer));
    close(fd);
    if (count < 0) {
        return std::nullopt;
    }
    return atoi(buffer);
}

std::optional<std::string> nodeReadString(const std::string& path) {
    auto value = readFile(path, 255);
    if (!value) {
        return std::nullopt;
    }
    return *value;
}

std::optional<std::string> nodeReadTrimmed(const std::string& path) {
    auto value = readFile(path, 4096);
    if (!value) {
        return std::nullopt;
    }
    return trim(*value);
}

std::optional<std::string> nodeReadRaw(const std::string& path, size_t size) {
    return readFile(path, size);
}

int32_t nodeWrite(const std::string& path, const std::string& data) {
    const int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return kChargerError;
    }
    const ssize_t count = write(fd, data.data(), data.size());
    close(fd);
    if (count < 0) {
        return kChargerWriteError;
    }
    return 0;
}

int32_t nodeWriteCount(const std::string& path, const std::string& data) {
    const int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return kChargerError;
    }
    const ssize_t count = write(fd, data.data(), data.size());
    close(fd);
    if (count < 0) {
        return kChargerWriteError;
    }
    return static_cast<int32_t>(count);
}

std::optional<std::string> nodeReadFirstString(const std::string& first,
                                               const std::string& second) {
    auto value = nodeReadString(first);
    if (value) {
        return value;
    }
    return nodeReadString(second);
}

std::optional<int32_t> nodeReadFirstInt(const std::string& first,
                                        const std::string& second) {
    auto value = nodeReadInt(first);
    if (value) {
        return value;
    }
    return nodeReadInt(second);
}

int32_t nodeWriteFirst(const std::string& first, const std::string& second,
                       const std::string& data) {
    if (access(first.c_str(), F_OK) == 0) {
        return nodeWrite(first, data);
    }
    return nodeWrite(second, data);
}

std::string intWithNewline(int32_t value) {
    return std::to_string(value) + "\n";
}

void extraParse(const std::string& extra, std::string* text,
                std::vector<int32_t>* args) {
    args->clear();
    const size_t first = extra.find('+');
    if (first == std::string::npos) {
        // The stock single-string parser only succeeds when there is at least
        // one '+'.  The callers that use the string-only variant pass dates
        // directly, so preserve that behavior here by returning the input.
        if (text != nullptr) {
            *text = extra;
        }
        return;
    }

    if (text != nullptr) {
        *text = extra.substr(0, first);
    }

    size_t start = first + 1;
    while (start <= extra.size() && args->size() < 4) {
        const size_t next = extra.find('+', start);
        const std::string token = extra.substr(
                start, next == std::string::npos ? std::string::npos
                                                 : next - start);
        if (!token.empty()) {
            args->push_back(atoi(token.c_str()));
        } else {
            args->push_back(0);
        }
        if (next == std::string::npos) {
            break;
        }
        start = next + 1;
    }
}

std::vector<std::string> splitPlus(const std::string& value) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t next = value.find('+', start);
        result.push_back(value.substr(
                start, next == std::string::npos ? std::string::npos
                                                 : next - start));
        if (next == std::string::npos) {
            break;
        }
        start = next + 1;
    }
    return result;
}

}  // namespace aidl::vendor::oplus::hardware::charger
