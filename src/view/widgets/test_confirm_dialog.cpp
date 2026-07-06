/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "test_confirm_dialog.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include "bindings.h"
#include "theme.h"

namespace view::widgets {
namespace {

constexpr int32_t K_WIDTH             = 286;
constexpr int32_t K_HEIGHT            = 146;
constexpr int32_t K_PAD               = 8;
constexpr int32_t K_TITLE_HEIGHT      = 18;
constexpr int32_t K_BODY_WIDTH        = 246;
constexpr int32_t K_BUTTON_ROW_WIDTH  = 232;
constexpr int32_t K_BUTTON_WIDTH      = 62;
constexpr int32_t K_BUTTON_HEIGHT     = 26;
constexpr int32_t K_BUTTON_BOTTOM_PAD = 2;

}  // namespace

TestConfirmDialog::TestConfirmDialog(lv_obj_t* parent,
                                     viewmodel::AppViewModel& app_view_model,
                                     app::AssetManager& assets,
                                     TestConfirmDialogConfig config,
                                     TestConfirmDialogCallbacks callbacks)
    : BaseWidgets(parent),
      app_view_model_(app_view_model),
      assets_(assets),
      config_(std::move(config)),
      callbacks_(std::move(callbacks)) {}

TestConfirmDialog::~TestConfirmDialog() = default;

void TestConfirmDialog::build() {
  if (core_obj_ || !parent_) {
    return;
  }

  core_obj_ = lv_obj_create(parent_);
  register_core_obj_();
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, K_WIDTH, K_HEIGHT);
  lv_obj_align(core_obj_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(core_obj_, K_PAD, 0);
  lv_obj_set_style_radius(core_obj_, 12, 0);
  lv_obj_set_style_shadow_width(core_obj_, 12, 0);
  lv_obj_set_style_shadow_opa(core_obj_, LV_OPA_20, 0);
  lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);

  add_title_();
  add_body_();
  add_buttons_();
  bind_theme_(app_view_model_.dark_mode_subject(), app_view_model_.is_dark_mode());
  lv_obj_move_foreground(core_obj_);
}

bool TestConfirmDialog::visible() const {
  return core_obj_ && lv_obj_is_valid(core_obj_);
}

bool TestConfirmDialog::handle_key(uint32_t key, const char* key_name) {
  if (!visible() || action_triggered_) {
    return false;
  }
  if (is_previous_key_(key, key_name)) {
    move_focus_(-1);
    return true;
  }
  if (is_next_key_(key, key_name)) {
    move_focus_(1);
    return true;
  }
  if (is_enter_key_(key, key_name)) {
    trigger_focused_button_();
    return true;
  }

  if (is_cancel_key_(key, key_name)) {
    close();
    return true;
  }
  return false;
}

void TestConfirmDialog::close() {
  destroy_core_obj_();
  core_obj_              = nullptr;
  title_label_           = nullptr;
  body_label_            = nullptr;
  button_row_            = nullptr;
  for (auto& button : buttons_) {
    button = {};
  }
  button_count_          = 0;
  focused_button_index_  = 0;
  action_triggered_      = false;
}

const lv_font_t* TestConfirmDialog::ui_font_(const char* latin_font_name, uint32_t size) {
  auto* font = assets_.load_font(app_view_model_.ui_font_name(latin_font_name), size);
  return font ? font : &lv_font_montserrat_12;
}

