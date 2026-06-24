/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace model {

class LcdTestModel {
 public:
  static constexpr int32_t K_COLOR_STEP_COUNT = 9;
  static constexpr int32_t K_MIN_BRIGHTNESS_PERCENT = 1;
  static constexpr int32_t K_INITIAL_BRIGHTNESS_PERCENT = 80;
  static constexpr int32_t K_BRIGHTNESS_STEP_PERCENT = 5;

  int32_t color_index() const;
  int32_t brightness_percent() const;
  bool brightness_test_active() const;

  void reset();
  void advance_color_index();
  void increase_brightness();
  void decrease_brightness();

 private:
  int32_t color_index_{0};
  int32_t brightness_percent_{K_INITIAL_BRIGHTNESS_PERCENT};
  bool brightness_test_active_{false};
};

}  // namespace model
