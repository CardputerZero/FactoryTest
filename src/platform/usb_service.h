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


std::vector<UsbPortDeviceInfo> list_usb_bus_devices(std::string& error_message);

}  // namespace platform::connectivity
