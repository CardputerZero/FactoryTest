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

namespace platform::spi {

struct DeviceConfig {
  std::string path;
  uint32_t speed_hz{1000000};
  uint8_t mode{0};
  uint8_t bits_per_word{8};
  bool lsb_first{false};
  bool no_chip_select{false};
};

class Device {
 public:
  ~Device();

  Device(const Device&)            = delete;
  Device& operator=(const Device&) = delete;

  static std::unique_ptr<Device> open(const DeviceConfig& config, std::string& error_message);
  bool transfer(const std::vector<uint8_t>& tx,
                std::vector<uint8_t>& rx,
                std::string& error_message);

 private:
  Device(int fd, DeviceConfig config);

  int fd_{-1};
  DeviceConfig config_{};
};

}  // namespace platform::spi
