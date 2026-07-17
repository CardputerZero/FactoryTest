/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace platform::connectivity {

struct WirelessScanItem {
  std::string name;
  std::string detail;
  int32_t strength_percent{-1};
  std::string bssid;
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

struct HdmiInfo {
  std::string connector_name;
  std::string status;
  std::string enabled;
  std::string resolution;
  std::string sys_path;
  bool connected{false};
};

struct LinkTestSettings {
  std::string iperf_host{"192.168.10.187"};
  int iperf_port{5201};
  int iperf_duration_seconds{3};
};

struct LinkPingResult {
  bool success{false};
  int ttl{-1};
  double latency_ms{-1.0};
  std::string host;
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

}  // namespace platform::connectivity
