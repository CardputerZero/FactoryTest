/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_test_model.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <utility>

#include "bluetooth_service.h"
#include "ethernet_service.h"
#include "hdmi_service.h"
#include "i2c_service.h"
#include "link_service.h"
#include "spi_service.h"
#include "usb_service.h"
#include "wifi_service.h"

namespace model {
namespace {

using Clock = std::chrono::steady_clock;

constexpr auto K_BACKEND_REFRESH_INTERVAL = std::chrono::milliseconds(5000);

const std::array<ConnectivityMenuItem, ConnectivityTestModel::K_ITEM_COUNT>& connectivity_items() {
  static constexpr std::array<ConnectivityMenuItem, ConnectivityTestModel::K_ITEM_COUNT> ITEMS = {{
      {"Wi-Fi", ConnectivitySubPage::WIFI},
      {"Bluetooth", ConnectivitySubPage::BLUETOOTH},
      {"Ethernet", ConnectivitySubPage::ETHERNET},
      {"USB", ConnectivitySubPage::USB},
      {"HDMI", ConnectivitySubPage::HDMI},
      {"I2C", ConnectivitySubPage::I2C},
      {"SPI", ConnectivitySubPage::SPI},
      {"UART", ConnectivitySubPage::UART},
      {"EXT.IO", ConnectivitySubPage::EXT_IO},
      {"Link Test", ConnectivitySubPage::LINK_TEST},
  }};
  return ITEMS;
}

bool same_scan_info(const ConnectivityScanInfo& left, const ConnectivityScanInfo& right) {
  return left.name == right.name && left.detail == right.detail &&
         left.strength_percent == right.strength_percent;
}

bool same_scan_infos(const std::vector<ConnectivityScanInfo>& left,
                     const std::vector<ConnectivityScanInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_scan_info);
}

bool same_info_field(const ConnectivityInfoField& left, const ConnectivityInfoField& right) {
  return left.label == right.label && left.value == right.value;
}

bool same_info_fields(const std::vector<ConnectivityInfoField>& left,
                      const std::vector<ConnectivityInfoField>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_info_field);
}

bool same_i2c_address(const ConnectivityI2cAddressInfo& left,
                      const ConnectivityI2cAddressInfo& right) {
  return left.address == right.address && left.state == right.state;
}

bool same_i2c_addresses(const std::vector<ConnectivityI2cAddressInfo>& left,
                        const std::vector<ConnectivityI2cAddressInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_i2c_address);
}

bool same_link_metric(const LinkTestMetric& left, const LinkTestMetric& right) {
  return left.status == right.status && left.value == right.value && left.detail == right.detail;
}

bool same_link_settings(const LinkTestSettings& left, const LinkTestSettings& right) {
  return left.iperf_host == right.iperf_host && left.iperf_port == right.iperf_port;
}

bool same_link_snapshot(const LinkTestSnapshot& left, const LinkTestSnapshot& right) {
  return same_link_metric(left.ping, right.ping) &&
         same_link_metric(left.wifi_iperf, right.wifi_iperf) &&
         same_link_metric(left.ethernet_iperf, right.ethernet_iperf) &&
         same_link_settings(left.settings, right.settings) && left.running == right.running;
}

