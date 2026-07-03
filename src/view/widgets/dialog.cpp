/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "dialog.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "bindings.h"
#include "theme.h"

namespace view::widgets {

Dialog::Dialog(lv_obj_t* parent,
               viewmodel::AppViewModel& app_view_model,
               app::AssetManager& assets,
               DialogConfig config,
               DialogCallbacks callbacks)
    : BaseWidgets(parent),
      app_view_model_(app_view_model),
      assets_(assets),
      config_(std::move(config)),
      callbacks_(std::move(callbacks)) {}

Dialog::~Dialog() = default;

void Dialog::build() {
  if (core_obj_ || !parent_) {
    return;
  }

  core_obj_ = lv_obj_create(parent_);
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, config_.width, config_.height);
  lv_obj_align(core_obj_, config_.align, config_.offset_x, config_.offset_y);
  lv_obj_set_style_pad_all(core_obj_, config_.pad_all, 0);
  lv_obj_set_style_pad_row(core_obj_, config_.pad_row, 0);
  lv_obj_set_style_radius(core_obj_, 12, 0);
  lv_obj_set_style_shadow_width(core_obj_, 12, 0);
  lv_obj_set_style_shadow_opa(core_obj_, LV_OPA_20, 0);
  if (config_.scroll_y) {
    lv_obj_add_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(core_obj_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(core_obj_, LV_SCROLLBAR_MODE_AUTO);
  } else {
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
  }

  if (config_.show_title || config_.show_shortcuts) {
    add_title_row_();
  }

  content_ = lv_obj_create(core_obj_);
  lv_obj_remove_style_all(content_);
  const int32_t title_height = (config_.show_title || config_.show_shortcuts) ? 18 : 0;
  const int32_t content_y = config_.pad_all + title_height +
                            ((config_.show_title || config_.show_shortcuts) ? config_.pad_row : 0);
  const int32_t button_y =
      config_.height - config_.pad_all - config_.button_bottom_pad - config_.button_height;
  const int32_t content_height = std::max<int32_t>(24, button_y - content_y - config_.pad_row);
  lv_obj_set_size(content_, config_.width - config_.pad_all * 2, content_height);
  lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(content_, config_.pad_row, 0);
  lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(content_, LV_ALIGN_TOP_MID, 0, content_y + config_.body_offset_y);

  add_button_row_();
  bind_theme_(app_view_model_.dark_mode_subject(), app_view_model_.is_dark_mode());
  update_button_focus_();
  lv_obj_move_foreground(core_obj_);
}

bool Dialog::visible() const { return core_obj_ && lv_obj_is_valid(core_obj_); }

bool Dialog::handle_key(uint32_t key, const char* key_name) {
  if (!visible() || action_triggered_) {
    return false;
  }
  if (config_.focus_button_navigation) {
    if (is_cancel_key_(key, key_name)) {
      return true;
    }
    if (is_focus_previous_key_(key, key_name)) {
      move_button_focus_(-1);
      return true;
    }
    if (is_focus_next_key_(key, key_name)) {
      move_button_focus_(1);
      return true;
    }
    if (is_ok_key_(key, key_name)) {
      trigger_focused_button_();
      return true;
    }
    return false;
  }
  if (is_cancel_key_(key, key_name)) {
    trigger_cancel();
    return true;
  }
  if (is_ok_key_(key, key_name)) {
    trigger_ok();
    return true;
  }
  return false;
}

void Dialog::set_ok_action(std::function<void()> action) {
  callbacks_.ok_action = std::move(action);
}

void Dialog::set_skip_action(std::function<void()> action) {
  callbacks_.skip_action = std::move(action);
}

void Dialog::set_cancel_action(std::function<void()> action) {
  callbacks_.cancel_action = std::move(action);
}

void Dialog::trigger_ok() {
  if (action_triggered_) {
    return;
  }
  action_triggered_ = true;
  if (callbacks_.ok_action) {
    callbacks_.ok_action();
  }
}

void Dialog::trigger_skip() {
  if (action_triggered_) {
    return;
  }
  action_triggered_ = true;
  if (callbacks_.skip_action) {
    callbacks_.skip_action();
  }
}

void Dialog::trigger_cancel() {
  if (action_triggered_) {
    return;
  }
  action_triggered_ = true;
  if (callbacks_.cancel_action) {
    callbacks_.cancel_action();
  } else {
    close();
  }
}

void Dialog::close() {
  if (core_obj_ && lv_obj_is_valid(core_obj_)) {
    lv_obj_delete(core_obj_);
  }
  core_obj_      = nullptr;
  content_       = nullptr;
  button_row_    = nullptr;
  ok_button_     = nullptr;
  skip_button_   = nullptr;
  cancel_button_ = nullptr;
  button_entries_.clear();
  action_triggered_ = false;
}

lv_obj_t* Dialog::content() const { return content_; }

lv_obj_t* Dialog::add_label(const char* text, int32_t width, lv_text_align_t align) {
  if (!content_) {
    return nullptr;
  }
  auto* label = lv_label_create(content_);
  lv_label_set_text(label, text ? text : "");
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, align, 0);
  auto* font = assets_.load_font("inter-medium.ttf", config_.body_font_size);
  lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(label, app_view_model_.dark_mode_subject(), reactive::ThemeRole::TEXT);
  return label;
}

