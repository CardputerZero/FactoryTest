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
  const std::vector<model::ConnectivityScanInfo>& scan_items() const;
  const std::string& error_message() const;

 private:
  model::WifiConnectivityModel model_{};
};

class BluetoothConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ConnectivityScanInfo>& scan_items() const;
  const std::string& error_message() const;

 private:
  model::BluetoothConnectivityModel model_{};
};

class EthernetConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ConnectivityInfoField>& fields() const;
  const std::string& error_message() const;

 private:
  model::EthernetConnectivityModel model_{};
};

class UsbConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ConnectivityScanInfo>& devices() const;
  const std::string& error_message() const;

 private:
  model::UsbConnectivityModel model_{};
};

class I2cConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ConnectivityI2cAddressInfo>& addresses() const;
  const std::string& error_message() const;

 private:
  model::I2cConnectivityModel model_{};
};

class SpiConnectivityViewModel {
 public:
  const char* title_text() const;
  bool refresh(bool force_refresh = false);
  const std::vector<model::ConnectivityScanInfo>& devices() const;
  const std::string& error_message() const;

 private:
  model::SpiConnectivityModel model_{};
};

class ConnectivityTestViewModel {
 public:
  ConnectivityTestViewModel();
  ~ConnectivityTestViewModel();

  ConnectivityTestViewModel(const ConnectivityTestViewModel&)            = delete;
  ConnectivityTestViewModel& operator=(const ConnectivityTestViewModel&) = delete;

  lv_subject_t* selected_index_subject();
  lv_subject_t* active_page_subject();
  const std::array<model::ConnectivityMenuItem, model::ConnectivityTestModel::K_ITEM_COUNT>& items()
      const;
  std::size_t selected_index() const;
  model::ConnectivitySubPage active_page() const;
  bool is_menu_active() const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void activate_selected();
  void show_menu();
  bool request_back();
  bool refresh_active();

  WifiConnectivityViewModel& wifi_view_model();
  BluetoothConnectivityViewModel& bluetooth_view_model();
  EthernetConnectivityViewModel& ethernet_view_model();
  UsbConnectivityViewModel& usb_view_model();
  I2cConnectivityViewModel& i2c_view_model();
  SpiConnectivityViewModel& spi_view_model();

 protected:
  void publish_all_();

 private:
  model::ConnectivityTestModel model_{};
  reactive::IntSubject selected_index_subject_;
  reactive::IntSubject active_page_subject_;
  WifiConnectivityViewModel wifi_view_model_{};
  BluetoothConnectivityViewModel bluetooth_view_model_{};
  EthernetConnectivityViewModel ethernet_view_model_{};
  UsbConnectivityViewModel usb_view_model_{};
  I2cConnectivityViewModel i2c_view_model_{};
  SpiConnectivityViewModel spi_view_model_{};
};

}  // namespace viewmodel
