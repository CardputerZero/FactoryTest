/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace model {
class CapFixtureTestModel;
class IrFixtureTestModel;
}

namespace precheck {

enum class RunState { IDLE, RUNNING, PASSED, FAILED, CANCELED };
enum class CheckState { PENDING, RUNNING, PASSED, FAILED, CANCELED, SKIPPED };

// One aggregate row of the factory_test CAP fixture page (I2C, Power, UART,
// SPI, USB, GPIO).  The precheck keeps its own display copy; the detection
// order and results themselves come from model::CapFixtureTestModel.
struct CapFixtureSubItem {
  std::string name;
  CheckState state{CheckState::PENDING};
  std::string detail;
};

struct IrFixtureSubItem {
  std::string name;
  CheckState state{CheckState::PENDING};
  std::string detail;
};

struct CheckItem {
  std::string id;
  std::string name;
  CheckState state{CheckState::PENDING};
  std::string detail;
  std::vector<CapFixtureSubItem> cap_sub_items;
  std::vector<IrFixtureSubItem> ir_sub_items;
};

struct Snapshot {
  RunState state{RunState::IDLE};
  std::size_t current_index{0};
  std::size_t completed{0};
  std::size_t passed{0};
  std::size_t failed{0};
  std::string device_name{"设备初检"};
  std::string serial_number{"--"};
  std::string headline{"按 Enter 开始自动初检"};
  std::vector<CheckItem> items;
};

class PrecheckModel {
 public:
  PrecheckModel();
  ~PrecheckModel();

  PrecheckModel(const PrecheckModel&) = delete;
  PrecheckModel& operator=(const PrecheckModel&) = delete;

  void start();
  void cancel();
  void retry_failed();

  bool safe_poweroff(std::string& error_message);
  bool safe_reboot(std::string& error_message);
  Snapshot snapshot() const;

 private:
  void run_();
  bool run_check_(std::size_t index);
  bool run_cap_fixture_check_(std::size_t index);
  bool run_ir_fixture_check_(std::size_t index);
  bool run_usb_port_check_(std::size_t index, uint32_t expected_bus, uint32_t expected_device);
  void publish_(RunState state, const std::string& headline);
  bool canceled_() const;

  mutable std::mutex mutex_;
  Snapshot snapshot_;
  std::atomic_bool cancel_requested_{false};
  std::thread worker_;
  std::unique_ptr<model::CapFixtureTestModel> cap_fixture_model_;
  std::unique_ptr<model::IrFixtureTestModel> ir_fixture_model_;
};

const char* run_state_text(RunState state);
const char* check_state_text(CheckState state);

}  // namespace precheck
