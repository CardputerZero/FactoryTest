/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_link_page.h"

#include "connectivity_subpage_common.h"

#include <cstdlib>
#include <utility>

#include "bindings.h"
#include "linux_input.h"
#include "theme.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace screen {

lv_obj_t* build_link_panel(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model) {
  if (!parent) {
    return nullptr;
  }

  auto* panel = lv_obj_create(parent);
  lv_obj_remove_style_all(panel);
  lv_obj_set_width(panel, K_LINK_PANEL_WIDTH);
  lv_obj_set_height(panel, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(panel, 2, 0);
  lv_obj_set_style_pad_bottom(panel, 6, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 0);
  reactive::bind_theme(panel, app_view_model.dark_mode_subject(), reactive::ThemeRole::SURFACE);
  return panel;
}

const char* link_status_text(model::LinkTestStatus status) {
  switch (status) {
    case model::LinkTestStatus::RUNNING:
      return "Testing";
    case model::LinkTestStatus::SUCCESS:
      return "Success";
    case model::LinkTestStatus::FAILED:
      return "Failed";
    case model::LinkTestStatus::IDLE:
    default:
      return "Idle";
  }
}

std::string ping_value_text(const model::LinkTestMetric& metric) {
  if (metric.status == model::LinkTestStatus::SUCCESS) {
    return "Internet OK";
  }
  if (metric.status == model::LinkTestStatus::RUNNING) {
    return "Testing...";
  }
  return "Failed";
}

std::string iperf_value_text(const model::LinkTestMetric& metric) {
  if (metric.status == model::LinkTestStatus::SUCCESS) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << metric.value << " Mbps";
    return stream.str();
  }
  if (metric.status == model::LinkTestStatus::RUNNING) {
    return "Testing...";
  }
  return "Failed";
}

void add_link_divider(lv_obj_t* card, viewmodel::AppViewModel& app_view_model) {
  auto* divider = lv_obj_create(card);
  lv_obj_remove_style_all(divider);
  lv_obj_set_size(divider, K_LINK_ROW_WIDTH, 1);
  lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
  reactive::bind_theme(divider, app_view_model.dark_mode_subject(), reactive::ThemeRole::BAR);
}

void add_link_row(lv_obj_t* card,
                  viewmodel::AppViewModel& app_view_model,
                  const CardFonts& fonts,
                  const char* title,
                  const std::string& value,
                  const model::LinkTestMetric& metric) {
  auto* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, K_LINK_ROW_WIDTH, K_LINK_ROW_HEIGHT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row,
                        LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

  auto* title_label = lv_label_create(row);
  lv_label_set_text(title_label, title ? title : "Link");
  lv_obj_set_width(title_label, 84);
  lv_obj_set_style_text_font(title_label, fonts.title, 0);
  reactive::bind_theme(title_label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);

  std::string text = value;
  if (!metric.detail.empty()) {
    text += " | ";
    text += metric.detail;
  } else {
    text += " | ";
    text += link_status_text(metric.status);
  }

  auto* value_label = lv_label_create(row);
  lv_label_set_text(value_label, text.c_str());
  lv_label_set_long_mode(value_label, LV_LABEL_LONG_SCROLL);
  lv_obj_set_width(value_label, K_LINK_ROW_WIDTH - 92);
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(value_label, fonts.value, 0);
  const auto colors = view::palette(app_view_model.is_dark_mode());
  if (metric.status == model::LinkTestStatus::SUCCESS) {
    lv_obj_set_style_text_color(value_label, colors.success, 0);
  } else if (metric.status == model::LinkTestStatus::FAILED) {
    lv_obj_set_style_text_color(value_label, colors.error, 0);
  } else {
    reactive::bind_theme(value_label,
                         app_view_model.dark_mode_subject(),
                         reactive::ThemeRole::TEXT);
  }
}

