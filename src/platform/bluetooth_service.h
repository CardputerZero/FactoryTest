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

std::vector<WirelessScanItem> scan_bluetooth(std::string& error_message);

}  // namespace platform::connectivity
