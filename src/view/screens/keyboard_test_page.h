/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base_screen.h"
#include "icon_button.h"
#include "keyboard_test_viewmodel.h"
#include "popup.h"

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
  static void key_release_listener(uint32_t key, const char* key_name, void* user_data);

 private:
  enum class KeyState {
    IDLE,
    PRESSED,
    FAILED,
  };

  void update_progress_();
  void update_button_state_(std::size_t index);
  void flash_key_(std::size_t index);
  void build_counter_(lv_obj_t* layout, KeyLayer layer);
  void update_counter_(KeyLayer layer);
  void note_key_conflict_(std::size_t index);
  void flash_conflict_(std::size_t first, std::size_t second);
  void show_multi_key_warning_();
  void build_layer_(KeyLayer layer);
  void mark_key_pressed_(uint32_t key, const char* key_name);
  void transition_to_(KeyLayer layer, bool animate = true);
  void schedule_auto_advance_(KeyLayer source_layer);
  void update_nav_actions_();
  void switch_to_next_layout_();
  void sync_active_tile_();
  void show_hold_popup_(const char* message, view::widgets::PopupTone tone);
  void hide_hold_popup_();
  static void auto_advance_timer_cb(lv_timer_t* timer);
  static void tile_scroll_end_cb(lv_event_t* event);
  static void long_key_listener(uint32_t key, const char* key_name, void* user_data);

  static constexpr std::size_t K_LAYER_COUNT = 3;
  viewmodel::KeyboardTestViewModel& keyboard_view_model_;
  std::vector<std::unique_ptr<view::widgets::IconButton>> key_buttons_{};
  std::vector<KeyState> key_states_{};
  // Number of times each key was triggered. From ten presses on the key keeps
  // the layer color, so a key that keeps firing stays visible on the layout.
  std::vector<std::uint32_t> key_press_counts_{};
  // Live counter of each layer: the most recent key of that layer and its press
  // count, so a key of another layer never shows up in the active readout.
  std::array<std::unique_ptr<view::widgets::IconButton>, K_LAYER_COUNT> counter_keys_{};
  std::array<lv_obj_t*, K_LAYER_COUNT> counter_labels_{};
  std::array<std::size_t, K_LAYER_COUNT> counter_key_indices_{};
  std::array<bool, K_LAYER_COUNT> counter_has_key_{};
  // Last key press, used to catch one press that triggers several keys and two
  // keys hit at the same time.
  std::size_t last_press_index_{0};
  uint32_t last_press_tick_{0};
  bool has_last_press_{false};
  std::vector<lv_obj_t*> layer_tiles_{};
  std::array<bool, K_LAYER_COUNT> layer_built_{};
  std::array<std::size_t, K_LAYER_COUNT> layer_pressed_counts_{};
  std::array<std::size_t, K_LAYER_COUNT> layer_total_counts_{};
  KeyLayer active_layer_{KeyLayer::STANDARD_KEY};
  KeyLayer auto_advance_source_layer_{KeyLayer::STANDARD_KEY};
  lv_font_t* key_font_{nullptr};
  lv_font_t* extra_key_font_{nullptr};
  lv_obj_t* tileview_{nullptr};
  lv_timer_t* auto_advance_timer_{nullptr};
  std::unique_ptr<view::widgets::Popup> hold_popup_{};
  std::unique_ptr<view::widgets::Popup> conflict_popup_{};
  view::widgets::PopupTone hold_popup_tone_{view::widgets::PopupTone::DEFAULT};
};

}  // namespace screen
