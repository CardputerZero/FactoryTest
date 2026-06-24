/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_uart_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

#include "bindings.h"
#include "connectivity_subpage_common.h"
#include "linux_input.h"
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

bool is_enter_key(uint32_t key, const char* key_name) {
  if (key == LV_KEY_ENTER || key == '\n' || key == '\r') {
    return true;
  }
  return key_name && (std::strcmp(key_name, "Enter") == 0 || std::strcmp(key_name, "Return") == 0);
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
  hide_config_dialog_();
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

  input_label_ = lv_label_create(input_card);
  lv_label_set_long_mode(input_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_width(input_label_, K_UART_WIDTH - 12);
  auto* input_font = assets.load_font("inter-medium.ttf", 11);
  lv_obj_set_style_text_font(input_label_, input_font ? input_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(input_label_, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);
  lv_obj_center(input_label_);
  update_input_label_();

  open_session_();
  poll_timer_ = lv_timer_create(poll_timer_cb, 80, this);
}

bool UartConnectivityView::handle_key(uint32_t key, const char* key_name) {
  if (dialog_visible()) {
    if (is_enter_key(key, key_name)) {
      apply_config_dialog_();
      return true;
    }
    if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
      if (baud_input_) {
        lv_textarea_delete_char(baud_input_);
      }
      return true;
    }
    if (dialog_ && dialog_->handle_key(key, key_name)) {
      return true;
    }
    if (key >= '0' && key <= '9') {
      return append_dialog_char_(static_cast<char>(key));
    }
    return true;
  }

  if (key == '6') {
    show_config_dialog();
    return true;
  }
  if (is_enter_key(key, key_name)) {
    send_input_();
    return true;
  }
  if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
    if (!input_buffer_.empty()) {
      input_buffer_.pop_back();
      update_input_label_();
    }
    return true;
  }
  if (key >= 32 && key <= 126 && input_buffer_.size() < K_MAX_INPUT_CHARS) {
    input_buffer_.push_back(static_cast<char>(key));
    update_input_label_();
    return true;
  }
  return false;
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
  config.shortcut_text       = "ESC: Cancel  OK: Confirm";
  config.use_nav_action_keys = true;

  view::widgets::DialogCallbacks callbacks;
  callbacks.ok_action     = [this]() { apply_config_dialog_(); };
  callbacks.cancel_action = [this]() { hide_config_dialog_(); };
  dialog_                 = std::make_unique<view::widgets::Dialog>(dialog_parent_,
                                                                    *app_view_model_,
                                                                    *assets_,
                                                                    std::move(config),
                                                                    std::move(callbacks));
  dialog_->build();

  char baud_text[16]{};
  std::snprintf(baud_text, sizeof(baud_text), "%d", baud_rate_);
  view::widgets::DialogTextareaOptions options;
  options.accepted_chars = "0123456789";
  options.width          = 196;
  baud_input_            = dialog_->add_textarea(baud_text, options);
  if (baud_input_) {
    lv_obj_add_state(baud_input_, LV_STATE_FOCUSED);
  }
}

bool UartConnectivityView::dialog_visible() const { return dialog_ && dialog_->visible(); }

void UartConnectivityView::open_session_() {
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
  const auto text = std::exchange(input_buffer_, {});
  update_input_label_();
  append_log_(">>>", text);

  if (!session_) {
    append_log_("***", "UART is not open");
    return;
  }

  std::string error;
  if (!session_->write_text(text, error) && !error.empty()) {
    append_log_("***", error);
  }
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
  baud_input_ = nullptr;
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
}

void UartConnectivityView::apply_config_dialog_() {
  int new_baud = baud_rate_;
  if (baud_input_) {
    new_baud = std::atoi(lv_textarea_get_text(baud_input_));
  }
  hide_config_dialog_();
  if (new_baud <= 0) {
    append_log_("***", "Invalid baud rate");
    return;
  }

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

bool UartConnectivityView::append_dialog_char_(char ch) {
  if (baud_input_ && ch >= '0' && ch <= '9') {
    lv_textarea_add_char(baud_input_, static_cast<uint32_t>(ch));
  }
  return true;
}

void UartConnectivityView::poll_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<UartConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->poll_rx_();
  }
}

}  // namespace screen
