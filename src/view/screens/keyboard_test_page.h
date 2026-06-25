/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base_screen.h"
#include "icon_button.h"
#include "keyboard_test_viewmodel.h"

namespace screen {

class KeyboardTestPage : public BaseScreen {
 public:
  enum class KeyLayer {
    STANDARD_KEY,
    FN_KEY,
    SYM_KEY,
  };

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
  void build_layer_(KeyLayer layer);
  void mark_key_pressed_(uint32_t key, const char* key_name);
  void transition_to_(KeyLayer layer, bool animate = true);
  void switch_to_next_layout_();
  void sync_active_tile_();
  static void tile_scroll_end_cb(lv_event_t* event);
  static void long_key_listener(uint32_t key, const char* key_name, void* user_data);

  static constexpr std::size_t K_LAYER_COUNT = 3;
  viewmodel::KeyboardTestViewModel& keyboard_view_model_;
  std::vector<std::unique_ptr<view::widgets::IconButton>> key_buttons_{};
  std::vector<KeyState> key_states_{};
  std::vector<lv_obj_t*> layer_tiles_{};
  std::array<bool, K_LAYER_COUNT> layer_built_{};
  std::array<std::size_t, K_LAYER_COUNT> layer_pressed_counts_{};
  std::array<std::size_t, K_LAYER_COUNT> layer_total_counts_{};
  KeyLayer active_layer_{KeyLayer::STANDARD_KEY};
  lv_font_t* key_font_{nullptr};
  lv_font_t* extra_key_font_{nullptr};
  lv_obj_t* tileview_{nullptr};
};

}  // namespace screen
