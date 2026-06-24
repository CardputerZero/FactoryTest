/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform::ir {

struct IrDeviceInfo {
  bool available{false};
  std::string rc_name;
  std::string lirc_path;
  std::string driver_name;
  std::string device_name;
  std::string error_message;
  uint32_t features{0};
};

struct IrSendResult {
  bool success{false};
  uint16_t address{0};
  uint16_t command{0};
  std::vector<uint8_t> data;
  std::string protocol;
  std::string device_path;
  std::string message;
};

struct IrReceiveSnapshot {
  bool opened{false};
  bool signal_seen{false};
  bool data_changed{false};
  bool address_matched{false};
  bool address_filter_enabled{false};
  uint16_t expected_address{0};
  uint16_t address{0};
  uint16_t command{0};
  std::vector<uint8_t> data;
  std::string protocol;
  std::string device_path;
  std::string raw_summary;
  std::string message;
};

IrDeviceInfo read_receiver_info();
IrDeviceInfo read_sender_info();
IrSendResult send_nec_packet(uint16_t address);
std::string format_hex_bytes(const std::vector<uint8_t>& data);
std::string format_nec_address(uint16_t address);
std::string format_nec_command(uint16_t command);

class IrReceiverSession {
 public:
  IrReceiverSession();
  ~IrReceiverSession();

  IrReceiverSession(const IrReceiverSession&)            = delete;
  IrReceiverSession& operator=(const IrReceiverSession&) = delete;

  bool start(uint16_t expected_address, bool filter_address = true);
  void stop();
  IrReceiveSnapshot poll();
  const IrReceiveSnapshot& snapshot() const;

 private:
  int fd_{-1};
  bool scancode_mode_{false};
  bool mode2_{true};
  std::vector<uint32_t> pulse_space_buffer_{};
  IrReceiveSnapshot snapshot_{};
};

}  // namespace platform::ir
