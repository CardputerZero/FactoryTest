/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

namespace platform::device_info {

struct DeviceInfoField {
  std::string label;
  std::string value;
};

std::vector<DeviceInfoField> read_device_info_fields();

}  // namespace platform::device_info
