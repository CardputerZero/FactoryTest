/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "keyboard_test_viewmodel.h"

namespace viewmodel {

KeyboardTestViewModel::KeyboardTestViewModel()
    : key_subject_(model_.placeholder()) {}

KeyboardTestViewModel::~KeyboardTestViewModel() = default;

lv_subject_t* KeyboardTestViewModel::key_subject() { return key_subject_.native(); }

void KeyboardTestViewModel::record_key(const char* key_text) { key_subject_.set(key_text); }

}  // namespace viewmodel
