/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include <cstdint>
#include <memory>

#include "base_screen.h"
#include "icon_list.h"
#include "ir_service.h"

namespace screen {

class IrTestPage : public BaseScreen {
 public:
  enum class SubPage {
    MENU,
    SENDER,
    RECEIVER,
  };

  IrTestPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~IrTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static bool back_request_handler(void* user_data);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void menu_item_clicked(std::size_t index, void* user_data);
  static void receiver_poll_timer_cb(lv_timer_t* timer);

  void select_previous_();
  void select_next_();
  void activate_selected_();
  void show_page_(SubPage page);
  void update_nav_actions_();
  void send_packet_();
  void start_receiver_();
  void stop_receiver_();
  void refresh_receiver_();
  void update_sender_status_(const platform::ir::IrSendResult& result);
  void update_receiver_status_(const platform::ir::IrReceiveSnapshot& snapshot);
  lv_obj_t* build_subpage_container_(lv_obj_t* parent, int32_t x);

  SubPage active_page_{SubPage::MENU};
  std::size_t selected_index_{0};
  uint16_t address_{0x2AD5};
  lv_obj_t* plane_{nullptr};
  std::unique_ptr<view::widgets::IconList> menu_list_{};
  lv_obj_t* sender_status_label_{nullptr};
  lv_obj_t* sender_data_label_{nullptr};
  lv_obj_t* sender_detail_label_{nullptr};
  lv_obj_t* receiver_status_label_{nullptr};
  lv_obj_t* receiver_data_label_{nullptr};
  lv_obj_t* receiver_detail_label_{nullptr};
  lv_timer_t* receiver_poll_timer_{nullptr};
  platform::ir::IrReceiverSession receiver_session_{};
};

}  // namespace screen
