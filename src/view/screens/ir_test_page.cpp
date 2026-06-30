/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ir_test_page.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_VIEWPORT_WIDTH    = view::K_SCREEN_WIDTH;
constexpr int32_t K_VIEWPORT_HEIGHT   = 106;
constexpr int32_t K_MENU_WIDTH        = 300;
constexpr int32_t K_MENU_HEIGHT       = 106;
constexpr uint32_t K_RECEIVER_POLL_MS = 60;

struct MenuItem {
  const char* icon;
  const char* title;
  IrTestPage::SubPage page;
};

constexpr std::array<MenuItem, 2> K_MENU_ITEMS = {{
    {view::ICON_PAPER_PLANE, "IR Sender", IrTestPage::SubPage::SENDER},
    {view::ICON_ENVELOPE_OPEN, "IR Receiver", IrTestPage::SubPage::RECEIVER},
}};

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

IrTestPage::IrTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  update_nav_actions_();
  app_view_model_ref_().set_back_request_handler(back_request_handler, this);
  init();
  platform::set_key_listener(key_listener, this);
}

IrTestPage::~IrTestPage() {
  app_view_model_ref_().clear_back_request_handler(back_request_handler, this);
  platform::clear_key_listener(key_listener, this);
  stop_receiver_();
  menu_list_.reset();
}

