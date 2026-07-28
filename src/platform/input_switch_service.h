/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform::input_switch {

bool read_headphone_inserted(bool& inserted, std::string& error_message);
void release_headphone_device();

}  // namespace platform::input_switch
