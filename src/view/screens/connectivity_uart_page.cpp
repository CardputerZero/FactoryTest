/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_uart_page.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <utility>
#include <unistd.h>

#include "bindings.h"
#include "connectivity_subpage_common.h"
#include "gpio_service.h"
#include "linux_input.h"
#include "screenshot_service.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr const char* K_UART_PATH       = "/dev/ttyS0";
constexpr int32_t K_UART_WIDTH          = 308;
constexpr int32_t K_LOG_HEIGHT          = 74;
constexpr int32_t K_INPUT_HEIGHT        = 26;
constexpr std::size_t K_MAX_LOG_LINES   = 120;
constexpr std::size_t K_MAX_INPUT_CHARS = 96;
constexpr std::array<int, 5> K_BAUD_RATES{9600, 19200, 38400, 57600, 115200};
constexpr const char* K_BAUD_OPTIONS = "9600\n19200\n38400\n57600\n115200";

bool is_enter_key(uint32_t key, const char* key_name) {
  if (key == LV_KEY_ENTER || key == '\n' || key == '\r') {
    return true;
  }
  return key_name && (std::strcmp(key_name, "Enter") == 0 || std::strcmp(key_name, "Return") == 0);
}

bool key_name_is(const char* key_name, const char* expected) {
  return key_name && expected && std::strcmp(key_name, expected) == 0;
}

bool is_up_key(uint32_t key, const char* key_name) {
  return key == LV_KEY_UP || key == 'f' || key == 'F' || key_name_is(key_name, "Up");
}

bool is_down_key(uint32_t key, const char* key_name) {
  return key == LV_KEY_DOWN || key == 'x' || key == 'X' || key_name_is(key_name, "Down");
}

std::string escaped_uart_text(const std::string& text) {
  std::ostringstream stream;
  for (unsigned char ch : text) {
    switch (ch) {
      case '\r':
        stream << "\\r";
        break;
      case '\n':
        stream << "\\n";
        break;
      case '\t':
        stream << "\\t";
        break;
      case '\\':
        stream << "\\\\";
        break;
      case '\'':
        stream << "\\'";
        break;
      default:
        if (std::isprint(ch)) {
          stream << static_cast<char>(ch);
        } else {
          char hex[5]{};
          std::snprintf(hex, sizeof(hex), "\\x%02X", ch);
          stream << hex;
        }
        break;
    }
  }
  return stream.str();
}

}  // namespace

UartConnectivityView::UartConnectivityView() = default;

UartConnectivityView::~UartConnectivityView() {
  delete_timer(poll_timer_);
  if (theme_observer_handle_) {
    lv_observer_remove(theme_observer_handle_);
    theme_observer_handle_ = nullptr;
  }
  hide_config_dialog_();
  hold_popup_.reset();
}

