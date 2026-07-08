/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ir_test_page.h"

#include <cstdint>
#include <string>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_VIEWPORT_WIDTH    = view::K_SCREEN_WIDTH;
constexpr int32_t K_VIEWPORT_HEIGHT   = 106;
constexpr uint32_t K_RECEIVER_POLL_MS = 60;

lv_obj_t* create_label(lv_obj_t* parent,
                       viewmodel::AppViewModel& app_view_model,
                       const lv_font_t* font,
                       int32_t width,
                       lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
  auto* label = lv_label_create(parent);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  reactive::bind_theme(label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);
  return label;
}

void set_label_text(lv_obj_t* label, const std::string& text) {
  if (label) {
    lv_label_set_text(label, text.c_str());
  }
}

void set_label_text(lv_obj_t* label, const char* text) {
  if (label) {
    lv_label_set_text(label, text ? text : "");
  }
}

}  // namespace

IrTestPage::IrTestPage(viewmodel::AppViewModel& app_view_model,
                       app::AssetManager& assets,
                       SubPage active_page)
    : BaseScreen(app_view_model, assets),
      active_page_(active_page) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  update_nav_actions_();
  init();
  platform::set_key_listener(key_listener, this);
}

IrTestPage::~IrTestPage() {
  platform::clear_key_listener(key_listener, this);
  stop_receiver_();
}

