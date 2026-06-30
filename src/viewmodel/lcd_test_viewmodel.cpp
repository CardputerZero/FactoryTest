/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lcd_test_viewmodel.h"

namespace viewmodel {

LcdTestViewModel::LcdTestViewModel()
    : color_index_subject_(model_.color_index()),
      brightness_percent_subject_(model_.brightness_percent()),
      brightness_test_active_subject_(model_.brightness_test_active()) {}

LcdTestViewModel::~LcdTestViewModel() = default;

lv_subject_t* LcdTestViewModel::color_index_subject() { return color_index_subject_.native(); }

lv_subject_t* LcdTestViewModel::brightness_percent_subject() {
  return brightness_percent_subject_.native();
}

lv_subject_t* LcdTestViewModel::brightness_test_active_subject() {
  return brightness_test_active_subject_.native();
}

bool LcdTestViewModel::is_brightness_test_active() const { return model_.brightness_test_active(); }

void LcdTestViewModel::reset_test() {
  model_.reset();
  publish_all_();
}

void LcdTestViewModel::advance_color() {
  model_.advance_color_index();
  publish_all_();
}

void LcdTestViewModel::start_brightness_test() {
  model_.start_brightness_test();
  publish_all_();
}

void LcdTestViewModel::increase_brightness() {
  model_.increase_brightness();
  publish_all_();
}

void LcdTestViewModel::decrease_brightness() {
  model_.decrease_brightness();
  publish_all_();
}

void LcdTestViewModel::publish_all_() {
  color_index_subject_.set(model_.color_index());
  brightness_percent_subject_.set(model_.brightness_percent());
  brightness_test_active_subject_.set(model_.brightness_test_active());
}

}  // namespace viewmodel