void UartConnectivityView::build(lv_obj_t* parent,
                                 lv_obj_t* dialog_parent,
                                 viewmodel::AppViewModel& app_view_model,
                                 app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  dialog_parent_  = dialog_parent;

  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, K_UART_WIDTH, 106);
  lv_obj_align(root_, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(root_, 2, 0);
  lv_obj_set_style_pad_row(root_, 4, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(root_, app_view_model.dark_mode_subject(), reactive::ThemeRole::SURFACE);

  const auto colors = view::palette(app_view_model.is_dark_mode());
  log_view_         = lv_obj_create(root_);
  lv_obj_remove_style_all(log_view_);
  lv_obj_set_size(log_view_, K_UART_WIDTH, K_LOG_HEIGHT);
  lv_obj_set_style_radius(log_view_, 8, 0);
  lv_obj_set_style_bg_color(log_view_, colors.button, 0);
  lv_obj_set_style_bg_opa(log_view_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(log_view_, colors.border, 0);
  lv_obj_set_style_border_width(log_view_, 1, 0);
  lv_obj_set_style_pad_all(log_view_, 5, 0);
  lv_obj_add_flag(log_view_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(log_view_, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(log_view_, LV_SCROLLBAR_MODE_AUTO);

  log_label_ = lv_label_create(log_view_);
  lv_label_set_text(log_label_, "");
  lv_label_set_long_mode(log_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(log_label_, K_UART_WIDTH - 14);
  auto* log_font = assets.load_font("inter-medium.ttf", 10);
  lv_obj_set_style_text_font(log_label_, log_font ? log_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(log_label_, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);

  auto* input_card = lv_obj_create(root_);
  lv_obj_remove_style_all(input_card);
  lv_obj_set_size(input_card, K_UART_WIDTH, K_INPUT_HEIGHT);
  lv_obj_set_style_radius(input_card, 8, 0);
  lv_obj_set_style_bg_color(input_card, colors.button, 0);
  lv_obj_set_style_bg_opa(input_card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(input_card, colors.border, 0);
  lv_obj_set_style_border_width(input_card, 1, 0);
  lv_obj_set_style_pad_left(input_card, 6, 0);
  lv_obj_set_style_pad_right(input_card, 6, 0);
  lv_obj_clear_flag(input_card, LV_OBJ_FLAG_SCROLLABLE);
  input_card_ = input_card;

  input_label_ = lv_label_create(input_card);
  lv_label_set_long_mode(input_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_width(input_label_, K_UART_WIDTH - 12);
  auto* input_font = assets.load_font("inter-medium.ttf", 11);
  lv_obj_set_style_text_font(input_label_, input_font ? input_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(input_label_, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);
  lv_obj_center(input_label_);
  update_input_label_();
  apply_theme_(app_view_model.is_dark_mode());
  theme_observer_handle_ =
      reactive::observe_obj(root_, app_view_model.dark_mode_subject(), theme_observer, this);

  open_session_();
  poll_timer_ = lv_timer_create(poll_timer_cb, 80, this);
}

bool UartConnectivityView::handle_key(uint32_t key, const char* key_name) {
  if (dialog_visible()) {
    return handle_dialog_key_(key, key_name);
  }

  if (is_enter_key(key, key_name)) {
    send_input_();
    return true;
  }
  const auto hold_action = hold_action_for_key_(key);
  if (hold_action != HoldAction::NONE) {
    begin_hold_action_(key, hold_action);
    return true;
  }
  if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
    if (!input_buffer_.empty()) {
      input_buffer_.pop_back();
      update_input_label_();
    }
    return true;
  }
  if (key >= 32 && key <= 126) {
    return append_input_char_(static_cast<char>(key));
  }
  return false;
}

bool UartConnectivityView::handle_key_release(uint32_t key, const char* key_name) {
  LV_UNUSED(key_name);

  if (!pending_hold_matches_(key)) {
    return false;
  }

  const auto action      = pending_hold_action_;
  const auto pressed_key = pending_hold_key_;
  const bool consumed    = pending_hold_consumed_;
  pending_hold_action_   = HoldAction::NONE;
  pending_hold_key_      = 0;
  pending_hold_consumed_ = false;
  hide_hold_popup_();

  if (!consumed && pressed_key >= 32 && pressed_key <= 126) {
    append_input_char_(static_cast<char>(pressed_key));
  }
  return action != HoldAction::NONE;
}

bool UartConnectivityView::handle_long_key(uint32_t key, const char* key_name) {
  LV_UNUSED(key_name);

  const auto action = hold_action_for_key_(key);
  if (action == HoldAction::NONE || !pending_hold_matches_(key)) {
    return false;
  }

  pending_hold_consumed_ = true;
  hide_hold_popup_();
  switch (action) {
    case HoldAction::CLEAR_BUFFER:
      reset_buffers_();
      return true;
    case HoldAction::BAUD_SETTINGS:
      show_config_dialog();
      return true;
    case HoldAction::SCREENSHOT:
      platform::screenshot::capture_active_screen_with_overlay();
      return true;
    case HoldAction::THEME_TOGGLE:
      if (app_view_model_) {
        app_view_model_->toggle_dark_mode();
      }
      return true;
    case HoldAction::EXIT_UART:
      return false;
    case HoldAction::NONE:
    default:
      return false;
  }
}

void UartConnectivityView::show_config_dialog() {
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
  config.title               = "UART Baud Rate";
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

  baud_dropdown_ = dialog_->add_dropdown(K_BAUD_OPTIONS, baud_index_for_rate_(baud_rate_), 196);
  if (baud_dropdown_) {
    lv_obj_add_state(baud_dropdown_, LV_STATE_FOCUSED);
  }
}

bool UartConnectivityView::dialog_visible() const { return dialog_ && dialog_->visible(); }

void UartConnectivityView::open_session_() {
  std::string gpio_error;
  platform::gpio::set_external_bus_uart_mode(gpio_error);

  usleep(100000);

  platform::connectivity::UartOpenResult result;
  session_ = platform::connectivity::UartDebugSession::open(K_UART_PATH, baud_rate_, result);
  if (session_) {
    append_log_("***", std::string{"Opened /dev/ttyS0 @ "} + std::to_string(baud_rate_) + " 8N1");
    return;
  }

  if (result.status == platform::connectivity::UartOpenStatus::OCCUPIED_BY_CONSOLE) {
    show_status_("UART console active\nDisable login shell in raspi-config");
  } else {
    show_status_(result.message.empty() ? "Unable to open /dev/ttyS0" : result.message);
  }
}

void UartConnectivityView::poll_rx_() {
  if (!session_) {
    return;
  }
  std::string error;
  const auto data = session_->read_available(error);
  if (!data.empty()) {
    append_log_("<<<", data);
  }
  if (!error.empty()) {
    append_log_("***", error);
  }
}

void UartConnectivityView::append_log_(const char* prefix, const std::string& text) {
  std::string line = std::string{prefix ? prefix : "***"} + " '" + escaped_uart_text(text) + "'";
  log_lines_.push_back(std::move(line));
  if (log_lines_.size() > K_MAX_LOG_LINES) {
    log_lines_.erase(
        log_lines_.begin(),
        log_lines_.begin() + static_cast<std::ptrdiff_t>(log_lines_.size() - K_MAX_LOG_LINES));
  }
  refresh_log_label_();
}

void UartConnectivityView::refresh_log_label_() {
  if (!log_label_) {
    return;
  }
  std::string text;
  for (const auto& line : log_lines_) {
    if (!text.empty()) {
      text.push_back('\n');
    }
    text += line;
  }
  lv_label_set_text(log_label_, text.c_str());
  if (log_view_) {
    lv_obj_update_layout(log_view_);
    lv_obj_scroll_to_y(log_view_,
                       lv_obj_get_scroll_y(log_view_) + lv_obj_get_scroll_bottom(log_view_),
                       LV_ANIM_OFF);
  }
}

void UartConnectivityView::update_input_label_() {
  if (!input_label_) {
    return;
  }
  const std::string text = std::string{"> "} + input_buffer_;
  lv_label_set_text(input_label_, text.c_str());
}

void UartConnectivityView::send_input_() {
  if (input_buffer_.empty()) {
    return;
  }
  const auto payload = std::exchange(input_buffer_, {});
  update_input_label_();
  append_log_(">>>", payload);

  if (!session_) {
    append_log_("***", "UART is not open");
    return;
  }

  std::string error;
  if (!session_->write_text(payload, error) && !error.empty()) {
    append_log_("***", error);
  }
}

void UartConnectivityView::reset_buffers_() {
  input_buffer_.clear();
  update_input_label_();

  if (session_) {
    std::string error;
    session_->read_available(error);
  }

  log_lines_.clear();
  refresh_log_label_();
  append_log_("***", "UART buffers reset");
}

void UartConnectivityView::show_status_(const std::string& message) {
  if (!root_ || !app_view_model_ || !assets_) {
    return;
  }
  if (log_view_) {
    lv_obj_add_flag(log_view_, LV_OBJ_FLAG_HIDDEN);
  }
  if (input_label_) {
    auto* input_card = lv_obj_get_parent(input_label_);
    if (input_card) {
      lv_obj_add_flag(input_card, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (status_label_ && lv_obj_is_valid(status_label_)) {
    lv_obj_delete(status_label_);
  }
  status_label_ = lv_label_create(root_);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_label_set_text(status_label_, message.c_str());
  lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(status_label_, 260);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  auto* font = assets_->load_font("inter-semibold.ttf", 12);
  lv_obj_set_style_text_font(status_label_, font ? font : &lv_font_montserrat_12, 0);
  const auto colors = view::palette(app_view_model_->is_dark_mode());
  lv_obj_set_style_text_color(status_label_, colors.warning, 0);
  lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
}

void UartConnectivityView::hide_config_dialog_() {
  dialog_.reset();
  baud_dropdown_ = nullptr;
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
}

void UartConnectivityView::apply_config_dialog_() {
  const int new_baud = selected_baud_rate_();
  hide_config_dialog_();

  baud_rate_ = new_baud;
  if (session_) {
    std::string error;
    if (session_->set_baud_rate(baud_rate_, error)) {
      append_log_("***", std::string{"Baud set to "} + std::to_string(baud_rate_));
    } else {
      append_log_("***", error.empty() ? "Failed to set baud" : error);
    }
  } else {
    if (status_label_ && lv_obj_is_valid(status_label_)) {
      lv_obj_delete(status_label_);
      status_label_ = nullptr;
    }
    lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (log_view_) {
      lv_obj_remove_flag(log_view_, LV_OBJ_FLAG_HIDDEN);
    }
    if (input_label_) {
      auto* input_card = lv_obj_get_parent(input_label_);
      if (input_card) {
        lv_obj_remove_flag(input_card, LV_OBJ_FLAG_HIDDEN);
      }
    }
    open_session_();
  }
}

bool UartConnectivityView::handle_dialog_key_(uint32_t key, const char* key_name) {
  if (is_up_key(key, key_name)) {
    step_baud_selection_(-1);
    return true;
  }
  if (is_down_key(key, key_name)) {
    step_baud_selection_(1);
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

void UartConnectivityView::step_baud_selection_(int delta) {
  if (!baud_dropdown_) {
    return;
  }
  const auto count = static_cast<int>(K_BAUD_RATES.size());
  auto index       = static_cast<int>(lv_dropdown_get_selected(baud_dropdown_));
  index            = (index + delta + count) % count;
  lv_dropdown_set_selected(baud_dropdown_, static_cast<uint32_t>(index));
}

int UartConnectivityView::selected_baud_rate_() const {
  if (!baud_dropdown_) {
    return baud_rate_;
  }
  const auto selected = lv_dropdown_get_selected(baud_dropdown_);
  if (selected >= K_BAUD_RATES.size()) {
    return baud_rate_;
  }
  return K_BAUD_RATES[selected];
}

uint32_t UartConnectivityView::baud_index_for_rate_(int baud_rate) const {
  const auto it = std::find(K_BAUD_RATES.begin(), K_BAUD_RATES.end(), baud_rate);
  if (it == K_BAUD_RATES.end()) {
    return 0;
  }
  return static_cast<uint32_t>(it - K_BAUD_RATES.begin());
}

void UartConnectivityView::show_hold_popup_(HoldAction action) {
  if (!dialog_parent_ || !app_view_model_) {
    return;
  }
  if (!hold_popup_) {
    view::widgets::PopupConfig config;
    config.width       = 250;
    config.label_width = 234;
    config.tone        = view::widgets::PopupTone::WARNING;
    hold_popup_ = std::make_unique<view::widgets::Popup>(dialog_parent_, *app_view_model_, config);
    hold_popup_->build();
  }

  const char* message = "";
  switch (action) {
    case HoldAction::CLEAR_BUFFER:
      message = "hold R to clear buffer";
      break;
    case HoldAction::BAUD_SETTINGS:
      message = "hold 6 to open baud rate";
      break;
    case HoldAction::SCREENSHOT:
      message = "hold P to screenshot";
      break;
    case HoldAction::THEME_TOGGLE:
      message = "hold T to switch theme";
      break;
    case HoldAction::EXIT_UART:
      message = "hold ESC/4 to exit UART";
      break;
    case HoldAction::NONE:
    default:
      break;
  }
  hold_popup_->set_text(message);
  hold_popup_->show();
}

void UartConnectivityView::hide_hold_popup_() {
  if (hold_popup_) {
    hold_popup_->hide();
  }
}

UartConnectivityView::HoldAction UartConnectivityView::hold_action_for_key_(uint32_t key) const {
  if (key == 'r' || key == 'R') {
    return HoldAction::CLEAR_BUFFER;
  }
  if (key == '6') {
    return HoldAction::BAUD_SETTINGS;
  }
  if (key == 'p' || key == 'P') {
    return HoldAction::SCREENSHOT;
  }
  if (key == 't' || key == 'T') {
    return HoldAction::THEME_TOGGLE;
  }
  if (key == LV_KEY_ESC || key == 27 || key == '4') {
    return HoldAction::EXIT_UART;
  }
  return HoldAction::NONE;
}

void UartConnectivityView::begin_hold_action_(uint32_t key, HoldAction action) {
  pending_hold_key_      = key;
  pending_hold_action_   = action;
  pending_hold_consumed_ = false;
  show_hold_popup_(action);
}

bool UartConnectivityView::pending_hold_matches_(uint32_t key) const {
  if (pending_hold_action_ == HoldAction::NONE) {
    return false;
  }
  return hold_action_for_key_(key) == pending_hold_action_;
}

bool UartConnectivityView::append_input_char_(char ch) {
  if (input_buffer_.size() >= K_MAX_INPUT_CHARS) {
    return true;
  }
  input_buffer_.push_back(ch);
  update_input_label_();
  return true;
}

void UartConnectivityView::apply_theme_(bool dark_mode) {
  const auto colors = view::palette(dark_mode);
  if (log_view_) {
    lv_obj_set_style_bg_color(log_view_, colors.button, 0);
    lv_obj_set_style_border_color(log_view_, colors.border, 0);
  }
  if (input_card_) {
    lv_obj_set_style_bg_color(input_card_, colors.button, 0);
    lv_obj_set_style_border_color(input_card_, colors.border, 0);
  }
  if (status_label_) {
    lv_obj_set_style_text_color(status_label_, colors.warning, 0);
  }
}

void UartConnectivityView::poll_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<UartConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->poll_rx_();
  }
}

void UartConnectivityView::theme_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* view = static_cast<UartConnectivityView*>(lv_observer_get_user_data(observer));
  if (view) {
    view->apply_theme_(lv_subject_get_int(subject) != 0);
  }
}

}  // namespace screen
