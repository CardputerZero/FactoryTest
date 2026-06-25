/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "connectivity_types.h"

namespace platform::connectivity {

LinkTestResult run_link_test(const LinkTestSettings& settings);

}  // namespace platform::connectivity
