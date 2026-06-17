/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "keyboard_test_page.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

struct KeySpec {
  const char* match;
  const char* icon;
  int32_t row;
};

constexpr int32_t K_KEY_SIZE       = 32;
constexpr int32_t K_KEY_STRIDE     = 26;
constexpr int32_t K_ROW_STRIDE     = 20;
constexpr int32_t K_LAYOUT_WIDTH   = K_KEY_SIZE + 10 * K_KEY_STRIDE;
constexpr int32_t K_LAYOUT_HEIGHT  = K_KEY_SIZE + 4 * K_ROW_STRIDE;

constexpr const char* K_MATCH_0         = "0";
constexpr const char* K_MATCH_1         = "1";
constexpr const char* K_MATCH_2         = "2";
constexpr const char* K_MATCH_3         = "3";
constexpr const char* K_MATCH_4         = "4";
constexpr const char* K_MATCH_5         = "5";
constexpr const char* K_MATCH_6         = "6";
constexpr const char* K_MATCH_7         = "7";
constexpr const char* K_MATCH_8         = "8";
constexpr const char* K_MATCH_9         = "9";
constexpr const char* K_MATCH_A         = "a";
constexpr const char* K_MATCH_ALT       = "alt";
constexpr const char* K_MATCH_B         = "b";
constexpr const char* K_MATCH_BACKSPACE = "backspace";
constexpr const char* K_MATCH_C         = "c";
constexpr const char* K_MATCH_CTRL      = "ctrl";
constexpr const char* K_MATCH_D         = "d";
constexpr const char* K_MATCH_E         = "e";
constexpr const char* K_MATCH_ENTER     = "enter";
constexpr const char* K_MATCH_ESC       = "esc";
constexpr const char* K_MATCH_F         = "f";
constexpr const char* K_MATCH_FN        = "fn";
constexpr const char* K_MATCH_G         = "g";
constexpr const char* K_MATCH_H         = "h";
constexpr const char* K_MATCH_I         = "i";
constexpr const char* K_MATCH_J         = "j";
constexpr const char* K_MATCH_K         = "k";
constexpr const char* K_MATCH_L         = "l";
constexpr const char* K_MATCH_M         = "m";
constexpr const char* K_MATCH_N         = "n";
constexpr const char* K_MATCH_NEXT      = "next";
constexpr const char* K_MATCH_O         = "o";
constexpr const char* K_MATCH_P         = "p";
constexpr const char* K_MATCH_Q         = "q";
constexpr const char* K_MATCH_R         = "r";
constexpr const char* K_MATCH_S         = "s";
constexpr const char* K_MATCH_SHIFT     = "shift";
constexpr const char* K_MATCH_SPACE     = "space";
constexpr const char* K_MATCH_SYM       = "sym";
constexpr const char* K_MATCH_T         = "t";
constexpr const char* K_MATCH_TAB       = "tab";
constexpr const char* K_MATCH_U         = "u";
constexpr const char* K_MATCH_V         = "v";
constexpr const char* K_MATCH_W         = "w";
constexpr const char* K_MATCH_X         = "x";
constexpr const char* K_MATCH_Y         = "y";
constexpr const char* K_MATCH_Z         = "z";

