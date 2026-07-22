/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include "gpio_service.h"

namespace platform::cap_lora_1262 {

struct OptionalGpio {
  bool configured{false};
  platform::gpio::OutputLineConfig line{};
  bool active_high{true};
};

struct HatPowerConfig {
  bool configured{true};
  std::string brightness_path{"/sys/class/leds/ext_5v_out/brightness"};
  uint32_t settle_time_ms{150};
};

struct RadioConfig {
  std::string spi_path{"/dev/spidev0.1"};
  uint32_t spi_speed_hz{1000000};
  OptionalGpio reset{true, {"/dev/gpiochip0", 26, "factory-test-lora-reset"}, false};
  OptionalGpio busy{true, {"/dev/gpiochip0", 22, "factory-test-lora-busy"}, true};
  HatPowerConfig power{};
};

struct RadioTestResult {
  bool detected{false};
  uint8_t status{0};
  std::string spi_path{};
  std::string error_message{};
};

using ProgressCallback = std::function<void(const std::string&)>;

RadioTestResult run_radio_test(const RadioConfig& config,
                               const std::atomic_bool& cancel_requested,
                               const ProgressCallback& progress = {});

struct GpsTestResult {
  bool nmea_detected{false};
  unsigned int valid_sentence_count{0};
  std::string last_sentence_type{};
  std::string ic_info{};
  std::string error_message{};
};

GpsTestResult run_gps_test(const std::string& uart_path,
                           int baud_rate,
                           std::chrono::milliseconds timeout,
                           const std::atomic_bool& cancel_requested,
                           const std::function<void(const GpsTestResult&)>& progress = {});

}  // namespace platform::cap_lora_1262
