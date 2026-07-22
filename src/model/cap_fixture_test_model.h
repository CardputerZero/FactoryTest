/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <thread>

namespace model {

enum class CapFixtureRunState {
  IDLE,
  RUNNING,
  PASSED,
  FAILED,
  CANCELED,
};

enum class CapFixtureItemState {
  PENDING,
  RUNNING,
  PASSED,
  FAILED,
};

struct CapFixtureItemSnapshot {
  const char* name{nullptr};
  CapFixtureItemState state{CapFixtureItemState::PENDING};
  std::string detail{"Waiting"};
};

struct CapFixtureSnapshot {
  CapFixtureRunState state{CapFixtureRunState::IDLE};
  std::array<CapFixtureItemSnapshot, 6> items{};
  std::size_t active_index{0};
  std::string headline{"Install CAP fixture, then press Enter"};
  std::string error_message{};
  unsigned int revision{0};
};

class CapFixtureTestModel {
 public:
  CapFixtureTestModel();
  ~CapFixtureTestModel();

  CapFixtureTestModel(const CapFixtureTestModel&)            = delete;
  CapFixtureTestModel& operator=(const CapFixtureTestModel&) = delete;

  void start();
  void cancel();
  CapFixtureSnapshot snapshot() const;

 private:
  void run_();
  bool run_i2c_();
  bool run_power_();
  bool run_uart_();
  bool run_spi_();
  bool run_usb_();
  bool run_gpio_();
  bool delay_(const char* stage, unsigned int milliseconds, const char* reason);
  bool hold_gpio_outputs_high_(const char* stage, unsigned int milliseconds, const char* reason);
  bool write_reg_(const char* stage, unsigned int reg, const unsigned char* data, std::size_t size);
  bool write_reg_(const char* stage, unsigned int reg, unsigned int value);
  bool read_reg_(const char* stage, unsigned int reg, unsigned char* data, std::size_t size);
  bool fail_(const char* stage, const std::string& message);
  void begin_item_(std::size_t index, const char* detail);
  void pass_item_(std::size_t index, const char* detail);
  void cleanup_();
  void publish_(CapFixtureRunState state, const std::string& headline);

  mutable std::mutex mutex_;
  CapFixtureSnapshot snapshot_{};
  std::atomic_bool cancel_requested_{false};
  std::thread worker_{};
  std::size_t active_item_{6};
};

const char* cap_fixture_item_state_text(CapFixtureItemState state);

}  // namespace model
