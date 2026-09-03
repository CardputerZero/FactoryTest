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

// USB 设备按物理拓扑位置描述。port_path 对应 sysfs devpath，例如 Hub 位于
// "1" 时，其下游 Port 001 / 002 分别为 "1.1" / "1.2"。
struct UsbPortDeviceInfo {
  uint32_t bus{0};
  uint32_t device{0};
  std::string port_path;
  std::string vid;
  std::string pid;
  std::string name;
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

struct LinkInternetResult {
  bool success{false};
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
  LinkInternetResult internet;
  LinkIperfResult wifi;
  LinkIperfResult ethernet;
};

}  // namespace platform::connectivity