void IrTestPage::build_content(lv_obj_t* content) {
  plane_ = lv_obj_create(content);
  lv_obj_remove_style_all(plane_);
  lv_obj_set_size(plane_, K_VIEWPORT_WIDTH, K_VIEWPORT_HEIGHT);
  lv_obj_align(plane_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(plane_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(plane_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  auto* viewport = build_page_container_(plane_);

  auto* title_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 16);
  auto* value_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 13);
  auto* detail_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 11);
  auto* title  = title_font ? title_font : &lv_font_montserrat_16;
  auto* value  = value_font ? value_font : &lv_font_montserrat_12;
  auto* detail = detail_font ? detail_font : &lv_font_montserrat_12;

  auto* group = lv_obj_create(viewport);
  lv_obj_remove_style_all(group);
  lv_obj_set_size(group, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(group, 7, 0);
  lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(group);

  if (active_page_ == SubPage::SENDER) {
    sender_status_label_ = create_label(group, app_view_model_ref_(), title, 290);
    sender_data_label_   = create_label(group, app_view_model_ref_(), value, 290);
    sender_detail_label_ = create_label(group, app_view_model_ref_(), detail, 290);
    set_label_text(sender_status_label_, app_view_model_ref_().tr("Enter/S Send Random Packet"));
    set_label_text(sender_data_label_,
                   app_view_model_ref_().tr("Addr") + " " +
                       platform::ir::format_nec_address(address_) + "  " +
                       app_view_model_ref_().tr("Cmd") + " --");
    const auto sender_info = platform::ir::read_sender_info();
    set_label_text(sender_detail_label_,
                   sender_info.available ? sender_info.lirc_path : sender_info.error_message);
    return;
  }

  receiver_status_label_ = create_label(group, app_view_model_ref_(), title, 290);
  receiver_data_label_   = create_label(group, app_view_model_ref_(), value, 290);
  receiver_detail_label_ = create_label(group, app_view_model_ref_(), detail, 290);
  set_label_text(receiver_status_label_, app_view_model_ref_().tr("Waiting for NEC data"));
  set_label_text(
      receiver_data_label_,
      app_view_model_ref_().tr("Addr") + " --  " + app_view_model_ref_().tr("Cmd") + " --");
  const auto receiver_info = platform::ir::read_receiver_info();
  set_label_text(receiver_detail_label_,
                 receiver_info.available ? receiver_info.lirc_path : receiver_info.error_message);
  start_receiver_();
}

lv_obj_t* IrTestPage::build_page_container_(lv_obj_t* parent) {
  auto* viewport = lv_obj_create(parent);
  lv_obj_remove_style_all(viewport);
  lv_obj_set_size(viewport, K_VIEWPORT_WIDTH, K_VIEWPORT_HEIGHT);
  lv_obj_align(viewport, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(viewport,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);
  return viewport;
}

void IrTestPage::update_nav_actions_() {
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() {
    app_view_model_ref_().request_back_or_quit();
  });
  set_nav_action_('8', view::ICON_CHECK_SQUARE, [this]() { show_test_result_dialog_(); });
}

void IrTestPage::send_packet_() {
  const auto result = platform::ir::send_nec_packet(address_);
  update_sender_status_(result);
}

void IrTestPage::start_receiver_() {
  receiver_session_.start(0, false);
  update_receiver_status_(receiver_session_.snapshot());
  if (!receiver_poll_timer_) {
    receiver_poll_timer_ = lv_timer_create(receiver_poll_timer_cb, K_RECEIVER_POLL_MS, this);
    lv_timer_set_auto_delete(receiver_poll_timer_, false);
  }
  lv_timer_resume(receiver_poll_timer_);
  lv_timer_reset(receiver_poll_timer_);
}

void IrTestPage::stop_receiver_() {
  if (receiver_poll_timer_) {
    lv_timer_delete(receiver_poll_timer_);
    receiver_poll_timer_ = nullptr;
  }
  receiver_session_.stop();
}

void IrTestPage::refresh_receiver_() { update_receiver_status_(receiver_session_.poll()); }

void IrTestPage::update_sender_status_(const platform::ir::IrSendResult& result) {
  const std::string protocol = result.protocol.empty() ? "NEC" : result.protocol;
  set_label_text(sender_status_label_,
                 result.success ? protocol + " " + app_view_model_ref_().tr("packet sent")
                                : app_view_model_ref_().tr("IR send failed"));
  std::string data = app_view_model_ref_().tr("Addr") + " " +
                     platform::ir::format_nec_address(result.address) + "  " +
                     app_view_model_ref_().tr("Cmd") + " " +
                     (result.success ? platform::ir::format_nec_command(result.command) : "--");
  set_label_text(sender_data_label_, data);
  std::string detail = result.message;
  if (!result.data.empty()) {
    if (!detail.empty()) {
      detail += " | ";
    }
    detail += platform::ir::format_hex_bytes(result.data);
  }
  if (!result.device_path.empty()) {
    detail += " (" + result.device_path + ")";
  }
  set_label_text(sender_detail_label_, detail);
}

void IrTestPage::update_receiver_status_(const platform::ir::IrReceiveSnapshot& snapshot) {
  set_label_text(receiver_status_label_,
                 snapshot.message.empty() ? app_view_model_ref_().tr("Waiting for NEC data")
                                          : app_view_model_ref_().tr(snapshot.message.c_str()));
  std::string data = app_view_model_ref_().tr("Addr") + " ";
  data += !snapshot.data.empty() ? platform::ir::format_nec_address(snapshot.address) : "--";
  data += "  " + app_view_model_ref_().tr("Cmd") + " ";
  data += !snapshot.data.empty() ? platform::ir::format_nec_command(snapshot.command) : "--";
  set_label_text(receiver_data_label_, data);

  std::string detail;
  if (!snapshot.data.empty()) {
    if (!snapshot.protocol.empty()) {
      detail = snapshot.protocol + " | ";
    }
    detail += platform::ir::format_hex_bytes(snapshot.data);
    if (snapshot.address_filter_enabled) {
      detail += snapshot.address_matched ? " | " + app_view_model_ref_().tr("Address OK")
                                         : " | " + app_view_model_ref_().tr("Expected") + " ";
    }
    if (snapshot.address_filter_enabled && !snapshot.address_matched) {
      detail += platform::ir::format_nec_address(snapshot.expected_address);
    }
  } else if (!snapshot.device_path.empty()) {
    detail = snapshot.device_path;
  } else if (!snapshot.raw_summary.empty()) {
    if (!detail.empty()) {
      detail += " | ";
    }
    detail += snapshot.raw_summary;
  }
  if (detail.empty()) {
    detail = snapshot.opened ? app_view_model_ref_().tr("Listening")
                             : app_view_model_ref_().tr("Receiver not opened");
  }
  set_label_text(receiver_detail_label_, detail);
}

void IrTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<IrTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  switch (key) {
    case LV_KEY_ENTER:
      if (page->active_page_ == SubPage::SENDER) {
        page->send_packet_();
      }
      break;
    case 's':
    case 'S':
      if (page->active_page_ == SubPage::SENDER) {
        page->send_packet_();
      }
      break;
    default:
      break;
  }
}

void IrTestPage::receiver_poll_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<IrTestPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->refresh_receiver_();
  }
}

}  // namespace screen
