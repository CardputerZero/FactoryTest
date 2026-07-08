/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "base_screen.h"
#include "ir_service.h"

namespace screen {

class IrTestPage : public BaseScreen {
 public:
  enum class SubPage {
    SENDER,
    RECEIVER,
  };

  IrTestPage(viewmodel::AppViewModel& app_view_model,
             app::AssetManager& assets,
             SubPage active_page);
  ~IrTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void receiver_poll_timer_cb(lv_timer_t* timer);

  void update_nav_actions_();
  void send_packet_();
  void start_receiver_();
  void stop_receiver_();
  void refresh_receiver_();
  void update_sender_status_(const platform::ir::IrSendResult& result);
  void update_receiver_status_(const platform::ir::IrReceiveSnapshot& snapshot);
  lv_obj_t* build_page_container_(lv_obj_t* parent);

  SubPage active_page_{SubPage::SENDER};
  uint16_t address_{0x2AD5};
  lv_obj_t* plane_{nullptr};
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