void IrTestPage::build_content(lv_obj_t* content) {
  plane_ = lv_obj_create(content);
  lv_obj_remove_style_all(plane_);
  lv_obj_set_size(plane_, K_VIEWPORT_WIDTH * 3, K_VIEWPORT_HEIGHT);
  lv_obj_align(plane_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(plane_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(plane_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  auto* menu_viewport = build_subpage_container_(plane_, 0);
  std::vector<view::widgets::IconList::Item> list_items;
  list_items.reserve(K_MENU_ITEMS.size());
  for (const auto& item : K_MENU_ITEMS) {
    list_items.push_back({item.icon, item.title});
  }

  auto* text_font = assets_ref_().load_font("inter-medium.ttf", 14);
  auto* icon_font = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  menu_list_ =
      std::make_unique<view::widgets::IconList>(menu_viewport,
                                                app_view_model_ref_(),
                                                list_items,
                                                text_font ? text_font : &lv_font_montserrat_14,
                                                icon_font ? icon_font : &lv_font_montserrat_14,
                                                K_MENU_WIDTH,
                                                K_MENU_HEIGHT,
                                                menu_item_clicked,
                                                this);
  menu_list_->build();
  menu_list_->set_selected_index(selected_index_);

  auto* sender_viewport   = build_subpage_container_(plane_, K_VIEWPORT_WIDTH);
  auto* receiver_viewport = build_subpage_container_(plane_, K_VIEWPORT_WIDTH * 2);

  auto* title_font  = assets_ref_().load_font("inter-semibold.ttf", 16);
  auto* value_font  = assets_ref_().load_font("inter-semibold.ttf", 13);
  auto* detail_font = assets_ref_().load_font("inter-medium.ttf", 11);
  auto* title       = title_font ? title_font : &lv_font_montserrat_16;
  auto* value       = value_font ? value_font : &lv_font_montserrat_12;
  auto* detail      = detail_font ? detail_font : &lv_font_montserrat_12;

  auto* sender_group = lv_obj_create(sender_viewport);
  lv_obj_remove_style_all(sender_group);
  lv_obj_set_size(sender_group, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(sender_group, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(sender_group,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(sender_group, 7, 0);
  lv_obj_clear_flag(sender_group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(sender_group);
  sender_status_label_ = create_label(sender_group, app_view_model_ref_(), title, 290);
  sender_data_label_   = create_label(sender_group, app_view_model_ref_(), value, 290);
  sender_detail_label_ = create_label(sender_group, app_view_model_ref_(), detail, 290);
  set_label_text(sender_status_label_, "Enter/S Send Random Packet");
  set_label_text(sender_data_label_,
                 "Addr " + platform::ir::format_nec_address(address_) + "  Cmd --");
  const auto sender_info = platform::ir::read_sender_info();
  set_label_text(sender_detail_label_,
                 sender_info.available ? sender_info.lirc_path : sender_info.error_message);

  auto* receiver_group = lv_obj_create(receiver_viewport);
  lv_obj_remove_style_all(receiver_group);
  lv_obj_set_size(receiver_group, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(receiver_group, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(receiver_group,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(receiver_group, 7, 0);
  lv_obj_clear_flag(receiver_group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(receiver_group);
  receiver_status_label_ = create_label(receiver_group, app_view_model_ref_(), title, 290);
  receiver_data_label_   = create_label(receiver_group, app_view_model_ref_(), value, 290);
  receiver_detail_label_ = create_label(receiver_group, app_view_model_ref_(), detail, 290);
  set_label_text(receiver_status_label_, "Waiting for NEC data");
  set_label_text(receiver_data_label_, "Addr --  Cmd --");
  const auto receiver_info = platform::ir::read_receiver_info();
  set_label_text(receiver_detail_label_,
                 receiver_info.available ? receiver_info.lirc_path : receiver_info.error_message);
}

lv_obj_t* IrTestPage::build_subpage_container_(lv_obj_t* parent, int32_t x) {
  auto* viewport = lv_obj_create(parent);
  lv_obj_remove_style_all(viewport);
  lv_obj_set_size(viewport, K_VIEWPORT_WIDTH, K_VIEWPORT_HEIGHT);
  lv_obj_align(viewport, LV_ALIGN_TOP_LEFT, x, 0);
  lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(viewport,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);
  return viewport;
}

bool IrTestPage::back_request_handler(void* user_data) {
  auto* page = static_cast<IrTestPage*>(user_data);
  if (!page || page->app_view_model_ref_().current_page() != model::AppPage::IR_TEST) {
    return false;
  }
  if (page->active_page_ != SubPage::SENDER) {
    return false;
  }
  page->show_page_(SubPage::MENU);
  return true;
}

void IrTestPage::select_previous_() {
  if (selected_index_ > 0) {
    --selected_index_;
  }
  if (menu_list_) {
    menu_list_->set_selected_index(selected_index_);
  }
}

void IrTestPage::select_next_() {
  if (selected_index_ + 1 < K_MENU_ITEMS.size()) {
    ++selected_index_;
  }
  if (menu_list_) {
    menu_list_->set_selected_index(selected_index_);
  }
}

void IrTestPage::activate_selected_() {
  if (selected_index_ < K_MENU_ITEMS.size()) {
    show_page_(K_MENU_ITEMS[selected_index_].page);
  }
}

void IrTestPage::show_page_(SubPage page) {
  active_page_ = page;
  update_nav_actions_();
  if (menu_list_) {
    menu_list_->set_focused(page == SubPage::MENU);
  }
  if (plane_) {
    int32_t index = 0;
    if (page == SubPage::SENDER) {
      index = 1;
    } else if (page == SubPage::RECEIVER) {
      index = 2;
    }
    lv_obj_set_x(plane_, -index * K_VIEWPORT_WIDTH);
  }

  if (page == SubPage::RECEIVER) {
    start_receiver_();
  } else {
    stop_receiver_();
  }
}

void IrTestPage::update_nav_actions_() {
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() {
    app_view_model_ref_().request_back_or_quit();
  });
  set_nav_action_('8', view::ICON_CHECK_SQUARE, [this]() {
    show_test_result_dialog_();
  });
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
                 result.success ? protocol + " packet sent" : "IR send failed");
  std::string data = "Addr " + platform::ir::format_nec_address(result.address) + "  Cmd " +
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
                 snapshot.message.empty() ? "Waiting for NEC data" : snapshot.message);
  std::string data = "Addr ";
  data += !snapshot.data.empty() ? platform::ir::format_nec_address(snapshot.address) : "--";
  data += "  Cmd ";
  data += !snapshot.data.empty() ? platform::ir::format_nec_command(snapshot.command) : "--";
  set_label_text(receiver_data_label_, data);

  std::string detail;
  if (!snapshot.data.empty()) {
    if (!snapshot.protocol.empty()) {
      detail = snapshot.protocol + " | ";
    }
    detail += platform::ir::format_hex_bytes(snapshot.data);
    if (snapshot.address_filter_enabled) {
      detail += snapshot.address_matched ? " | Address OK" : " | Expected ";
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
    detail = snapshot.opened ? "Listening" : "Receiver not opened";
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
    case LV_KEY_UP:
    case LV_KEY_LEFT:
    case '5':
    case 'f':
    case 'F':
      if (page->active_page_ == SubPage::MENU) {
        page->select_previous_();
      }
      break;
    case LV_KEY_DOWN:
    case LV_KEY_RIGHT:
    case '7':
    case 'x':
    case 'X':
      if (page->active_page_ == SubPage::MENU) {
        page->select_next_();
      }
      break;
    case LV_KEY_ENTER:
      if (page->active_page_ == SubPage::MENU) {
        page->activate_selected_();
      } else if (page->active_page_ == SubPage::SENDER) {
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

void IrTestPage::menu_item_clicked(std::size_t index, void* user_data) {
  auto* page = static_cast<IrTestPage*>(user_data);
  if (!page || index >= K_MENU_ITEMS.size()) {
    return;
  }
  page->selected_index_ = index;
  if (page->menu_list_) {
    page->menu_list_->set_selected_index(index);
  }
  page->activate_selected_();
}

void IrTestPage::receiver_poll_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<IrTestPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->refresh_receiver_();
  }
}

}  // namespace screen