template <typename Result>
bool future_ready(std::future<Result>& future) {
  return future.valid() &&
         future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

bool refresh_interval_elapsed(Clock::time_point last_refresh_start) {
  return last_refresh_start == Clock::time_point{} ||
         Clock::now() - last_refresh_start >= K_BACKEND_REFRESH_INTERVAL;
}

std::vector<ConnectivityScanInfo> to_scan_infos(
    std::vector<platform::connectivity::WirelessScanItem> items) {
  std::vector<ConnectivityScanInfo> result;
  result.reserve(items.size());
  for (auto& item : items) {
    result.push_back({std::move(item.name), std::move(item.detail), item.strength_percent});
  }
  return result;
}

ConnectivityI2cAddressState to_i2c_address_state(platform::connectivity::I2cAddressState state) {
  switch (state) {
    case platform::connectivity::I2cAddressState::PRESENT:
      return ConnectivityI2cAddressState::PRESENT;
    case platform::connectivity::I2cAddressState::KERNEL_DRIVER:
      return ConnectivityI2cAddressState::KERNEL_DRIVER;
    case platform::connectivity::I2cAddressState::ABSENT:
    default:
      return ConnectivityI2cAddressState::ABSENT;
  }
}

std::vector<ConnectivityI2cAddressInfo> to_i2c_addresses(
    std::vector<platform::connectivity::I2cAddressInfo> addresses) {
  std::vector<ConnectivityI2cAddressInfo> result;
  result.reserve(addresses.size());
  for (const auto& address : addresses) {
    result.push_back({address.address, to_i2c_address_state(address.state)});
  }
  return result;
}

ConnectivityScanRefreshResult read_wifi_scan_result() {
  std::string error;
  auto items = platform::connectivity::scan_wifi(error);
  return {to_scan_infos(std::move(items)), std::move(error)};
}

ConnectivityScanRefreshResult read_bluetooth_scan_result() {
  std::string error;
  auto items = platform::connectivity::scan_bluetooth(error);
  return {to_scan_infos(std::move(items)), std::move(error)};
}

ConnectivityInfoRefreshResult read_ethernet_refresh_result() {
  std::string error;
  const auto info = platform::connectivity::read_ethernet_info(error);

  std::vector<ConnectivityInfoField> fields;
  if (!info.interface_name.empty()) {
    fields.push_back({"Interface", info.interface_name});
  }
  if (!info.state.empty()) {
    fields.push_back({"State", info.state});
  }
  if (!info.connection_name.empty()) {
    fields.push_back({"Connection", info.connection_name});
  }
  if (!info.ip4_address.empty()) {
    fields.push_back({"IPv4", info.ip4_address});
  }
  if (!info.hw_address.empty()) {
    fields.push_back({"MAC", info.hw_address});
  }
  if (fields.empty() && error.empty()) {
    error = "No Ethernet information";
  }
  return {std::move(fields), std::move(error)};
}

ConnectivityScanRefreshResult read_usb_refresh_result() {
  std::string error;
  auto devices = platform::connectivity::list_usb_devices(error);

  std::vector<ConnectivityScanInfo> items;
  items.reserve(devices.size());
  for (auto& device : devices) {
    items.push_back({std::move(device.name), std::move(device.detail), -1});
  }
  return {std::move(items), std::move(error)};
}

ConnectivityI2cRefreshResult read_i2c_refresh_result() {
  std::string error;
  auto addresses = platform::connectivity::scan_i2c_bus(1, error);
  return {to_i2c_addresses(std::move(addresses)), std::move(error)};
}

ConnectivityScanRefreshResult read_spi_refresh_result() {
  std::string error;
  auto devices = platform::connectivity::list_spi_devices(error);

  std::vector<ConnectivityScanInfo> items;
  items.reserve(devices.size());
  for (auto& device : devices) {
    items.push_back({std::move(device.name), std::move(device.path), -1});
  }
  return {std::move(items), std::move(error)};
}

ConnectivityInfoRefreshResult read_hdmi_refresh_result() {
  std::string error;
  const auto info = platform::connectivity::read_hdmi_info(error);

  std::vector<ConnectivityInfoField> fields;
  if (!info.connector_name.empty()) {
    fields.push_back({"Connector", info.connector_name});
  }
  if (!info.status.empty()) {
    fields.push_back({"Status", info.status});
  }
  if (!info.resolution.empty()) {
    fields.push_back({"Resolution", info.resolution});
  }
  if (!info.enabled.empty()) {
    fields.push_back({"Enabled", info.enabled});
  }
  if (fields.empty() && error.empty()) {
    error = "No HDMI information";
  }
  return {std::move(fields), std::move(error)};
}

LinkTestMetric ping_metric(const platform::connectivity::LinkPingResult& result) {
  return {result.success ? LinkTestStatus::SUCCESS : LinkTestStatus::FAILED,
          static_cast<double>(result.ttl),
          result.message};
}

LinkTestMetric iperf_metric(const platform::connectivity::LinkIperfResult& result) {
  std::string detail = result.message;
  if (!result.interface_name.empty()) {
    detail = result.interface_name + (detail.empty() ? std::string{} : std::string{": "} + detail);
  }
  return {result.success ? LinkTestStatus::SUCCESS : LinkTestStatus::FAILED,
          result.megabits_per_second,
          std::move(detail)};
}

LinkTestSnapshot read_link_test_result(LinkTestSettings settings) {
  platform::connectivity::LinkTestSettings platform_settings;
  platform_settings.iperf_host = settings.iperf_host;
  platform_settings.iperf_port = settings.iperf_port;

  const auto result = platform::connectivity::run_link_test(platform_settings);
  LinkTestSnapshot snapshot;
  snapshot.settings       = std::move(settings);
  snapshot.ping           = ping_metric(result.ping);
  snapshot.wifi_iperf     = iperf_metric(result.wifi);
  snapshot.ethernet_iperf = iperf_metric(result.ethernet);
  snapshot.running        = false;
  return snapshot;
}

}  // namespace

