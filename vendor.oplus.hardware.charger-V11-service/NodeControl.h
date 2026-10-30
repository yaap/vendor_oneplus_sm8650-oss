/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aidl::vendor::oplus::hardware::charger {

// Error values used by the OPLUS charger HAL.  They are not errno values.
constexpr int32_t kChargerError = static_cast<int32_t>(0x80000000u);
constexpr int32_t kChargerReadError = static_cast<int32_t>(0x80000002u);
constexpr int32_t kChargerWriteError = static_cast<int32_t>(0x80000003u);

// Reads an integer from a sysfs/proc node.  The stock implementation reads at
// most 16 bytes and uses atoi(), so "12\n" and "12 extra" both become 12.
std::optional<int32_t> nodeReadInt(const std::string& path);

// Reads at most 255 bytes, returns the raw contents (the stock code does not
// trim these nodes).
std::optional<std::string> nodeReadString(const std::string& path);

// Reads a whole file and trims leading/trailing whitespace, matching
// android::base::ReadFileToString(..., false) + android::base::Trim().
std::optional<std::string> nodeReadTrimmed(const std::string& path);

// Reads at most `size` bytes.  Returns std::nullopt when the node cannot be
// opened or read.
std::optional<std::string> nodeReadRaw(const std::string& path, size_t size);

// Opens a node for writing and writes exactly `data.size()` bytes.  Returns 0
// on success, kChargerError if the node cannot be opened and
// kChargerWriteError if the write fails.
int32_t nodeWrite(const std::string& path, const std::string& data);

// Same as nodeWrite(), but returns the number of bytes written on success.
// The stock "writeFile" helper exposes that value to a few flag handlers.
int32_t nodeWriteCount(const std::string& path, const std::string& data);

// First existing node from the list, or an empty optional.
std::optional<std::string> nodeReadFirstString(const std::string& first,
                                               const std::string& second);
std::optional<int32_t> nodeReadFirstInt(const std::string& first,
                                        const std::string& second);

// Writes to the first writable node from the list.
int32_t nodeWriteFirst(const std::string& first, const std::string& second,
                       const std::string& data);

// Formats a signed integer with the stock "%d\n" format used by several
// setter paths.
std::string intWithNewline(int32_t value);

// Splits an OPLUS "extra" argument on '+'.  The first token is copied to
// `text`; up to four following tokens are parsed as decimal integers.  This
// mirrors the stock extraParse()/lineParse() helpers.
void extraParse(const std::string& extra, std::string* text,
                std::vector<int32_t>* args);

// Split a '+'-separated type name list used by GET_CHARGER_INFO.
std::vector<std::string> splitPlus(const std::string& value);

}  // namespace aidl::vendor::oplus::hardware::charger
