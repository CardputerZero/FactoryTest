/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform::connectivity {

struct WirelessScanItem {
  std::string name;
  std::string detail;
  int32_t strength_percent{-1};
};

struct EthernetInfo {
  std::string interface_name;
  std::string state;
  std::string hw_address;
  std::string ip4_address;
  std::string connection_name;
  bool connected{false};
};

struct UsbDeviceInfo {
  std::string name;
  std::string detail;
  std::string sys_path;
};

enum class I2cAddressState {
  ABSENT,
  PRESENT,
  KERNEL_DRIVER,
};

struct I2cAddressInfo {
  uint8_t address{0};
  I2cAddressState state{I2cAddressState::ABSENT};
};

struct SpiDeviceInfo {
  std::string name;
  std::string path;
};

struct LinkTestSettings {
  std::string iperf_host{"192.168.10.187"};
  int iperf_port{5201};
  int iperf_duration_seconds{3};
};

struct LinkPingResult {
  bool success{false};
  int ttl{-1};
  std::string message;
};

struct LinkIperfResult {
  bool success{false};
  double megabits_per_second{0.0};
  std::string interface_name;
  std::string message;
};

struct LinkTestResult {
  LinkPingResult ping;
  LinkIperfResult wifi;
  LinkIperfResult ethernet;
};

std::vector<WirelessScanItem> scan_wifi(std::string& error_message);
std::vector<WirelessScanItem> scan_bluetooth(std::string& error_message);
EthernetInfo read_ethernet_info(std::string& error_message);
std::vector<UsbDeviceInfo> list_usb_devices(std::string& error_message);
std::vector<I2cAddressInfo> scan_i2c_bus(int bus_number, std::string& error_message);
std::vector<SpiDeviceInfo> list_spi_devices(std::string& error_message);
LinkTestResult run_link_test(const LinkTestSettings& settings);

}  // namespace platform::connectivity