void rebuild_link_panel(lv_obj_t* panel,
                        viewmodel::AppViewModel& app_view_model,
                        app::AssetManager& assets,
                        const model::LinkTestSnapshot& snapshot) {
  if (!panel) {
    return;
  }

  lv_obj_clean(panel);
  const auto colors = view::palette(app_view_model.is_dark_mode());
  const auto fonts  = load_card_fonts(assets);
  auto* title_font  = assets.load_font("inter-semibold.ttf", 12);
  auto* hint_font   = assets.load_font("inter-medium.ttf", 10);

  std::ostringstream title;
  title << "iperf " << snapshot.settings.iperf_host << ':' << snapshot.settings.iperf_port;
  auto* title_label = lv_label_create(panel);
  lv_label_set_text(title_label, title.str().c_str());
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL);
  lv_obj_set_width(title_label, K_LINK_CARD_WIDTH);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_label, title_font ? title_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(title_label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);

  auto* card = lv_obj_create(panel);
  lv_obj_remove_style_all(card);
  lv_obj_set_width(card, K_LINK_CARD_WIDTH);
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, colors.button, 0);
  lv_obj_set_style_border_color(card, colors.border, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  add_link_row(card, app_view_model, fonts, "Ping", ping_value_text(snapshot.ping), snapshot.ping);
  add_link_divider(card, app_view_model);
  add_link_row(card,
               app_view_model,
               fonts,
               "Wi-Fi iperf",
               iperf_value_text(snapshot.wifi_iperf),
               snapshot.wifi_iperf);
  add_link_divider(card, app_view_model);
  add_link_row(card,
               app_view_model,
               fonts,
               "Eth iperf",
               iperf_value_text(snapshot.ethernet_iperf),
               snapshot.ethernet_iperf);

  auto* hint_label = lv_label_create(panel);
  lv_label_set_text(hint_label, "5/R Restart  |  7 iperf Settings");
  lv_obj_set_width(hint_label, K_LINK_CARD_WIDTH);
  lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(hint_label, hint_font ? hint_font : &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hint_label, colors.text_muted, 0);
}

lv_obj_t* add_dialog_label(lv_obj_t* parent,
                           viewmodel::AppViewModel& app_view_model,
                           app::AssetManager& assets,
                           const char* text) {
  auto* label = lv_label_create(parent);
  lv_label_set_text(label, text ? text : "");
  auto* font = assets.load_font("inter-medium.ttf", 10);
  lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);
  return label;
}

lv_obj_t* add_dialog_textarea(lv_obj_t* parent,
                              viewmodel::AppViewModel& app_view_model,
                              app::AssetManager& assets,
                              const char* text,
                              const char* accepted_chars) {
  auto* textarea = lv_textarea_create(parent);
  lv_textarea_set_one_line(textarea, true);
  lv_textarea_set_text(textarea, text ? text : "");
  if (accepted_chars) {
    lv_textarea_set_accepted_chars(textarea, accepted_chars);
  }
  lv_obj_set_size(textarea, 246, 24);
  auto* font = assets.load_font("inter-medium.ttf", 11);
  lv_obj_set_style_text_font(textarea, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(textarea, app_view_model.dark_mode_subject(), reactive::ThemeRole::BUTTON);
  return textarea;
}

lv_obj_t* add_dialog_button(lv_obj_t* parent,
                            viewmodel::AppViewModel& app_view_model,
                            app::AssetManager& assets,
                            const char* text,
                            lv_event_cb_t callback,
                            void* user_data) {
  auto* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_set_size(button, 78, 24);
  lv_obj_set_style_radius(button, 8, 0);
  reactive::bind_theme(button, app_view_model.dark_mode_subject(), reactive::ThemeRole::BUTTON);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

  auto* label = lv_label_create(button);
  lv_label_set_text(label, text ? text : "OK");
  auto* font = assets.load_font("inter-semibold.ttf", 11);
  lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);
  lv_obj_center(label);
  return button;
}

LinkConnectivityView::LinkConnectivityView(viewmodel::LinkConnectivityViewModel& view_model)
    : view_model_(view_model) {}

LinkConnectivityView::~LinkConnectivityView() {
  delete_timer(refresh_timer_);
  hide_config_dialog_();
}

