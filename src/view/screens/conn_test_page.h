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
#include "connectivity_test_viewmodel.h"
#include "icon_list.h"
#include "io_page_views.h"

namespace screen {

class ConnTestPage : public BaseScreen {
 public:
  ConnTestPage(viewmodel::AppViewModel& app_view_model,
                       viewmodel::ConnectivityTestViewModel& connectivity_view_model,
                       app::AssetManager& assets);
  ~ConnTestPage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  static bool back_request_handler(void* user_data);
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void key_release_listener(uint32_t key, const char* key_name, void* user_data);
  static void long_key_listener(uint32_t key, const char* key_name, void* user_data);
  static void menu_item_clicked(std::size_t index, void* user_data);
  static void selected_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void active_page_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void link_restart_request_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void link_settings_request_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void uart_settings_request_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void loading_modal_timer_cb(lv_timer_t* timer);
  std::size_t viewport_index_(model::SubPage page) const;
  void show_page_(model::SubPage page, bool animate = true);
  void update_selection_(std::size_t index);
  void scroll_active_page_(int32_t direction);
  lv_obj_t* viewport_for_page_(model::SubPage page) const;
  void reset_subpage_views_(model::SubPage keep_page);
  void ensure_subpage_view_(model::SubPage page);
  void build_subpage_view_(lv_obj_t* viewport, model::SubPage page);
  void switch_external_bus_(model::SubPage page);
  bool should_suppress_uart_settings_();
  void update_title_(model::SubPage page);
  void update_nav_actions_();
  void show_loading_modal_(model::SubPage page);
  void hide_loading_modal_();

  viewmodel::ConnectivityTestViewModel& connectivity_view_model_;
  lv_obj_t* plane_{nullptr};
  std::unique_ptr<view::widgets::IconList> menu_list_{};
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
  uint32_t uart_page_entered_at_{0};
  bool suppress_uart_settings_once_{false};
  lv_observer_t* selected_observer_handle_{nullptr};
  lv_observer_t* active_page_observer_handle_{nullptr};
  lv_observer_t* link_restart_observer_handle_{nullptr};
  lv_observer_t* link_settings_observer_handle_{nullptr};
  lv_observer_t* uart_settings_observer_handle_{nullptr};
};

}  // namespace screen
