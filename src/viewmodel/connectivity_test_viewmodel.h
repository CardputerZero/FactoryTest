/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "connectivity_test_model.h"
#include "lvgl.h"
#include "subjects.h"

namespace viewmodel {

class WifiConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ScanItem>& scan_items() const;
  const std::string& error_message() const;

 private:
  model::WifiConnectivityModel model_{};
};

class BluetoothConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ScanItem>& scan_items() const;
  const std::string& error_message() const;

 private:
  model::BluetoothConnectivityModel model_{};
};

class EthernetConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::InfoField>& fields() const;
  const std::string& error_message() const;

 private:
  model::EthernetConnectivityModel model_{};
};

class UsbConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ScanItem>& devices() const;
  const std::string& error_message() const;

 private:
  model::UsbConnectivityModel model_{};
};

class I2cConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  bool is_scanning() const;
  const std::vector<model::I2cAddress>& addresses() const;
  const std::string& error_message() const;

 private:
  model::I2cConnectivityModel model_{};
};

class SpiConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ScanItem>& devices() const;
  const std::string& error_message() const;

 private:
  model::SpiConnectivityModel model_{};
};

class HdmiConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::InfoField>& fields() const;
  const std::string& error_message() const;

 private:
  model::HdmiConnectivityModel model_{};
};

class LinkConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const model::LinkTestSnapshot& snapshot() const;
  const model::LinkTestSettings& settings() const;
  void set_settings(model::LinkTestSettings settings);

 private:
  model::LinkConnectivityModel model_{};
};

class ConnectivityTestViewModel {
 public:
  ConnectivityTestViewModel();
  ~ConnectivityTestViewModel();

  ConnectivityTestViewModel(const ConnectivityTestViewModel&)            = delete;
  ConnectivityTestViewModel& operator=(const ConnectivityTestViewModel&) = delete;

  lv_subject_t* selected_index_subject();
  lv_subject_t* active_page_subject();
  lv_subject_t* link_restart_request_subject();
  lv_subject_t* link_settings_request_subject();
  lv_subject_t* uart_settings_request_subject();
  const std::array<model::MenuItem, model::ConnectivityTestModel::K_ITEM_COUNT>& items() const;
  std::size_t selected_index() const;
  model::SubPage active_page() const;
  bool is_menu_active() const;
  bool is_direct_subpage_active() const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void activate_selected();
  void show_subpage(model::SubPage page, bool direct = false);
  void show_menu();
  void clear_direct_subpage();
  bool request_back();
  bool refresh_active();
  void request_link_restart();
  void request_link_settings();
  void request_uart_settings();

  WifiConnectivityViewModel& wifi_view_model();
  BluetoothConnectivityViewModel& bluetooth_view_model();
  EthernetConnectivityViewModel& ethernet_view_model();
  UsbConnectivityViewModel& usb_view_model();
  I2cConnectivityViewModel& i2c_view_model();
  SpiConnectivityViewModel& spi_view_model();
  HdmiConnectivityViewModel& hdmi_view_model();
  LinkConnectivityViewModel& link_view_model();

 protected:
  void publish_all_();

 private:
  model::ConnectivityTestModel model_{};
  reactive::IntSubject selected_index_subject_;
  reactive::IntSubject active_page_subject_;
  reactive::IntSubject link_restart_request_subject_{0};
  reactive::IntSubject link_settings_request_subject_{0};
  reactive::IntSubject uart_settings_request_subject_{0};
  int32_t link_restart_request_count_{0};
  int32_t link_settings_request_count_{0};
  int32_t uart_settings_request_count_{0};
  bool direct_subpage_active_{false};
  WifiConnectivityViewModel wifi_view_model_{};
  BluetoothConnectivityViewModel bluetooth_view_model_{};
  EthernetConnectivityViewModel ethernet_view_model_{};
  UsbConnectivityViewModel usb_view_model_{};
  I2cConnectivityViewModel i2c_view_model_{};
  SpiConnectivityViewModel spi_view_model_{};
  HdmiConnectivityViewModel hdmi_view_model_{};
  LinkConnectivityViewModel link_view_model_{};
};

}  // namespace viewmodel
