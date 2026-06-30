/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>

#include "app_model.h"
#include "base_screen.h"
#include "connectivity_test_viewmodel.h"
#include "io_page_views.h"
#include "popup.h"

namespace screen {

class IoTestPage : public BaseScreen {
 public:
  IoTestPage(viewmodel::AppViewModel& app_view_model,
             viewmodel::ConnectivityTestViewModel& connectivity_view_model,
             app::AssetManager& assets,
             model::AppPage page);
  ~IoTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void key_release_listener(uint32_t key, const char* key_name, void* user_data);
  static void long_key_listener(uint32_t key, const char* key_name, void* user_data);
  static void loading_modal_timer_cb(lv_timer_t* timer);

  void update_nav_actions_();
  void scroll_viewport_(int32_t direction);
  void show_confirm_hold_popup_();
  void hide_confirm_hold_popup_();
  void show_loading_modal_();
  void hide_loading_modal_();
  bool is_uart_page_() const;
  bool is_ext_io_page_() const;
  bool is_link_page_() const;

  viewmodel::ConnectivityTestViewModel& connectivity_view_model_;
  model::AppPage page_;
  lv_obj_t* viewport_{nullptr};
  std::unique_ptr<WifiConnectivityView> wifi_view_{};
  std::unique_ptr<BluetoothConnectivityView> bluetooth_view_{};
  std::unique_ptr<EthernetConnectivityView> ethernet_view_{};
  std::unique_ptr<UsbConnectivityView> usb_view_{};
  std::unique_ptr<HdmiConnectivityView> hdmi_view_{};
  std::unique_ptr<I2cConnectivityView> i2c_view_{};
  std::unique_ptr<SpiConnectivityView> spi_view_{};
  std::unique_ptr<UartConnectivityView> uart_view_{};
  std::unique_ptr<ExtIoConnectivityView> ext_io_view_{};
  std::unique_ptr<LinkConnectivityView> link_view_{};
  lv_obj_t* loading_modal_{nullptr};
  lv_timer_t* loading_modal_timer_{nullptr};
  std::unique_ptr<view::widgets::Popup> confirm_hold_popup_{};
};

}  // namespace screen
