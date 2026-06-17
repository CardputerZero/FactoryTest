/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "keyboard_test_model.h"
#include "lvgl.h"
#include "subjects.h"

namespace viewmodel {

class KeyboardTestViewModel {
 public:
  KeyboardTestViewModel();
  ~KeyboardTestViewModel();

  KeyboardTestViewModel(const KeyboardTestViewModel&) = delete;
  KeyboardTestViewModel& operator=(const KeyboardTestViewModel&) = delete;

  lv_subject_t* key_subject();
  void record_key(const char* key_text);

 private:
  model::KeyboardTestModel model_{};
  reactive::StringSubject<64> key_subject_;
};

}  // namespace viewmodel
