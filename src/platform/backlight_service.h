/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace platform::backlight {

bool read_brightness_percent(int32_t& percent);
bool write_brightness_percent(int32_t percent);

}  // namespace platform::backlight