void LinkConnectivityView::build(lv_obj_t* parent,
                                 lv_obj_t* dialog_parent,
                                 viewmodel::AppViewModel& app_view_model,
                                 app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  dialog_parent_  = dialog_parent;
  panel_          = build_link_panel(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void LinkConnectivityView::restart() {
  view_model_.refresh(true);
  rebuild_();
}

void LinkConnectivityView::show_config_dialog() {
  if (!dialog_parent_ || !app_view_model_ || !assets_) {
    return;
  }
  if (dialog_ && lv_obj_is_valid(dialog_)) {
    return;
  }

  hide_config_dialog_();
  platform::set_nav_trigger_mode(platform::NavTriggerMode::LONG_PRESS);
  const auto colors = view::palette(app_view_model_->is_dark_mode());
  dialog_           = lv_obj_create(dialog_parent_);
  lv_obj_remove_style_all(dialog_);
  lv_obj_set_size(dialog_, 286, 150);
  lv_obj_align(dialog_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_flow(dialog_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(dialog_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(dialog_, 8, 0);
  lv_obj_set_style_pad_row(dialog_, 2, 0);
  lv_obj_set_style_radius(dialog_, 12, 0);
  lv_obj_set_style_bg_color(dialog_, colors.button, 0);
  lv_obj_set_style_bg_opa(dialog_, LV_OPA_90, 0);
  lv_obj_set_style_border_color(dialog_, colors.border, 0);
  lv_obj_set_style_border_width(dialog_, 1, 0);
  lv_obj_set_style_shadow_width(dialog_, 12, 0);
  lv_obj_set_style_shadow_opa(dialog_, LV_OPA_20, 0);
  lv_obj_add_flag(dialog_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(dialog_, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(dialog_, LV_SCROLLBAR_MODE_AUTO);

  auto* title = lv_label_create(dialog_);
  lv_label_set_text(title, "iperf Settings");
  auto* title_font = assets_->load_font("inter-semibold.ttf", 12);
  lv_obj_set_style_text_font(title, title_font ? title_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(title, app_view_model_->dark_mode_subject(), reactive::ThemeRole::TEXT);

  add_dialog_label(dialog_, *app_view_model_, *assets_, "Server IP");
  host_input_ = add_dialog_textarea(dialog_,
                                    *app_view_model_,
                                    *assets_,
                                    view_model_.settings().iperf_host.c_str(),
                                    "0123456789.");
  lv_obj_add_event_cb(host_input_, dialog_host_focus_cb, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(host_input_, dialog_host_focus_cb, LV_EVENT_FOCUSED, this);

  add_dialog_label(dialog_, *app_view_model_, *assets_, "Port");
  char port_text[8]{};
  lv_snprintf(port_text, sizeof(port_text), "%d", view_model_.settings().iperf_port);
  port_input_ = add_dialog_textarea(dialog_, *app_view_model_, *assets_, port_text, "0123456789");
  lv_obj_add_event_cb(port_input_, dialog_port_focus_cb, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(port_input_, dialog_port_focus_cb, LV_EVENT_FOCUSED, this);

  auto* button_row = lv_obj_create(dialog_);
  lv_obj_remove_style_all(button_row);
  lv_obj_set_size(button_row, 190, 26);
  lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_row,
                        LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(button_row, LV_OBJ_FLAG_SCROLLABLE);
  add_dialog_button(button_row, *app_view_model_, *assets_, "Cancel", dialog_cancel_cb, this);
  add_dialog_button(button_row, *app_view_model_, *assets_, "OK", dialog_apply_cb, this);

  lv_obj_move_foreground(dialog_);
  set_active_dialog_field_(DialogField::HOST);
}

bool LinkConnectivityView::handle_key(uint32_t key) {
  if (!dialog_visible()) {
    return false;
  }

  if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
    if (auto* input = active_dialog_input_()) {
      lv_textarea_delete_char(input);
    }
    return true;
  }

  if (key == LV_KEY_ESC) {
    hide_config_dialog_();
    return true;
  }

  if (key == LV_KEY_ENTER) {
    if (active_dialog_field_ == DialogField::HOST) {
      set_active_dialog_field_(DialogField::PORT);
    } else {
      apply_config_dialog_();
    }
    return true;
  }

  if (key == '\t' || key == LV_KEY_NEXT || key == LV_KEY_LEFT || key == LV_KEY_RIGHT ||
      key == LV_KEY_UP || key == LV_KEY_DOWN) {
    set_active_dialog_field_(active_dialog_field_ == DialogField::HOST ? DialogField::PORT
                                                                       : DialogField::HOST);
    return true;
  }

  if (key >= 32 && key <= 126) {
    return append_dialog_char_(static_cast<char>(key));
  }

  return true;
}

bool LinkConnectivityView::dialog_visible() const {
  return dialog_ && lv_obj_is_valid(dialog_);
}

void LinkConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if (changed || !panel_initialized_) {
    rebuild_();
  }
}

void LinkConnectivityView::rebuild_() {
  if (panel_ && app_view_model_ && assets_) {
    rebuild_link_panel(panel_, *app_view_model_, *assets_, view_model_.snapshot());
    panel_initialized_ = true;
  }
}

void LinkConnectivityView::hide_config_dialog_() {
  if (dialog_ && lv_obj_is_valid(dialog_)) {
    lv_obj_delete(dialog_);
  }
  dialog_     = nullptr;
  host_input_ = nullptr;
  port_input_ = nullptr;
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
}

void LinkConnectivityView::apply_config_dialog_() {
  model::LinkTestSettings settings = view_model_.settings();
  if (host_input_) {
    settings.iperf_host = lv_textarea_get_text(host_input_);
  }
  if (port_input_) {
    settings.iperf_port = std::atoi(lv_textarea_get_text(port_input_));
  }

  view_model_.set_settings(std::move(settings));
  hide_config_dialog_();
  restart();
}

void LinkConnectivityView::set_active_dialog_field_(DialogField field) {
  active_dialog_field_ = field;
  if (host_input_) {
    lv_obj_clear_state(host_input_, LV_STATE_FOCUSED);
  }
  if (port_input_) {
    lv_obj_clear_state(port_input_, LV_STATE_FOCUSED);
  }

  if (auto* input = active_dialog_input_()) {
    lv_obj_add_state(input, LV_STATE_FOCUSED);
  }
}

lv_obj_t* LinkConnectivityView::active_dialog_input_() const {
  return active_dialog_field_ == DialogField::HOST ? host_input_ : port_input_;
}

bool LinkConnectivityView::append_dialog_char_(char ch) {
  const bool host_field = active_dialog_field_ == DialogField::HOST;
  const bool allowed = (ch >= '0' && ch <= '9') || (host_field && ch == '.');
  if (!allowed) {
    return true;
  }

  if (auto* input = active_dialog_input_()) {
    lv_textarea_add_char(input, static_cast<uint32_t>(ch));
  }
  return true;
}

void LinkConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<LinkConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

void LinkConnectivityView::dialog_apply_cb(lv_event_t* event) {
  auto* view = static_cast<LinkConnectivityView*>(lv_event_get_user_data(event));
  if (view) {
    view->apply_config_dialog_();
  }
}

void LinkConnectivityView::dialog_cancel_cb(lv_event_t* event) {
  auto* view = static_cast<LinkConnectivityView*>(lv_event_get_user_data(event));
  if (view) {
    view->hide_config_dialog_();
  }
}

void LinkConnectivityView::dialog_host_focus_cb(lv_event_t* event) {
  auto* view = static_cast<LinkConnectivityView*>(lv_event_get_user_data(event));
  if (view) {
    view->set_active_dialog_field_(DialogField::HOST);
  }
}

void LinkConnectivityView::dialog_port_focus_cb(lv_event_t* event) {
  auto* view = static_cast<LinkConnectivityView*>(lv_event_get_user_data(event));
  if (view) {
    view->set_active_dialog_field_(DialogField::PORT);
  }
}

}  // namespace screen
