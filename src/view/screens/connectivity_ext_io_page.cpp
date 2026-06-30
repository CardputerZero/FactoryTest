/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_ext_io_page.h"

#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_PANEL_WIDTH           = 304;
constexpr int32_t K_ROW_WIDTH             = 300;
constexpr int32_t K_ROW_HEIGHT            = 32;
constexpr int32_t K_ROW_SPACING           = 10;
constexpr int32_t K_ICON_WIDTH            = 22;
constexpr int32_t K_TITLE_WIDTH           = 76;
constexpr int32_t K_DETAIL_WIDTH          = 92;
constexpr int32_t K_STATE_WIDTH           = 34;
constexpr int32_t K_LED_SIZE              = 12;
constexpr int32_t K_SWITCH_WIDTH          = 38;
constexpr int32_t K_SWITCH_HEIGHT         = 20;
constexpr uint32_t K_GPIO_ACTION_DELAY_MS = 120;
constexpr uint32_t K_INPUT_POLL_MS        = 250;
constexpr const char* K_FUNCTION_OPTIONS  = "Output\nInput";

bool key_name_is(const char* key_name, const char* expected) {
  return key_name && expected && std::strcmp(key_name, expected) == 0;
}

bool is_up_key(uint32_t key, const char* key_name) {
  return key == LV_KEY_UP || key == 'f' || key == 'F' || key_name_is(key_name, "Up");
}

bool is_down_key(uint32_t key, const char* key_name) {
  return key == LV_KEY_DOWN || key == 'x' || key == 'X' || key_name_is(key_name, "Down");
}

bool is_enter_key(uint32_t key, const char* key_name) {
  if (key == LV_KEY_ENTER || key == '\n' || key == '\r') {
    return true;
  }
  return key_name && (std::strcmp(key_name, "Enter") == 0 || std::strcmp(key_name, "Return") == 0);
}

std::string compact_error(const std::string& error) {
  if (error.empty()) {
    return "GPIO error";
  }
  constexpr std::size_t K_MAX_ERROR_LEN = 48;
  return error.size() <= K_MAX_ERROR_LEN ? error : error.substr(0, K_MAX_ERROR_LEN - 3) + "...";
}

}  // namespace

ExtIoConnectivityView::~ExtIoConnectivityView() {
  if (theme_observer_handle_) {
    lv_observer_remove(theme_observer_handle_);
    theme_observer_handle_ = nullptr;
  }
  if (input_poll_timer_) {
    lv_timer_delete(input_poll_timer_);
    input_poll_timer_ = nullptr;
  }
}