void TestConfirmDialog::add_title_() {
  title_label_ = lv_label_create(core_obj_);
  const auto title = app_view_model_.tr(config_.title.c_str());
  lv_label_set_text(title_label_, title.c_str());
  lv_label_set_long_mode(title_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title_label_, K_WIDTH - K_PAD * 2);
  lv_obj_set_style_text_font(title_label_, ui_font_("inter-semibold.ttf", 12), 0);
  lv_obj_align(title_label_, LV_ALIGN_TOP_LEFT, 0, 0);
  reactive::bind_theme(title_label_,
                       app_view_model_.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
}

void TestConfirmDialog::add_body_() {
  body_label_ = lv_label_create(core_obj_);
  char body[48]{};
  const auto result_text = app_view_model_.tr("Test Result");
  if (config_.current > 0 && config_.total > 0) {
    std::snprintf(body,
                  sizeof(body),
                  "%zu/%zu - %s",
                  config_.current,
                  config_.total,
                  result_text.c_str());
  } else {
    std::snprintf(body, sizeof(body), "%s", result_text.c_str());
  }
  lv_label_set_text(body_label_, body);
  lv_label_set_long_mode(body_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_width(body_label_, K_BODY_WIDTH);
  lv_obj_set_style_text_align(body_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(body_label_, ui_font_("inter-medium.ttf", 14), 0);
  lv_obj_align(body_label_, LV_ALIGN_CENTER, 0, -10);
  reactive::bind_theme(body_label_,
                       app_view_model_.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
}

void TestConfirmDialog::add_buttons_() {
  button_row_ = lv_obj_create(core_obj_);
  lv_obj_remove_style_all(button_row_);
  lv_obj_set_size(button_row_, K_BUTTON_ROW_WIDTH, K_BUTTON_HEIGHT + 4);
  lv_obj_set_style_pad_left(button_row_, 3, 0);
  lv_obj_set_style_pad_right(button_row_, 3, 0);
  lv_obj_set_style_pad_top(button_row_, 2, 0);
  lv_obj_set_style_pad_bottom(button_row_, 2, 0);
  lv_obj_set_flex_flow(button_row_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_row_,
                        LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(button_row_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(button_row_, LV_ALIGN_BOTTOM_MID, 0, -(K_PAD + K_BUTTON_BOTTOM_PAD));

  add_button_("Fail", ButtonRole::FAIL);
  add_button_("Skip", ButtonRole::SKIP);
  add_button_("Pass", ButtonRole::PASS);
  focused_button_index_ = button_count_ - 1;
}

void TestConfirmDialog::add_button_(const char* label, ButtonRole role) {
  if (button_count_ >= sizeof(buttons_) / sizeof(buttons_[0])) {
    return;
  }

  auto& entry = buttons_[button_count_++];
  entry.role  = role;
  entry.button = lv_button_create(button_row_);
  lv_obj_remove_style_all(entry.button);
  lv_obj_set_size(entry.button, K_BUTTON_WIDTH, K_BUTTON_HEIGHT);
  lv_obj_set_style_radius(entry.button, 8, 0);
  lv_obj_add_event_cb(entry.button, button_cb, LV_EVENT_CLICKED, this);

  entry.label = lv_label_create(entry.button);
  const auto translated = app_view_model_.tr(label);
  lv_label_set_text(entry.label, translated.c_str());
  lv_obj_set_style_text_font(entry.label, ui_font_("inter-semibold.ttf", 11), 0);
  lv_obj_center(entry.label);
}

void TestConfirmDialog::apply_theme(bool dark_mode) {
  if (!core_obj_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  lv_obj_set_style_bg_color(core_obj_, colors.button, 0);
  lv_obj_set_style_bg_opa(core_obj_, LV_OPA_90, 0);
  lv_obj_set_style_border_color(core_obj_, colors.border, 0);
  lv_obj_set_style_border_width(core_obj_, 1, 0);

  for (std::size_t i = 0; i < button_count_; ++i) {
    apply_button_style_(buttons_[i], i == focused_button_index_);
  }
}

void TestConfirmDialog::apply_button_style_(ButtonEntry& entry, bool focused) {
  if (!entry.button || !entry.label) {
    return;
  }

  const auto colors = view::palette(app_view_model_.is_dark_mode());
  const auto tone   = role_color_(entry.role);
  const auto bg_mix = focused ? 76 : 54;
  lv_obj_set_style_bg_opa(entry.button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(entry.button, lv_color_mix(tone, colors.button, bg_mix), 0);
  lv_obj_set_style_border_width(entry.button, 1, 0);
  lv_obj_set_style_border_color(entry.button, tone, 0);
  lv_obj_set_style_outline_width(entry.button, focused ? 2 : 0, 0);
  lv_obj_set_style_outline_pad(entry.button, 0, 0);
  lv_obj_set_style_outline_opa(entry.button, LV_OPA_COVER, 0);
  lv_obj_set_style_outline_color(entry.button, tone, 0);
  lv_obj_set_style_text_color(entry.label, tone, 0);
}

void TestConfirmDialog::move_focus_(int32_t direction) {
  if (button_count_ == 0 || direction == 0) {
    return;
  }

  const auto count = static_cast<int32_t>(button_count_);
  auto next        = static_cast<int32_t>(focused_button_index_) + (direction > 0 ? 1 : -1);
  if (next < 0) {
    next = count - 1;
  } else if (next >= count) {
    next = 0;
  }
  focused_button_index_ = static_cast<std::size_t>(next);
  apply_theme(app_view_model_.is_dark_mode());
}

void TestConfirmDialog::trigger_focused_button_() {
  if (button_count_ == 0 || focused_button_index_ >= button_count_) {
    return;
  }

  action_triggered_ = true;
  switch (buttons_[focused_button_index_].role) {
    case ButtonRole::FAIL:
      if (callbacks_.fail_action) {
        callbacks_.fail_action();
      }
      break;
    case ButtonRole::SKIP:
      if (callbacks_.skip_action) {
        callbacks_.skip_action();
      }
      break;
    case ButtonRole::PASS:
      if (callbacks_.pass_action) {
        callbacks_.pass_action();
      }
      break;
  }
}

lv_color_t TestConfirmDialog::role_color_(ButtonRole role) const {
  const auto colors = view::palette(app_view_model_.is_dark_mode());
  switch (role) {
    case ButtonRole::FAIL:
      return colors.error;
    case ButtonRole::SKIP:
      return colors.warning;
    case ButtonRole::PASS:
    default:
      return colors.success;
  }
}

bool TestConfirmDialog::is_previous_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_LEFT || key == 'z' || key == 'Z' ||
         key_name_is_one_of_(key_name, "Left", "KEY_105", "Z");
}

bool TestConfirmDialog::is_next_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_RIGHT || key == 'c' || key == 'C' ||
         key_name_is_one_of_(key_name, "Right", "KEY_106", "C");
}

bool TestConfirmDialog::is_enter_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_ENTER || key == '\r' ||
         key_name_is_one_of_(key_name, "Enter", "Return", "KEY_96");
}

bool TestConfirmDialog::is_cancel_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_ESC || key_name_is_one_of_(key_name, "Esc", "Escape", "KEY_1");
}

bool TestConfirmDialog::key_name_is_one_of_(const char* key_name,
                                            const char* a,
                                            const char* b,
                                            const char* c) {
  if (!key_name) {
    return false;
  }
  return std::strcmp(key_name, a) == 0 || std::strcmp(key_name, b) == 0 ||
         std::strcmp(key_name, c) == 0;
}

void TestConfirmDialog::button_cb(lv_event_t* event) {
  auto* dialog = static_cast<TestConfirmDialog*>(lv_event_get_user_data(event));
  auto* target = lv_event_get_target_obj(event);
  if (!dialog || !target || dialog->action_triggered_) {
    return;
  }

  for (std::size_t i = 0; i < dialog->button_count_; ++i) {
    if (dialog->buttons_[i].button == target) {
      dialog->focused_button_index_ = i;
      dialog->trigger_focused_button_();
      return;
    }
  }
}

}  // namespace view::widgets
