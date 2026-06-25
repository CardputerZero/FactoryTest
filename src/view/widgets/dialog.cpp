/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "dialog.h"

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

  const auto colors = view::palette(app_view_model_.is_dark_mode());
  core_obj_         = lv_obj_create(parent_);
  lv_obj_remove_style_all(core_obj_);
  lv_obj_set_size(core_obj_, config_.width, config_.height);
  lv_obj_align(core_obj_, config_.align, config_.offset_x, config_.offset_y);
  lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(core_obj_, config_.pad_all, 0);
  lv_obj_set_style_pad_row(core_obj_, config_.pad_row, 0);
  lv_obj_set_style_radius(core_obj_, 12, 0);
  lv_obj_set_style_bg_color(core_obj_, colors.button, 0);
  lv_obj_set_style_bg_opa(core_obj_, LV_OPA_90, 0);
  lv_obj_set_style_border_color(core_obj_, colors.border, 0);
  lv_obj_set_style_border_width(core_obj_, 1, 0);
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
  lv_obj_set_width(content_, config_.width - config_.pad_all * 2);
  lv_obj_set_height(content_, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(content_, config_.pad_row, 0);
  lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);

  add_button_row_();
  lv_obj_move_foreground(core_obj_);
}

bool Dialog::visible() const { return core_obj_ && lv_obj_is_valid(core_obj_); }

bool Dialog::handle_key(uint32_t key, const char* key_name) {
  if (!visible()) {
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

void Dialog::set_cancel_action(std::function<void()> action) {
  callbacks_.cancel_action = std::move(action);
}

void Dialog::trigger_ok() {
  if (callbacks_.ok_action) {
    callbacks_.ok_action();
  }
}

void Dialog::trigger_cancel() {
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
  cancel_button_ = nullptr;
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
  auto* font = assets_.load_font("inter-medium.ttf", 10);
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

lv_obj_t* Dialog::cancel_button() const { return cancel_button_; }

lv_obj_t* Dialog::add_button_(lv_obj_t* parent, const char* text, lv_event_cb_t callback) {
  auto* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_set_size(button, 78, 24);
  lv_obj_set_style_radius(button, 8, 0);
  reactive::bind_theme(button, app_view_model_.dark_mode_subject(), reactive::ThemeRole::BUTTON);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, this);

  auto* label = lv_label_create(button);
  lv_label_set_text(label, text ? text : "OK");
  auto* font = assets_.load_font("inter-semibold.ttf", 11);
  lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(label, app_view_model_.dark_mode_subject(), reactive::ThemeRole::TEXT);
  lv_obj_center(label);
  return button;
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
  lv_obj_set_width(title,
                   config_.show_shortcuts ? (config_.width / 2 - config_.pad_all)
                                          : (config_.width - config_.pad_all * 2));
  auto* title_font = assets_.load_font("inter-semibold.ttf", 12);
  lv_obj_set_style_text_font(title, title_font ? title_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(title, app_view_model_.dark_mode_subject(), reactive::ThemeRole::TEXT);

  if (config_.show_shortcuts) {
    auto* hint = lv_label_create(row);
    lv_label_set_text(hint, config_.shortcut_text.c_str());
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    lv_obj_set_width(hint, config_.width / 2 - config_.pad_all);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_RIGHT, 0);
    auto* hint_font = assets_.load_font("inter-medium.ttf", 10);
    lv_obj_set_style_text_font(hint, hint_font ? hint_font : &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, view::palette(app_view_model_.is_dark_mode()).text_muted, 0);
  }
}

void Dialog::add_button_row_() {
  if (!config_.show_ok_button && !config_.show_cancel_button) {
    return;
  }

  button_row_ = lv_obj_create(core_obj_);
  lv_obj_remove_style_all(button_row_);
  lv_obj_set_size(button_row_, 190, 26);
  lv_obj_set_flex_flow(button_row_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_row_,
                        config_.show_ok_button && config_.show_cancel_button
                            ? LV_FLEX_ALIGN_SPACE_BETWEEN
                            : LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(button_row_, LV_OBJ_FLAG_SCROLLABLE);

  if (config_.show_cancel_button) {
    cancel_button_ =
        add_button_(button_row_, config_.cancel_button_label.c_str(), cancel_button_cb);
  }
  if (config_.show_ok_button) {
    ok_button_ = add_button_(button_row_, config_.ok_button_label.c_str(), ok_button_cb);
  }
}

bool Dialog::is_ok_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_ENTER || key == '\n' || key == '\r' ||
         key_name_is_one_of_(key_name, "Enter", "Return", "KEY_96");
}

bool Dialog::is_cancel_key_(uint32_t key, const char* key_name) const {
  return key == LV_KEY_ESC || key == 27 ||
         key_name_is_one_of_(key_name, "Esc", "Escape", "KEY_1") ||
         (config_.use_nav_action_keys && key == '4');
}

bool Dialog::key_name_is_one_of_(const char* key_name,
                                 const char* a,
                                 const char* b,
                                 const char* c) {
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