void ExtIoConnectivityView::build(lv_obj_t* parent,
                                  lv_obj_t* dialog_parent,
                                  viewmodel::AppViewModel& app_view_model,
                                  app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  dialog_parent_  = dialog_parent;
  rows_           = {{
      {"EXT 5V",
       "gpiochip1 line 12",
       view::ICON_LIGHTNING,
       {"/dev/gpiochip1", 12, "factory-test-extio-5v"},
       false},
      {"GPIO26",
       "gpiochip0 line 26",
       view::ICON_PLUGS_CONNECTED,
       {"/dev/gpiochip0", 26, "factory-test-extio-gpio26"}},
      {"GPIO23",
       "gpiochip0 line 23",
       view::ICON_PLUGS_CONNECTED,
       {"/dev/gpiochip0", 23, "factory-test-extio-gpio23"}},
      {"GPIO22",
       "gpiochip0 line 22",
       view::ICON_PLUGS_CONNECTED,
       {"/dev/gpiochip0", 22, "factory-test-extio-gpio22"}},
  }};

  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_width(root_, K_PANEL_WIDTH);
  lv_obj_set_height(root_, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(root_, K_ROW_SPACING, 0);
  lv_obj_set_style_pad_top(root_, 4, 0);
  lv_obj_set_style_pad_bottom(root_, 4, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(root_, LV_ALIGN_TOP_MID, 0, 0);
  reactive::bind_theme(root_, app_view_model.dark_mode_subject(), reactive::ThemeRole::SURFACE);

  auto* icon_font   = assets.load_font("Phosphor-Fill.ttf", 14);
  auto* title_font  = assets.load_font("inter-semibold.ttf", 11);
  auto* detail_font = assets.load_font("inter-medium.ttf", 10);

  for (std::size_t i = 0; i < rows_.size(); ++i) {
    auto& row = rows_[i];
    row.row   = lv_obj_create(root_);
    lv_obj_remove_style_all(row.row);
    lv_obj_set_size(row.row, K_ROW_WIDTH, K_ROW_HEIGHT);
    lv_obj_set_flex_flow(row.row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row.row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(row.row, 6, 0);
    lv_obj_set_style_pad_right(row.row, 4, 0);
    lv_obj_set_style_pad_column(row.row, 4, 0);
    lv_obj_set_style_radius(row.row, 6, 0);
    lv_obj_clear_flag(row.row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row.row, row_clicked_cb, LV_EVENT_CLICKED, this);

    row.icon_label = lv_label_create(row.row);
    lv_label_set_text(row.icon_label, row.icon ? row.icon : view::ICON_INFO);
    lv_obj_set_width(row.icon_label, K_ICON_WIDTH);
    lv_obj_set_style_text_align(row.icon_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(row.icon_label, icon_font ? icon_font : &lv_font_montserrat_14, 0);

    row.title_label = lv_label_create(row.row);
    lv_label_set_text(row.title_label, row.title ? row.title : "GPIO");
    lv_obj_set_width(row.title_label, K_TITLE_WIDTH);
    lv_label_set_long_mode(row.title_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(row.title_label,
                               title_font ? title_font : &lv_font_montserrat_12,
                               0);

    row.detail_label = lv_label_create(row.row);
    lv_label_set_text(row.detail_label, row.detail ? row.detail : "line");
    lv_obj_set_width(row.detail_label, K_DETAIL_WIDTH);
    lv_label_set_long_mode(row.detail_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(row.detail_label,
                               detail_font ? detail_font : &lv_font_montserrat_12,
                               0);

    row.state_label = lv_label_create(row.row);
    lv_label_set_text(row.state_label, "OFF");
    lv_obj_set_width(row.state_label, K_STATE_WIDTH);
    lv_obj_set_style_text_align(row.state_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(row.state_label,
                               title_font ? title_font : &lv_font_montserrat_12,
                               0);

    row.led_obj = lv_obj_create(row.row);
    lv_obj_remove_style_all(row.led_obj);
    lv_obj_set_size(row.led_obj, K_LED_SIZE, K_LED_SIZE);
    lv_obj_set_style_radius(row.led_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(row.led_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row.led_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row.led_obj, LV_OBJ_FLAG_HIDDEN);

    row.switch_obj = lv_switch_create(row.row);
    lv_obj_set_size(row.switch_obj, K_SWITCH_WIDTH, K_SWITCH_HEIGHT);
    lv_obj_clear_flag(row.switch_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row.switch_obj, row_clicked_cb, LV_EVENT_CLICKED, this);
  }

  theme_observer_handle_ =
      reactive::observe_obj(root_, app_view_model.dark_mode_subject(), theme_observer, this);
  read_output_states_();
  apply_theme_();
  update_poll_timer_();
  scroll_selected_to_view_();
}

bool ExtIoConnectivityView::handle_key(uint32_t key, const char* key_name) {
  if (dialog_visible()) {
    return handle_dialog_key_(key, key_name);
  }

  switch (key) {
    case LV_KEY_UP:
    case LV_KEY_LEFT:
    case 'f':
    case 'F':
      select_previous_();
      return true;
    case LV_KEY_DOWN:
    case LV_KEY_RIGHT:
    case 'x':
    case 'X':
      select_next_();
      return true;
    case LV_KEY_ENTER:
      toggle_selected_();
      return true;
    case '6':
      show_config_dialog();
      return true;
    default:
      return false;
  }
}

void ExtIoConnectivityView::select_previous_() {
  auto index = selected_index_;
  while (index > 0) {
    --index;
    if (row_visible_(index)) {
      set_selected_index_(index);
      return;
    }
  }
}

void ExtIoConnectivityView::select_next_() {
  for (auto index = selected_index_ + 1; index < rows_.size(); ++index) {
    if (row_visible_(index)) {
      set_selected_index_(index);
      return;
    }
  }
}

void ExtIoConnectivityView::set_selected_index_(std::size_t index) {
  if (index >= rows_.size() || !row_visible_(index) || selected_index_ == index) {
    return;
  }
  selected_index_ = index;
  apply_theme_();
  scroll_selected_to_view_();
}

void ExtIoConnectivityView::toggle_selected_() { toggle_row_(selected_index_); }

void ExtIoConnectivityView::toggle_row_(std::size_t index) {
  if (io_function_ != IoFunction::OUTPUT || index >= rows_.size() || !row_visible_(index)) {
    return;
  }

  auto& row       = rows_[index];
  const bool next = !row.active;
  std::string error;
  wait_for_gpio_slot_();
  const bool ok = platform::gpio::set_output_value(row.gpio, next, error);
  row.error     = !ok;
  if (ok) {
    row.active = next;
    row.error_message.clear();
  } else {
    row.error_message = compact_error(error);
  }
  apply_row_style_(row);
}

void ExtIoConnectivityView::read_output_states_() {
  for (auto& row : rows_) {
    bool active = false;
    std::string error;
    const bool ok = platform::gpio::get_output_value(row.gpio, active, error);
    row.error     = !ok;
    if (ok) {
      row.active = active;
      row.error_message.clear();
    } else {
      row.active        = false;
      row.error_message = compact_error(error);
    }
  }
}

void ExtIoConnectivityView::read_input_states_() {
  for (auto& row : rows_) {
    if (!row.supports_input) {
      continue;
    }

    bool active = false;
    std::string error;
    const bool ok = platform::gpio::get_input_value(row.gpio, active, error);
    row.error     = !ok;
    if (ok) {
      row.active = active;
      row.error_message.clear();
    } else {
      row.active        = false;
      row.error_message = compact_error(error);
    }
  }
}

void ExtIoConnectivityView::wait_for_gpio_slot_() {
  if (has_gpio_action_) {
    const uint32_t elapsed = lv_tick_elaps(last_gpio_action_at_);
    if (elapsed < K_GPIO_ACTION_DELAY_MS) {
      std::this_thread::sleep_for(std::chrono::milliseconds(K_GPIO_ACTION_DELAY_MS - elapsed));
    }
  }
  last_gpio_action_at_ = lv_tick_get();
  has_gpio_action_     = true;
}

void ExtIoConnectivityView::apply_theme_() {
  ensure_selected_visible_();
  for (auto& row : rows_) {
    apply_row_style_(row);
  }
}

void ExtIoConnectivityView::apply_row_style_(SwitchRow& row) {
  if (!app_view_model_ || !row.row) {
    return;
  }

  const auto colors     = view::palette(app_view_model_->is_dark_mode());
  const bool focused    = &row == &rows_[selected_index_];
  const bool input_mode = io_function_ == IoFunction::INPUT;
  const auto row_bg = focused ? lv_color_mix(colors.primary, colors.surface, 36) : colors.button;
  const auto row_border = focused ? colors.primary : colors.border;

  if (row.row) {
    if (input_mode && !row.supports_input) {
      lv_obj_add_flag(row.row, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(row.row, LV_OBJ_FLAG_HIDDEN);
    }
  }

  lv_obj_set_style_bg_opa(row.row, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(row.row, row_bg, 0);
  lv_obj_set_style_border_width(row.row, focused ? 2 : 1, 0);
  lv_obj_set_style_border_color(row.row, row_border, 0);
  lv_obj_set_style_outline_width(row.row, focused ? 1 : 0, 0);
  lv_obj_set_style_outline_color(row.row, lv_color_mix(colors.primary, colors.surface, 96), 0);
  lv_obj_set_style_outline_pad(row.row, 0, 0);

  if (row.icon_label) {
    lv_obj_set_style_text_color(row.icon_label, row.active ? colors.success : colors.primary, 0);
  }
  if (row.title_label) {
    lv_obj_set_style_text_color(row.title_label, colors.text, 0);
  }
  if (row.detail_label) {
    lv_label_set_text(row.detail_label,
                      row.error ? row.error_message.c_str() : (row.detail ? row.detail : "line"));
    lv_obj_set_style_text_color(row.detail_label, row.error ? colors.error : colors.text_muted, 0);
  }
  if (row.state_label) {
    lv_label_set_text(
        row.state_label,
        row.error ? "ERR"
                  : (input_mode ? (row.active ? "HIGH" : "LOW") : (row.active ? "ON" : "OFF")));
    lv_obj_set_style_text_color(
        row.state_label,
        row.error ? colors.error : (row.active ? colors.success : colors.text_muted),
        0);
  }
  if (row.led_obj) {
    if (input_mode && row.supports_input) {
      lv_obj_remove_flag(row.led_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(row.led_obj, LV_OBJ_FLAG_HIDDEN);
    }
    const auto led_color = row.error    ? colors.error
                           : row.active ? colors.success
                                        : lv_color_mix(colors.text_muted, colors.button, 128);
    lv_obj_set_style_bg_opa(row.led_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row.led_obj, led_color, 0);
    lv_obj_set_style_border_width(row.led_obj, 1, 0);
    lv_obj_set_style_border_color(row.led_obj,
                                  row.active ? colors.success_hover : colors.border,
                                  0);
  }
  if (row.switch_obj) {
    if (input_mode) {
      lv_obj_add_flag(row.switch_obj, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    lv_obj_remove_flag(row.switch_obj, LV_OBJ_FLAG_HIDDEN);
    if (row.active) {
      lv_obj_add_state(row.switch_obj, LV_STATE_CHECKED);
    } else {
      lv_obj_remove_state(row.switch_obj, LV_STATE_CHECKED);
    }
    const auto off_indicator =
        lv_color_mix(colors.text_muted, colors.button, app_view_model_->is_dark_mode() ? 144 : 96);
    const auto on_indicator =
        app_view_model_->is_dark_mode() ? colors.success_hover : colors.success;
    lv_obj_set_style_bg_opa(row.switch_obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row.switch_obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(row.switch_obj, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(row.switch_obj, colors.button, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row.switch_obj, on_indicator, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(row.switch_obj, off_indicator, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(row.switch_obj, colors.surface, LV_PART_KNOB);
    lv_obj_set_style_border_width(row.switch_obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row.switch_obj,
                                  focused ? colors.primary : colors.border,
                                  LV_PART_MAIN);
  }
}

void ExtIoConnectivityView::scroll_selected_to_view_() {
  if (selected_index_ >= rows_.size() || !rows_[selected_index_].row) {
    return;
  }

  lv_obj_update_layout(root_);
  lv_obj_scroll_to_view_recursive(rows_[selected_index_].row, LV_ANIM_ON);
}

void ExtIoConnectivityView::show_config_dialog() {
  if (!dialog_parent_ || !app_view_model_ || !assets_) {
    return;
  }
  if (dialog_visible()) {
    return;
  }

  platform::set_nav_trigger_mode(platform::NavTriggerMode::LONG_PRESS);
  view::widgets::DialogConfig config;
  config.width               = 244;
  config.height              = 112;
  config.title               = "EXT.IO Function";
  config.shortcut_text       = "F/X: Select  Enter: Save";
  config.use_nav_action_keys = false;

  view::widgets::DialogCallbacks callbacks;
  callbacks.ok_action     = [this]() { apply_config_dialog_(); };
  callbacks.cancel_action = [this]() { hide_config_dialog_(); };
  dialog_                 = std::make_unique<view::widgets::Dialog>(dialog_parent_,
                                                                    *app_view_model_,
                                                                    *assets_,
                                                                    std::move(config),
                                                                    std::move(callbacks));
  dialog_->build();

  function_dropdown_ = dialog_->add_dropdown(K_FUNCTION_OPTIONS, function_index_(), 196);
  if (function_dropdown_) {
    lv_obj_add_state(function_dropdown_, LV_STATE_FOCUSED);
  }
}

bool ExtIoConnectivityView::dialog_visible() const { return dialog_ && dialog_->visible(); }

void ExtIoConnectivityView::hide_config_dialog_() {
  dialog_.reset();
  function_dropdown_ = nullptr;
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
}

void ExtIoConnectivityView::apply_config_dialog_() {
  const auto next_function = selected_function_();
  hide_config_dialog_();

  if (next_function == io_function_) {
    return;
  }

  io_function_ = next_function;
  if (io_function_ == IoFunction::INPUT) {
    read_input_states_();
  } else {
    read_output_states_();
  }
  ensure_selected_visible_();
  apply_theme_();
  update_poll_timer_();
  scroll_selected_to_view_();
}

bool ExtIoConnectivityView::handle_dialog_key_(uint32_t key, const char* key_name) {
  if (is_up_key(key, key_name)) {
    step_function_selection_(-1);
    return true;
  }
  if (is_down_key(key, key_name)) {
    step_function_selection_(1);
    return true;
  }
  if (is_enter_key(key, key_name)) {
    apply_config_dialog_();
    return true;
  }
  if (key == LV_KEY_ESC || key == 27 || key == '4') {
    hide_config_dialog_();
    return true;
  }
  if (dialog_ && dialog_->handle_key(key, key_name)) {
    return true;
  }
  return true;
}

void ExtIoConnectivityView::step_function_selection_(int delta) {
  if (!function_dropdown_) {
    return;
  }
  constexpr int K_FUNCTION_COUNT = 2;
  auto index                     = static_cast<int>(lv_dropdown_get_selected(function_dropdown_));
  index                          = (index + delta + K_FUNCTION_COUNT) % K_FUNCTION_COUNT;
  lv_dropdown_set_selected(function_dropdown_, static_cast<uint32_t>(index));
}

uint32_t ExtIoConnectivityView::function_index_() const {
  return io_function_ == IoFunction::INPUT ? 1 : 0;
}

ExtIoConnectivityView::IoFunction ExtIoConnectivityView::selected_function_() const {
  if (!function_dropdown_) {
    return io_function_;
  }
  return lv_dropdown_get_selected(function_dropdown_) == 1 ? IoFunction::INPUT : IoFunction::OUTPUT;
}

bool ExtIoConnectivityView::row_visible_(std::size_t index) const {
  if (index >= rows_.size()) {
    return false;
  }
  return io_function_ == IoFunction::OUTPUT || rows_[index].supports_input;
}

void ExtIoConnectivityView::ensure_selected_visible_() {
  if (row_visible_(selected_index_)) {
    return;
  }
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    if (row_visible_(index)) {
      selected_index_ = index;
      return;
    }
  }
}

void ExtIoConnectivityView::update_poll_timer_() {
  if (io_function_ != IoFunction::INPUT) {
    if (input_poll_timer_) {
      lv_timer_pause(input_poll_timer_);
    }
    return;
  }

  if (!input_poll_timer_) {
    input_poll_timer_ = lv_timer_create(input_poll_timer_cb, K_INPUT_POLL_MS, this);
    lv_timer_set_auto_delete(input_poll_timer_, false);
  }
  lv_timer_resume(input_poll_timer_);
  lv_timer_reset(input_poll_timer_);
}

void ExtIoConnectivityView::row_clicked_cb(lv_event_t* event) {
  auto* view = static_cast<ExtIoConnectivityView*>(lv_event_get_user_data(event));
  auto* obj  = lv_event_get_target_obj(event);
  if (!view || !obj) {
    return;
  }

  for (std::size_t i = 0; i < view->rows_.size(); ++i) {
    const auto& row = view->rows_[i];
    if (obj == row.row || obj == row.switch_obj) {
      view->set_selected_index_(i);
      if (view->io_function_ == IoFunction::OUTPUT) {
        view->toggle_row_(i);
      }
      return;
    }
  }
}

void ExtIoConnectivityView::input_poll_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<ExtIoConnectivityView*>(lv_timer_get_user_data(timer));
  if (!view || view->io_function_ != IoFunction::INPUT) {
    return;
  }
  view->read_input_states_();
  view->apply_theme_();
}

void ExtIoConnectivityView::theme_observer(lv_observer_t* observer, lv_subject_t* subject) {
  LV_UNUSED(subject);
  auto* view = static_cast<ExtIoConnectivityView*>(lv_observer_get_user_data(observer));
  if (view) {
    view->apply_theme_();
  }
}

}  // namespace screen