const std::array<ConnectivityMenuItem, ConnectivityTestModel::K_ITEM_COUNT>&
ConnectivityTestModel::items() const {
  return connectivity_items();
}

std::size_t ConnectivityTestModel::selected_index() const { return selected_index_; }

ConnectivitySubPage ConnectivityTestModel::active_page() const { return active_page_; }

const ConnectivityMenuItem& ConnectivityTestModel::selected_item() const {
  return items()[selected_index_];
}

void ConnectivityTestModel::select_previous() {
  if (selected_index_ > 0) {
    --selected_index_;
  }
}

void ConnectivityTestModel::select_next() {
  if (selected_index_ + 1 < items().size()) {
    ++selected_index_;
  }
}

void ConnectivityTestModel::set_selected_index(std::size_t index) {
  selected_index_ = std::min(index, items().size() - 1);
}

void ConnectivityTestModel::activate_selected() { active_page_ = selected_item().target_page; }

void ConnectivityTestModel::show_subpage(ConnectivitySubPage page) {
  if (page == ConnectivitySubPage::MENU) {
    show_menu();
    return;
  }

  for (std::size_t i = 0; i < items().size(); ++i) {
    if (items()[i].target_page == page) {
      selected_index_ = i;
      active_page_    = page;
      return;
    }
  }
}

void ConnectivityTestModel::show_menu() { active_page_ = ConnectivitySubPage::MENU; }

const char* WifiConnectivityModel::title() const { return "Wi-Fi Scan"; }

bool WifiConnectivityModel::refresh(bool force_refresh) {
  bool changed = false;
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_scan_result_(std::move(result.items), std::move(result.error_message));
  }

  if (!refresh_task_.valid() && (force_refresh || refresh_interval_elapsed(last_refresh_start_))) {
    last_refresh_start_ = Clock::now();
    refresh_task_       = std::async(std::launch::async, read_wifi_scan_result);
    if (scan_items_.empty() && error_message_.empty()) {
      changed |= set_scan_result_({}, "Scanning Wi-Fi...");
    }
  }
  return changed;
}

const std::vector<ConnectivityScanInfo>& WifiConnectivityModel::scan_items() const {
  return scan_items_;
}

const std::string& WifiConnectivityModel::error_message() const { return error_message_; }

bool WifiConnectivityModel::set_scan_result_(std::vector<ConnectivityScanInfo> items,
                                             std::string error_message) {
  if (error_message_ == error_message && same_scan_infos(scan_items_, items)) {
    return false;
  }

  scan_items_    = std::move(items);
  error_message_ = std::move(error_message);
  return true;
}

const char* BluetoothConnectivityModel::title() const { return "Bluetooth Scan"; }

bool BluetoothConnectivityModel::refresh(bool force_refresh) {
  bool changed = false;
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_scan_result_(std::move(result.items), std::move(result.error_message));
  }

  if (!refresh_task_.valid() && (force_refresh || refresh_interval_elapsed(last_refresh_start_))) {
    last_refresh_start_ = Clock::now();
    refresh_task_       = std::async(std::launch::async, read_bluetooth_scan_result);
    if (scan_items_.empty() && error_message_.empty()) {
      changed |= set_scan_result_({}, "Scanning Bluetooth...");
    }
  }
  return changed;
}