constexpr std::array<KeySpec, KeyboardTestPage::K_KEY_COUNT> K_KEYS = {{
    {K_MATCH_ESC, view::ICON_KB_ESC, 0},
    {K_MATCH_TAB, view::ICON_KB_TAB, 0},
    {K_MATCH_1, view::ICON_KB_1, 1},
    {K_MATCH_2, view::ICON_KB_2, 1},
    {K_MATCH_3, view::ICON_KB_3, 1},
    {K_MATCH_4, view::ICON_KB_4, 1},
    {K_MATCH_5, view::ICON_KB_5, 1},
    {K_MATCH_6, view::ICON_KB_6, 1},
    {K_MATCH_7, view::ICON_KB_7, 1},
    {K_MATCH_8, view::ICON_KB_8, 1},
    {K_MATCH_9, view::ICON_KB_9, 1},
    {K_MATCH_0, view::ICON_KB_0, 1},
    {K_MATCH_BACKSPACE, view::ICON_KB_BACKSPACE, 1},
    {K_MATCH_SYM, view::ICON_KB_ANY, 2},
    {K_MATCH_Q, view::ICON_KB_Q, 2},
    {K_MATCH_W, view::ICON_KB_W, 2},
    {K_MATCH_E, view::ICON_KB_E, 2},
    {K_MATCH_R, view::ICON_KB_R, 2},
    {K_MATCH_T, view::ICON_KB_T, 2},
    {K_MATCH_Y, view::ICON_KB_Y, 2},
    {K_MATCH_U, view::ICON_KB_U, 2},
    {K_MATCH_I, view::ICON_KB_I, 2},
    {K_MATCH_O, view::ICON_KB_O, 2},
    {K_MATCH_P, view::ICON_KB_P, 2},
    {K_MATCH_SHIFT, view::ICON_KB_SHIFT, 3},
    {K_MATCH_A, view::ICON_KB_A, 3},
    {K_MATCH_S, view::ICON_KB_S, 3},
    {K_MATCH_D, view::ICON_KB_D, 3},
    {K_MATCH_F, view::ICON_KB_F, 3},
    {K_MATCH_G, view::ICON_KB_G, 3},
    {K_MATCH_H, view::ICON_KB_H, 3},
    {K_MATCH_J, view::ICON_KB_J, 3},
    {K_MATCH_K, view::ICON_KB_K, 3},
    {K_MATCH_L, view::ICON_KB_L, 3},
    {K_MATCH_ENTER, view::ICON_KB_ENTER, 3},
    {K_MATCH_FN, view::ICON_KB_FN, 4},
    {K_MATCH_CTRL, view::ICON_KB_CTRL, 4},
    {K_MATCH_ALT, view::ICON_KB_ALT, 4},
    {K_MATCH_Z, view::ICON_KB_Z, 4},
    {K_MATCH_X, view::ICON_KB_X, 4},
    {K_MATCH_C, view::ICON_KB_C, 4},
    {K_MATCH_V, view::ICON_KB_V, 4},
    {K_MATCH_B, view::ICON_KB_B, 4},
    {K_MATCH_N, view::ICON_KB_N, 4},
    {K_MATCH_M, view::ICON_KB_M, 4},
    {K_MATCH_SPACE, view::ICON_KB_SPACE, 4},
}};

std::string normalize_key_name(uint32_t key, const char* key_name) {
  std::string normalized = key_name ? key_name : "";
  if (normalized.empty()) {
    if (key == ' ') {
      normalized = K_MATCH_SPACE;
    } else if (key == '\t') {
      normalized = K_MATCH_TAB;
    } else if (key >= 32 && key <= 126) {
      normalized = static_cast<char>(key);
    }
  }

  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return normalized;
}

bool matches_key(const KeySpec& spec, const std::string& normalized_key, uint32_t key) {
  const std::string match = spec.match;
  if (match == K_MATCH_TAB) {
    return normalized_key == K_MATCH_NEXT || normalized_key == K_MATCH_TAB || key == LV_KEY_NEXT ||
           key == '\t';
  }
  if (match == K_MATCH_SPACE) {
    return normalized_key == K_MATCH_SPACE || key == ' ';
  }
  return normalized_key == match;
}

}  // namespace

KeyboardTestPage::KeyboardTestPage(viewmodel::AppViewModel& app_view_model,
                                   viewmodel::KeyboardTestViewModel& keyboard_view_model,
                                   app::AssetManager& assets)
    : BaseScreen(app_view_model, assets),
      keyboard_view_model_(keyboard_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::LONG_PRESS);
  init();
  platform::set_key_listener(key_listener, this);
}

KeyboardTestPage::~KeyboardTestPage() { platform::clear_key_listener(key_listener, this); }

