/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>
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

enum class UartOpenStatus {
  OK,
  OCCUPIED_BY_CONSOLE,
  OPEN_FAILED,
  UNSUPPORTED,
};

struct UartOpenResult {
  UartOpenStatus status{UartOpenStatus::UNSUPPORTED};
  std::string message;
};

class UartDebugSession {
 public:
  ~UartDebugSession();

  UartDebugSession(const UartDebugSession&)            = delete;
  UartDebugSession& operator=(const UartDebugSession&) = delete;

  static std::unique_ptr<UartDebugSession> open(const std::string& path,
                                                int baud_rate,
                                                UartOpenResult& result);
  int baud_rate() const;
  bool set_baud_rate(int baud_rate, std::string& error_message);
  std::string read_available(std::string& error_message);
  bool write_text(const std::string& text, std::string& error_message);

 private:
  UartDebugSession(int fd, std::string path, int baud_rate);

  int fd_{-1};
  std::string path_;
  int baud_rate_{115200};
};

std::vector<WirelessScanItem> scan_wifi(std::string& error_message);
std::vector<WirelessScanItem> scan_bluetooth(std::string& error_message);
EthernetInfo read_ethernet_info(std::string& error_message);
std::vector<UsbDeviceInfo> list_usb_devices(std::string& error_message);
std::vector<I2cAddressInfo> scan_i2c_bus(int bus_number, std::string& error_message);
std::vector<SpiDeviceInfo> list_spi_devices(std::string& error_message);
HdmiInfo read_hdmi_info(std::string& error_message);
LinkTestResult run_link_test(const LinkTestSettings& settings);

}  // namespace platform::connectivity
