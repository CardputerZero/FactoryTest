/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lcd_test_model.h"

#include <algorithm>

namespace model {

int32_t LcdTestModel::color_index() const { return color_index_; }

int32_t LcdTestModel::brightness_percent() const { return brightness_percent_; }

bool LcdTestModel::brightness_test_active() const { return brightness_test_active_; }

void LcdTestModel::reset() {
  color_index_            = 0;
  brightness_percent_     = K_INITIAL_BRIGHTNESS_PERCENT;
  brightness_test_active_ = false;
}

void LcdTestModel::set_brightness_percent(int32_t percent) {
  brightness_percent_ = std::clamp(percent, K_MIN_BRIGHTNESS_PERCENT, 100);
}

void LcdTestModel::advance_color_index() {
  if (brightness_test_active_) {
    return;
  }

  if (color_index_ < K_TOTAL_VISUAL_STEP_COUNT) {
    ++color_index_;
    return;
  }

  brightness_test_active_ = true;
}

void LcdTestModel::start_brightness_test() {
  brightness_test_active_ = true;
  color_index_            = K_TOTAL_VISUAL_STEP_COUNT + 1;
}

void LcdTestModel::increase_brightness() {
  if (!brightness_test_active_) {
    return;
  }

  brightness_percent_ = std::min(100, brightness_percent_ + K_BRIGHTNESS_STEP_PERCENT);
}

void LcdTestModel::decrease_brightness() {
  if (!brightness_test_active_) {
    return;
  }

  brightness_percent_ =
      std::max(K_MIN_BRIGHTNESS_PERCENT, brightness_percent_ - K_BRIGHTNESS_STEP_PERCENT);
}

}  // namespace model
