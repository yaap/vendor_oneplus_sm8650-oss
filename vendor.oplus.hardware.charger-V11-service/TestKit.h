/*
 * Copyright (C) 2026 The YAAP Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <string>

namespace aidl::vendor::oplus::hardware::charger {

// Thin userspace wrapper for /dev/chg_test_kit.  The kernel driver uses four
// ioctls; the numbers are copied from the stock service.
class KernelTest {
  public:
    KernelTest();
    ~KernelTest();

    KernelTest(const KernelTest&) = delete;
    KernelTest& operator=(const KernelTest&) = delete;

    int open();
    int getFeatureNum(uint32_t* number) const;
    std::string getName() const;
    std::string getName(uint32_t index) const;
    std::string getFeatureList() const;
    int runTest(std::string* result) const;
    int runTest(uint32_t index, std::string* result) const;

  private:
    int fd_ = -1;
};

}  // namespace aidl::vendor::oplus::hardware::charger