lv_obj_t* Dialog::add_textarea(const char* text, const DialogTextareaOptions& options) {
  if (!content_) {
    return nullptr;
  }
  auto* textarea = lv_textarea_create(content_);
  lv_textarea_set_one_line(textarea, options.one_line);
  lv_textarea_set_text(textarea, text ? text : "");
  if (options.accepted_chars) {
    lv_textarea_set_accepted_chars(textarea, options.accepted_chars);
  }
  lv_obj_set_size(textarea, options.width, options.height);
  auto* font = assets_.load_font("inter-medium.ttf", 11);
  lv_obj_set_style_text_font(textarea, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(textarea, app_view_model_.dark_mode_subject(), reactive::ThemeRole::BUTTON);

  auto* state = new TextareaKeyState{this, options.enter_confirms};
  lv_obj_add_event_cb(
      textarea,
      [](lv_event_t* event) {
        if (lv_event_get_code(event) == LV_EVENT_DELETE) {
          delete static_cast<TextareaKeyState*>(lv_event_get_user_data(event));
        }
      },
      LV_EVENT_DELETE,
      state);
  lv_obj_add_event_cb(textarea, textarea_key_cb, LV_EVENT_KEY, state);
  lv_obj_add_event_cb(textarea, textarea_ready_cb, LV_EVENT_READY, state);
  return textarea;
}

lv_obj_t* Dialog::add_dropdown(const char* options, uint32_t selected, int32_t width) {
  if (!content_) {
    return nullptr;
  }
  auto* dropdown = lv_dropdown_create(content_);
  lv_dropdown_set_options(dropdown, options ? options : "");
  lv_dropdown_set_selected(dropdown, selected);
  lv_dropdown_set_symbol(dropdown, "v");
  lv_obj_set_width(dropdown, width);
  auto* font = assets_.load_font("inter-medium.ttf", 11);
  lv_obj_set_style_text_font(dropdown, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(dropdown, app_view_model_.dark_mode_subject(), reactive::ThemeRole::BUTTON);
  return dropdown;
}

lv_obj_t* Dialog::add_display_field(const char* text, int32_t width, int32_t height) {
  if (!content_) {
    return nullptr;
  }
  auto* box = lv_obj_create(content_);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, width, height);
  lv_obj_set_style_radius(box, 6, 0);
  lv_obj_set_style_pad_left(box, 8, 0);
  lv_obj_set_style_pad_right(box, 8, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(box, app_view_model_.dark_mode_subject(), reactive::ThemeRole::BUTTON);

  auto* label = lv_label_create(box);
  lv_label_set_text(label, text ? text : "");
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(label, width - 16);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
  auto* font = assets_.load_font("inter-medium.ttf", 12);
  lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(label, app_view_model_.dark_mode_subject(), reactive::ThemeRole::TEXT);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
  return label;
}

lv_obj_t* Dialog::ok_button() const { return ok_button_; }

lv_obj_t* Dialog::skip_button() const { return skip_button_; }

lv_obj_t* Dialog::cancel_button() const { return cancel_button_; }

lv_obj_t* Dialog::add_button_(lv_obj_t* parent,
                              const char* text,
                              DialogButtonTone tone,
                              lv_event_cb_t callback) {
  auto* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_set_size(button, config_.button_width, config_.button_height);
  lv_obj_set_style_radius(button, 8, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, this);

  auto* label = lv_label_create(button);
  lv_label_set_text(label, text ? text : "OK");
  auto* font = assets_.load_font("inter-semibold.ttf", 11);
  lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, 0);
  apply_button_tone_(button, label, tone);
  lv_obj_center(label);
  button_entries_.push_back({button, label, tone, ButtonRole::OK});
  return button;
}

void Dialog::apply_button_tone_(lv_obj_t* button, lv_obj_t* label, DialogButtonTone tone) {
  if (!button || !label) {
    return;
  }

  if (tone == DialogButtonTone::DEFAULT) {
    reactive::bind_theme(button, app_view_model_.dark_mode_subject(), reactive::ThemeRole::BUTTON);
    reactive::bind_theme(label, app_view_model_.dark_mode_subject(), reactive::ThemeRole::TEXT);
    return;
  }

  const auto colors = view::palette(app_view_model_.is_dark_mode());
  const auto color  = tone_color_(tone);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button, lv_color_mix(color, colors.button, 54), 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, color, 0);
  lv_obj_set_style_text_color(label, color, 0);
}