void KeyboardTestPage::build_content(lv_obj_t* content) {
  if (auto* title_bar = title_bar_ref_()) {
    title_bar->set_title_alignment(LV_ALIGN_LEFT_MID, 8, 0);
  }
  update_progress_();

  auto* viewport = lv_obj_create(content);
  lv_obj_remove_style_all(viewport);
  lv_obj_set_size(viewport, LV_PCT(100), LV_PCT(100));
  lv_obj_center(viewport);
  lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(viewport, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(viewport, 0, 0);
  reactive::bind_theme(viewport,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  auto* layout = lv_obj_create(viewport);
  lv_obj_remove_style_all(layout);
  lv_obj_set_size(layout, K_LAYOUT_WIDTH, K_LAYOUT_HEIGHT);
  lv_obj_clear_flag(layout, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(layout, LV_ALIGN_TOP_MID, 0, 0);

  auto* key_font      = assets_ref_().load_font("kenney_input_keyboard_and_mouse.ttf", 30);
  lv_obj_t* row       = nullptr;
  int32_t current_row = -1;
  int32_t row_col     = 0;
  for (std::size_t i = 0; i < K_KEYS.size(); ++i) {
    const auto& spec = K_KEYS[i];
    if (!row || spec.row != current_row) {
      current_row = spec.row;
      row_col     = 0;
      row         = lv_obj_create(layout);
      lv_obj_remove_style_all(row);
      lv_obj_set_size(row, K_LAYOUT_WIDTH, K_KEY_SIZE);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_pos(row, 0, current_row * K_ROW_STRIDE);
    }

    key_buttons_[i] =
        std::make_unique<view::widgets::IconButton>(row,
                                                    app_view_model_ref_(),
                                                    K_KEY_SIZE,
                                                    K_KEY_SIZE,
                                                    spec.icon,
                                                    key_font ? key_font : &lv_font_montserrat_14,
                                                    view::palette(false).text_muted,
                                                    view::palette(true).text_muted);
    key_buttons_[i]->build();
    if (key_buttons_[i]->root()) {
      const int32_t x =
          current_row == 0 && row_col == 1 ? 10 * K_KEY_STRIDE : row_col * K_KEY_STRIDE;
      lv_obj_set_pos(key_buttons_[i]->root(), x, 0);
    }
    update_button_state_(i);
    ++row_col;
  }
}

void KeyboardTestPage::update_progress_() {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%zu/%zu", pressed_count_, K_KEY_COUNT);
  if (auto* title_bar = title_bar_ref_()) {
    title_bar->set_center_text(buffer);
  }
}

void KeyboardTestPage::update_button_state_(std::size_t index) {
  if (index >= key_buttons_.size() || !key_buttons_[index]) {
    return;
  }

  switch (key_states_[index]) {
    case KeyState::PRESSED:
      key_buttons_[index]->set_color(view::palette(false).success, view::palette(true).success);
      break;
    case KeyState::FAILED:
      key_buttons_[index]->set_color(view::palette(false).error, view::palette(true).error);
      break;
    case KeyState::IDLE:
    default:
      key_buttons_[index]->set_color(view::palette(false).text_muted, view::palette(true).text_muted);
      break;
  }
}

void KeyboardTestPage::mark_key_pressed_(uint32_t key, const char* key_name) {
  const auto normalized_key = normalize_key_name(key, key_name);
  for (std::size_t i = 0; i < K_KEYS.size(); ++i) {
    if (!matches_key(K_KEYS[i], normalized_key, key)) {
      continue;
    }

    if (key_states_[i] != KeyState::PRESSED) {
      key_states_[i] = KeyState::PRESSED;
      ++pressed_count_;
      update_button_state_(i);
      update_progress_();
    }
    return;
  }
}

void KeyboardTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<KeyboardTestPage*>(user_data);
  if (!page) {
    return;
  }

  page->keyboard_view_model_.record_key(key_name ? key_name : "");
  page->mark_key_pressed_(key, key_name);
}

}  // namespace screen
