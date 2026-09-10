/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

#include "connectivity_types.h"

namespace platform::connectivity {

std::vector<UsbDeviceInfo> list_usb_devices(std::string& error_message);


std::vector<UsbPortDeviceInfo> list_usb_port_devices(std::string& error_message);

inline bool is_usb_device_at_port(const UsbPortDeviceInfo& device,
                                  uint32_t expected_bus,
                                  const std::string& expected_port_path) {
  return device.bus == expected_bus && device.port_path == expected_port_path;
}

}  // namespace platform::connectivity
