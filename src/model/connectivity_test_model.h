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

enum class SubPage {
  MENU      = 0,
  WIFI      = 1,
  BLUETOOTH = 2,
  ETHERNET  = 3,
  USB       = 4,
  HDMI      = 5,
  I2C       = 6,
  SPI       = 7,
  UART      = 8,
  EXT_IO    = 9,
  LINK_TEST = 10,
};

struct MenuItem {
  const char* title;
  SubPage target_page;
};

struct ScanItem {
  std::string name;
  std::string detail;
  int32_t strength_percent{-1};
  std::string bssid;
};

struct InfoField {
  std::string label;
  std::string value;
};

struct ScanResult {
  std::vector<ScanItem> items;
  std::string error_message;
};

struct InfoResult {
  std::vector<InfoField> fields;
  std::string error_message;
};

enum class I2cAddressState {
  ABSENT,
  PRESENT,
  KERNEL_DRIVER,
};

struct I2cAddress {
  uint8_t address{0};
  I2cAddressState state{I2cAddressState::ABSENT};
};

struct I2cResult {
  std::vector<I2cAddress> addresses;
  std::string error_message;
};

enum class LinkTestStatus {
  IDLE,
  RUNNING,
  SUCCESS,
  FAILED,
};

struct LinkTestMetric {
  LinkTestStatus status{LinkTestStatus::IDLE};
  double value{0.0};
  std::string detail;
};

struct LinkTestSettings {
  std::string iperf_host{"192.168.10.187"};
  int iperf_port{5201};
};

struct LinkTestSnapshot {
  LinkTestMetric ping;
  LinkTestMetric wifi_iperf;
  LinkTestMetric ethernet_iperf;
  LinkTestSettings settings;
  bool running{false};
};

class ConnectivityTestModel {
 public:
  static constexpr std::size_t K_ITEM_COUNT = 10;

  const std::array<MenuItem, K_ITEM_COUNT>& items() const;
  std::size_t selected_index() const;
  SubPage active_page() const;
  const MenuItem& selected_item() const;
  void select_previous();
  void select_next();
  void set_selected_index(std::size_t index);
  void activate_selected();
  void show_subpage(SubPage page);
  void show_menu();

 private:
  std::size_t selected_index_{0};
  SubPage active_page_{SubPage::MENU};
};

class WifiConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ScanItem>& scan_items() const;
  const std::string& error_message() const;

 private:
  bool set_scan_result_(std::vector<ScanItem> items, std::string error_message);

  std::vector<ScanItem> scan_items_{};
  std::string error_message_{};
  std::future<ScanResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class BluetoothConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ScanItem>& scan_items() const;
  const std::string& error_message() const;

 private:
  bool set_scan_result_(std::vector<ScanItem> items, std::string error_message);

  std::vector<ScanItem> scan_items_{};
  std::string error_message_{};
  std::future<ScanResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class EthernetConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<InfoField>& fields() const;
  const std::string& error_message() const;

 private:
  bool set_info_(std::vector<InfoField> fields, std::string error_message);

  std::vector<InfoField> fields_{};
  std::string error_message_{};
  std::future<InfoResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class UsbConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ScanItem>& devices() const;
  const std::string& error_message() const;

 private:
  bool set_devices_(std::vector<ScanItem> devices, std::string error_message);

  std::vector<ScanItem> devices_{};
  std::string error_message_{};
  std::future<ScanResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class I2cConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  bool is_scanning() const;
  const std::vector<I2cAddress>& addresses() const;
  const std::string& error_message() const;

 private:
  bool set_addresses_(std::vector<I2cAddress> addresses, std::string error_message);

  std::vector<I2cAddress> addresses_{};
  std::string error_message_{};
  std::future<I2cResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class SpiConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<ScanItem>& devices() const;
  const std::string& error_message() const;

 private:
  bool set_devices_(std::vector<ScanItem> devices, std::string error_message);

  std::vector<ScanItem> devices_{};
  std::string error_message_{};
  std::future<ScanResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class HdmiConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const std::vector<InfoField>& fields() const;
  const std::string& error_message() const;

 private:
  bool set_info_(std::vector<InfoField> fields, std::string error_message);

  std::vector<InfoField> fields_{};
  std::string error_message_{};
  std::future<InfoResult> refresh_task_{};
  std::chrono::steady_clock::time_point last_refresh_start_{};
};

class LinkConnectivityModel {
 public:
  const char* title() const;
  bool refresh(bool force_refresh = false);
  const LinkTestSnapshot& snapshot() const;
  const LinkTestSettings& settings() const;
  void set_settings(LinkTestSettings settings);

 private:
  bool set_snapshot_(LinkTestSnapshot snapshot);
  LinkTestSnapshot running_snapshot_() const;

  LinkTestSettings settings_{};
  LinkTestSnapshot snapshot_{};
  std::future<LinkTestSnapshot> refresh_task_{};
};

}  // namespace model
