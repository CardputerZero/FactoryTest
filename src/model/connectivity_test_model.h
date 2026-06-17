/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <string>
#include <vector>

namespace model {

enum class ConnectivitySubPage {
  MENU      = 0,
  WIFI      = 1,
  BLUETOOTH = 2,
  ETHERNET  = 3,
  USB       = 4,
  I2C       = 5,
  SPI       = 6,
};

struct ConnectivityMenuItem {
  const char* title;
  ConnectivitySubPage target_page;
};

struct ConnectivityScanInfo {
  std::string name;
  std::string detail;
  int32_t strength_percent{-1};
};

struct ConnectivityInfoField {
  std::string label;
  std::string value;
};

struct ConnectivityScanRefreshResult {
  std::vector<ConnectivityScanInfo> items;
  std::string error_message;
};

struct ConnectivityInfoRefreshResult {
  std::vector<ConnectivityInfoField> fields;
  std::string error_message;
};

enum class ConnectivityI2cAddressState {
  ABSENT,
  PRESENT,
  KERNEL_DRIVER,
};

struct ConnectivityI2cAddressInfo {
  uint8_t address{0};
  ConnectivityI2cAddressState state{ConnectivityI2cAddressState::ABSENT};
};

struct ConnectivityI2cRefreshResult {
  std::vector<ConnectivityI2cAddressInfo> addresses;
  std::string error_message;
};

class ConnectivityTestModel {
 public:
  static constexpr std::size_t K_ITEM_COUNT = 6;

  const std::array<ConnectivityMenuItem, K_ITEM_COUNT>& items() const;
  std::size_t selected_index() const;
  ConnectivitySubPage active_page() const;
  const ConnectivityMenuItem& selected_item() const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void activate_selected();
  void show_menu();

 private:
  std::size_t selected_index_{0};
  ConnectivitySubPage active_page_{ConnectivitySubPage::MENU};
};

class WifiConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ConnectivityScanInfo>& scan_items() const;
  const std::string& error_message() const;

 private:
  bool set_scan_result_(std::vector<ConnectivityScanInfo> items, std::string error_message);

  std::vector<ConnectivityScanInfo> scan_items_{};
  std::string error_message_{};
  std::future<ConnectivityScanRefreshResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class BluetoothConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ConnectivityScanInfo>& scan_items() const;
  const std::string& error_message() const;

 private:
  bool set_scan_result_(std::vector<ConnectivityScanInfo> items, std::string error_message);

  std::vector<ConnectivityScanInfo> scan_items_{};
  std::string error_message_{};
  std::future<ConnectivityScanRefreshResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class EthernetConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ConnectivityInfoField>& fields() const;
  const std::string& error_message() const;

 private:
  bool set_info_(std::vector<ConnectivityInfoField> fields, std::string error_message);

  std::vector<ConnectivityInfoField> fields_{};
  std::string error_message_{};
  std::future<ConnectivityInfoRefreshResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class UsbConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ConnectivityScanInfo>& devices() const;
  const std::string& error_message() const;

 private:
  bool set_devices_(std::vector<ConnectivityScanInfo> devices, std::string error_message);

  std::vector<ConnectivityScanInfo> devices_{};
  std::string error_message_{};
  std::future<ConnectivityScanRefreshResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class I2cConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ConnectivityI2cAddressInfo>& addresses() const;
  const std::string& error_message() const;

 private:
  bool set_addresses_(std::vector<ConnectivityI2cAddressInfo> addresses, std::string error_message);

  std::vector<ConnectivityI2cAddressInfo> addresses_{};
  std::string error_message_{};
  std::future<ConnectivityI2cRefreshResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class SpiConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ConnectivityScanInfo>& devices() const;
  const std::string& error_message() const;

 private:
  bool set_devices_(std::vector<ConnectivityScanInfo> devices, std::string error_message);

  std::vector<ConnectivityScanInfo> devices_{};
  std::string error_message_{};
  std::future<ConnectivityScanRefreshResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

}  // namespace model
