/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lcd_test_model.h"
#include "lvgl.h"
#include "subjects.h"

namespace viewmodel {

class LcdTestViewModel {
 public:
  LcdTestViewModel();
  ~LcdTestViewModel();

  LcdTestViewModel(const LcdTestViewModel&) = delete;
  LcdTestViewModel& operator=(const LcdTestViewModel&) = delete;

  lv_subject_t* color_index_subject();
  lv_subject_t* brightness_percent_subject();
  lv_subject_t* brightness_test_active_subject();

  bool is_brightness_test_active() const;
  void reset_test();
  void advance_color();
  void increase_brightness();
  void decrease_brightness();

 protected:
  void publish_all_();

 private:
  model::LcdTestModel model_{};
  reactive::IntSubject color_index_subject_;
  reactive::IntSubject brightness_percent_subject_;
  reactive::BoolSubject brightness_test_active_subject_;
};

}  // namespace viewmodel
