/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

#include "connectivity_types.h"

namespace platform::connectivity {

HdmiInfo read_hdmi_info(std::string& error_message);

}  // namespace platform::connectivity
