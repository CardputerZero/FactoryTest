/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "base_screen.h"
#include "icon_button.h"
#include "keyboard_test_viewmodel.h"

namespace screen {

class KeyboardTestPage : public BaseScreen {
 public:
  static constexpr std::size_t K_KEY_COUNT = 46;

  KeyboardTestPage(viewmodel::AppViewModel& app_view_model,
                   viewmodel::KeyboardTestViewModel& keyboard_view_model,
                   app::AssetManager& assets);
  ~KeyboardTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;
  static void key_listener(uint32_t key, const char* key_name, void* user_data);

 private:
  enum class KeyState {
    IDLE,
    PRESSED,
    FAILED,
  };

  void update_progress_();
  void update_button_state_(std::size_t index);
  void mark_key_pressed_(uint32_t key, const char* key_name);

  viewmodel::KeyboardTestViewModel& keyboard_view_model_;
  std::array<std::unique_ptr<view::widgets::IconButton>, K_KEY_COUNT> key_buttons_{};
  std::array<KeyState, K_KEY_COUNT> key_states_{};
  std::size_t pressed_count_{0};
};

}  // namespace screen
