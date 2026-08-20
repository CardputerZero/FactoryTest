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

// 按 USB 总线位置（bus/device 编号）枚举设备，附带 VID/PID 与设备名称。
std::vector<UsbPortDeviceInfo> list_usb_bus_devices(std::string& error_message);

}  // namespace platform::connectivity