const std::vector<ConnectivityScanInfo>& BluetoothConnectivityModel::scan_items() const {
  return scan_items_;
}

const std::string& BluetoothConnectivityModel::error_message() const { return error_message_; }

bool BluetoothConnectivityModel::set_scan_result_(std::vector<ConnectivityScanInfo> items,
                                                  std::string error_message) {
  if (error_message_ == error_message && same_scan_infos(scan_items_, items)) {
    return false;
  }

  scan_items_    = std::move(items);
  error_message_ = std::move(error_message);
  return true;
}

const char* EthernetConnectivityModel::title() const { return "Ethernet Info"; }

bool EthernetConnectivityModel::refresh(bool force_refresh) {
  bool changed = false;
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_info_(std::move(result.fields), std::move(result.error_message));
  }

  if (!refresh_task_.valid() && (force_refresh || refresh_interval_elapsed(last_refresh_start_))) {
    last_refresh_start_ = Clock::now();
    refresh_task_       = std::async(std::launch::async, read_ethernet_refresh_result);
    if (fields_.empty() && error_message_.empty()) {
      changed |= set_info_({}, "Reading Ethernet...");
    }
  }
  return changed;
}

const std::vector<ConnectivityInfoField>& EthernetConnectivityModel::fields() const {
  return fields_;
}

const std::string& EthernetConnectivityModel::error_message() const { return error_message_; }

bool EthernetConnectivityModel::set_info_(std::vector<ConnectivityInfoField> fields,
                                          std::string error_message) {
  if (error_message_ == error_message && same_info_fields(fields_, fields)) {
    return false;
  }

  fields_        = std::move(fields);
  error_message_ = std::move(error_message);
  return true;
}

const char* UsbConnectivityModel::title() const { return "USB Devices"; }

bool UsbConnectivityModel::refresh(bool force_refresh) {
  bool changed = false;
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_devices_(std::move(result.items), std::move(result.error_message));
  }

  if (!refresh_task_.valid() && (force_refresh || refresh_interval_elapsed(last_refresh_start_))) {
    last_refresh_start_ = Clock::now();
    refresh_task_       = std::async(std::launch::async, read_usb_refresh_result);
    if (devices_.empty() && error_message_.empty()) {
      changed |= set_devices_({}, "Scanning USB...");
    }
  }
  return changed;
}

const std::vector<ConnectivityScanInfo>& UsbConnectivityModel::devices() const { return devices_; }

const std::string& UsbConnectivityModel::error_message() const { return error_message_; }

bool UsbConnectivityModel::set_devices_(std::vector<ConnectivityScanInfo> devices,
                                        std::string error_message) {
  if (error_message_ == error_message && same_scan_infos(devices_, devices)) {
    return false;
  }

  devices_       = std::move(devices);
  error_message_ = std::move(error_message);
  return true;
}

const char* I2cConnectivityModel::title() const { return "I2C Scan"; }

bool I2cConnectivityModel::refresh(bool force_refresh) {
  bool changed            = false;
  const bool was_scanning = refresh_task_.valid();
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_addresses_(std::move(result.addresses), std::move(result.error_message));
  }

  bool started_scan = false;
  if (!refresh_task_.valid() && (force_refresh || refresh_interval_elapsed(last_refresh_start_))) {
    last_refresh_start_ = Clock::now();
    refresh_task_       = std::async(std::launch::async, read_i2c_refresh_result);
    started_scan        = true;
  }
  return changed || started_scan || (was_scanning != refresh_task_.valid());
}

bool I2cConnectivityModel::is_scanning() const { return refresh_task_.valid(); }

const std::vector<ConnectivityI2cAddressInfo>& I2cConnectivityModel::addresses() const {
  return addresses_;
}

const std::string& I2cConnectivityModel::error_message() const { return error_message_; }

bool I2cConnectivityModel::set_addresses_(std::vector<ConnectivityI2cAddressInfo> addresses,
                                          std::string error_message) {
  if (error_message_ == error_message && same_i2c_addresses(addresses_, addresses)) {
    return false;
  }

  addresses_     = std::move(addresses);
  error_message_ = std::move(error_message);
  return true;
}

const char* SpiConnectivityModel::title() const { return "SPI Scan"; }

