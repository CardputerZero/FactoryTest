/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "gpio_service.h"

namespace platform::cap_cc1101 {

struct Cc1101Config {
  std::string spi_path{"/dev/spidev0.1"};
  uint32_t spi_speed_hz{4000000};
};

struct Cc1101Result {
  bool detected{false};
  bool initialized{false};
  uint8_t part_number{0};
  uint8_t version{0};
  uint8_t chip_status{0};
  uint8_t marc_state{0};
  uint8_t packet_status{0};
  std::string error_message{};
};

struct St25r3916Config {
  // Reuse an exported SPI node for bus access while suppressing its hardware CS.
  std::string spi_path{"/dev/spidev0.1"};
  uint32_t spi_speed_hz{5000000};
  gpio::OutputLineConfig software_cs{"/dev/gpiochip0", 22, "factory-test-st25r3916-cs"};
};

struct St25r3916Result {
  bool detected{false};
  bool communication_verified{false};
  uint8_t identity{0};
  uint8_t identity_readback{0};
  uint8_t operation_control{0};
  uint8_t fifo_status1{0};
  uint8_t fifo_status2{0};
  std::string error_message{};
};

using ProgressCallback = std::function<void(const std::string&)>;

Cc1101Result run_cc1101_test(const Cc1101Config& config,
                             const std::atomic_bool& cancel_requested,
                             const ProgressCallback& progress = {});

St25r3916Result run_st25r3916_test(const St25r3916Config& config,
                                   const std::atomic_bool& cancel_requested,
                                   const ProgressCallback& progress = {});

}  // namespace platform::cap_cc1101