lv_color_t Dialog::tone_color_(DialogButtonTone tone) const {
  const auto colors = view::palette(app_view_model_.is_dark_mode());
  switch (tone) {
    case DialogButtonTone::SUCCESS:
      return colors.success;
    case DialogButtonTone::WARNING:
      return colors.warning;
    case DialogButtonTone::ERROR:
      return colors.error;
    case DialogButtonTone::DEFAULT:
    default:
      return colors.primary;
  }
}

void Dialog::update_button_focus_() {
  if (!config_.focus_button_navigation || button_entries_.empty()) {
    return;
  }
  if (focused_button_index_ >= button_entries_.size()) {
    focused_button_index_ = button_entries_.size() - 1;
  }

  const auto colors = view::palette(app_view_model_.is_dark_mode());
  for (std::size_t i = 0; i < button_entries_.size(); ++i) {
    auto& entry = button_entries_[i];
    if (!entry.button || !entry.label) {
      continue;
    }

    const auto tone_color = tone_color_(entry.tone);
    if (i == focused_button_index_) {
      const auto focus_bg = lv_color_mix(tone_color, colors.button, 76);
      view::animate_style_color(entry.button, LV_STYLE_BG_COLOR, focus_bg, 140);
      lv_obj_set_style_bg_opa(entry.button, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(entry.button, 1, 0);
      lv_obj_set_style_border_color(entry.button, tone_color, 0);
      lv_obj_set_style_outline_width(entry.button, 2, 0);
      lv_obj_set_style_outline_pad(entry.button, 0, 0);
      lv_obj_set_style_outline_opa(entry.button, LV_OPA_COVER, 0);
      lv_obj_set_style_outline_color(entry.button, tone_color, 0);
      view::animate_style_color(entry.label, LV_STYLE_TEXT_COLOR, tone_color, 140);
      continue;
    }

    const auto idle_bg = entry.tone == DialogButtonTone::DEFAULT
                             ? colors.button
                             : lv_color_mix(tone_color, colors.button, 54);
    view::animate_style_color(entry.button, LV_STYLE_BG_COLOR, idle_bg, 140);
    lv_obj_set_style_bg_opa(entry.button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(entry.button, 1, 0);
    lv_obj_set_style_border_color(
        entry.button,
        entry.tone == DialogButtonTone::DEFAULT ? colors.border : tone_color,
        0);
    lv_obj_set_style_outline_width(entry.button, 0, 0);
    view::animate_style_color(entry.label, LV_STYLE_TEXT_COLOR, tone_color, 140);
  }
}

void Dialog::apply_dialog_theme_(bool dark_mode) {
  if (!core_obj_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  lv_obj_set_style_bg_color(core_obj_, colors.button, 0);
  lv_obj_set_style_bg_opa(core_obj_, LV_OPA_90, 0);
  lv_obj_set_style_border_color(core_obj_, colors.border, 0);
  lv_obj_set_style_border_width(core_obj_, 1, 0);
  update_button_focus_();
}

void Dialog::apply_theme(bool dark_mode) { apply_dialog_theme_(dark_mode); }

void Dialog::move_button_focus_(int32_t direction) {
  if (button_entries_.empty() || direction == 0) {
    return;
  }

  const auto count = static_cast<int32_t>(button_entries_.size());
  auto next        = static_cast<int32_t>(focused_button_index_) + (direction > 0 ? 1 : -1);
  if (next < 0) {
    next = count - 1;
  } else if (next >= count) {
    next = 0;
  }
  focused_button_index_ = static_cast<std::size_t>(next);
  update_button_focus_();
}

void Dialog::trigger_focused_button_() {
  if (button_entries_.empty() || focused_button_index_ >= button_entries_.size()) {
    return;
  }

  switch (button_entries_[focused_button_index_].role) {
    case ButtonRole::CANCEL:
      trigger_cancel();
      break;
    case ButtonRole::SKIP:
      trigger_skip();
      break;
    case ButtonRole::OK:
    default:
      trigger_ok();
      break;
  }
}

void Dialog::add_title_row_() {
  auto* row = lv_obj_create(core_obj_);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, config_.width - config_.pad_all * 2, 18);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row,
                        LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  auto* title = lv_label_create(row);
  lv_label_set_text(title, config_.show_title ? config_.title.c_str() : "");
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  const int32_t row_width      = config_.width - config_.pad_all * 2;
  const int32_t shortcut_width = config_.shortcut_width > 0
                                     ? std::min(config_.shortcut_width, row_width - 48)
                                     : (config_.width / 2 - config_.pad_all);
  lv_obj_set_width(title, config_.show_shortcuts ? row_width - shortcut_width : row_width);
  auto* title_font = assets_.load_font("inter-semibold.ttf", 12);
  lv_obj_set_style_text_font(title, title_font ? title_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(title, app_view_model_.dark_mode_subject(), reactive::ThemeRole::TEXT);

  if (config_.show_shortcuts) {
    auto* hint = lv_label_create(row);
    lv_label_set_text(hint, config_.shortcut_text.c_str());
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    lv_obj_set_width(hint, shortcut_width);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_RIGHT, 0);
    auto* hint_font = assets_.load_font("inter-medium.ttf", 10);
    lv_obj_set_style_text_font(hint, hint_font ? hint_font : &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, view::palette(app_view_model_.is_dark_mode()).text_muted, 0);
  }
}

void Dialog::add_button_row_() {
  if (!config_.show_ok_button && !config_.show_skip_button && !config_.show_cancel_button) {
    return;
  }

  button_row_ = lv_obj_create(core_obj_);
  lv_obj_remove_style_all(button_row_);
  const int32_t focus_padding = config_.focus_button_navigation ? 4 : 0;
  lv_obj_set_size(button_row_, config_.button_row_width, config_.button_height + focus_padding);
  lv_obj_set_style_pad_left(button_row_, config_.focus_button_navigation ? 3 : 0, 0);
  lv_obj_set_style_pad_right(button_row_, config_.focus_button_navigation ? 3 : 0, 0);
  lv_obj_set_style_pad_top(button_row_, config_.focus_button_navigation ? 2 : 0, 0);
  lv_obj_set_style_pad_bottom(button_row_, config_.focus_button_navigation ? 2 : 0, 0);
  lv_obj_set_flex_flow(button_row_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(
      button_row_,
      (config_.show_ok_button || config_.show_skip_button) && config_.show_cancel_button
          ? LV_FLEX_ALIGN_SPACE_BETWEEN
          : LV_FLEX_ALIGN_CENTER,
      LV_FLEX_ALIGN_CENTER,
      LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(button_row_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(button_row_, LV_ALIGN_BOTTOM_MID, 0, -(config_.pad_all + config_.button_bottom_pad));

  if (config_.show_cancel_button) {
    cancel_button_              = add_button_(button_row_,
                                              config_.cancel_button_label.c_str(),
                                              config_.cancel_button_tone,
                                              cancel_button_cb);
    button_entries_.back().role = ButtonRole::CANCEL;
  }
  if (config_.show_skip_button) {
    skip_button_                = add_button_(button_row_,
                                              config_.skip_button_label.c_str(),
                                              config_.skip_button_tone,
                                              skip_button_cb);
    button_entries_.back().role = ButtonRole::SKIP;
  }
  if (config_.show_ok_button) {
    ok_button_                  = add_button_(button_row_,
                                              config_.ok_button_label.c_str(),
                                              config_.ok_button_tone,
                                              ok_button_cb);
    button_entries_.back().role = ButtonRole::OK;
    focused_button_index_       = button_entries_.size() - 1;
  }
}

bool Dialog::is_ok_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_ENTER || key == '\r' ||
         key_name_is_one_of(key_name, "Enter", "Return", "KEY_96");
}

bool Dialog::is_cancel_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_ESC || key_name_is_one_of(key_name, "Esc", "Escape", "KEY_1") ||
         (config_.use_nav_action_keys && key == '4');
}

bool Dialog::is_focus_previous_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_LEFT || key == 'z' || key == 'Z' ||
         key_name_is_one_of(key_name, "Left", "KEY_105", "Z");
}

bool Dialog::is_focus_next_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_RIGHT || key == 'c' || key == 'C' ||
         key_name_is_one_of(key_name, "Right", "KEY_106", "C");
}

bool Dialog::key_name_is_one_of(const char* key_name, const char* a, const char* b, const char* c) {
  if (!key_name) {
    return false;
  }
  return std::strcmp(key_name, a) == 0 || std::strcmp(key_name, b) == 0 ||
         std::strcmp(key_name, c) == 0;
}

void Dialog::ok_button_cb(lv_event_t* event) {
  auto* dialog = static_cast<Dialog*>(lv_event_get_user_data(event));
  if (dialog) {
    dialog->trigger_ok();
  }
}

void Dialog::skip_button_cb(lv_event_t* event) {
  auto* dialog = static_cast<Dialog*>(lv_event_get_user_data(event));
  if (dialog) {
    dialog->trigger_skip();
  }
}

void Dialog::cancel_button_cb(lv_event_t* event) {
  auto* dialog = static_cast<Dialog*>(lv_event_get_user_data(event));
  if (dialog) {
    dialog->trigger_cancel();
  }
}

void Dialog::textarea_key_cb(lv_event_t* event) {
  auto* state = static_cast<TextareaKeyState*>(lv_event_get_user_data(event));
  if (!state || !state->dialog || !state->dialog->is_ok_key_(lv_event_get_key(event), nullptr)) {
    return;
  }

  lv_event_stop_processing(event);
  const bool should_confirm = state->enter_confirms;
  state->enter_confirms     = false;
  if (should_confirm) {
    state->dialog->trigger_ok();
  }
}

void Dialog::textarea_ready_cb(lv_event_t* event) {
  auto* state = static_cast<TextareaKeyState*>(lv_event_get_user_data(event));
  if (state && state->enter_confirms && state->dialog) {
    state->dialog->trigger_ok();
  }
}

}  // namespace view::widgets