bool SpiConnectivityModel::refresh(bool force_refresh) {
  bool changed = false;
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_devices_(std::move(result.items), std::move(result.error_message));
  }

  if (!refresh_task_.valid() && (force_refresh || refresh_interval_elapsed(last_refresh_start_))) {
    last_refresh_start_ = Clock::now();
    refresh_task_       = std::async(std::launch::async, read_spi_refresh_result);
    if (devices_.empty() && error_message_.empty()) {
      changed |= set_devices_({}, "Scanning SPI...");
    }
  }
  return changed;
}

const std::vector<ConnectivityScanInfo>& SpiConnectivityModel::devices() const { return devices_; }

const std::string& SpiConnectivityModel::error_message() const { return error_message_; }

bool SpiConnectivityModel::set_devices_(std::vector<ConnectivityScanInfo> devices,
                                        std::string error_message) {
  if (error_message_ == error_message && same_scan_infos(devices_, devices)) {
    return false;
  }

  devices_       = std::move(devices);
  error_message_ = std::move(error_message);
  return true;
}

const char* HdmiConnectivityModel::title() const { return "HDMI Info"; }

bool HdmiConnectivityModel::refresh(bool force_refresh) {
  bool changed = false;
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_info_(std::move(result.fields), std::move(result.error_message));
  }

  if (!refresh_task_.valid() && (force_refresh || refresh_interval_elapsed(last_refresh_start_))) {
    last_refresh_start_ = Clock::now();
    refresh_task_       = std::async(std::launch::async, read_hdmi_refresh_result);
    if (fields_.empty() && error_message_.empty()) {
      changed |= set_info_({}, "Reading HDMI...");
    }
  }
  return changed;
}

const std::vector<ConnectivityInfoField>& HdmiConnectivityModel::fields() const { return fields_; }

const std::string& HdmiConnectivityModel::error_message() const { return error_message_; }

bool HdmiConnectivityModel::set_info_(std::vector<ConnectivityInfoField> fields,
                                      std::string error_message) {
  if (error_message_ == error_message && same_info_fields(fields_, fields)) {
    return false;
  }

  fields_        = std::move(fields);
  error_message_ = std::move(error_message);
  return true;
}

const char* LinkConnectivityModel::title() const { return "Network Link Test"; }

bool LinkConnectivityModel::refresh(bool force_refresh) {
  bool changed = false;
  if (future_ready(refresh_task_)) {
    auto result = refresh_task_.get();
    changed |= set_snapshot_(std::move(result));
  }

  if (!refresh_task_.valid() && (force_refresh || snapshot_.ping.status == LinkTestStatus::IDLE)) {
    refresh_task_ = std::async(std::launch::async, read_link_test_result, settings_);
    changed |= set_snapshot_(running_snapshot_());
  }
  return changed;
}

const LinkTestSnapshot& LinkConnectivityModel::snapshot() const { return snapshot_; }

const LinkTestSettings& LinkConnectivityModel::settings() const { return settings_; }

void LinkConnectivityModel::set_settings(LinkTestSettings settings) {
  if (settings.iperf_host.empty()) {
    settings.iperf_host = "192.168.10.187";
  }
  if (settings.iperf_port <= 0 || settings.iperf_port > 65535) {
    settings.iperf_port = 5201;
  }
  settings_          = std::move(settings);
  snapshot_.settings = settings_;
}

bool LinkConnectivityModel::set_snapshot_(LinkTestSnapshot snapshot) {
  if (same_link_snapshot(snapshot_, snapshot)) {
    return false;
  }

  snapshot_ = std::move(snapshot);
  return true;
}

LinkTestSnapshot LinkConnectivityModel::running_snapshot_() const {
  LinkTestSnapshot snapshot;
  snapshot.settings       = settings_;
  snapshot.running        = true;
  snapshot.ping           = {LinkTestStatus::RUNNING, 0.0, "Pinging 8.8.8.8..."};
  snapshot.wifi_iperf     = {LinkTestStatus::RUNNING, 0.0, "Waiting for iperf..."};
  snapshot.ethernet_iperf = {LinkTestStatus::RUNNING, 0.0, "Waiting for iperf..."};
  return snapshot;
}

}  // namespace model
