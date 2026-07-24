/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace model {

enum class IrFixtureRunState { WAITING_FOR_FIXTURE, RUNNING, PASSED, FAILED, CANCELED };
enum class IrFixtureItemState { PENDING, RUNNING, PASSED, FAILED };

struct IrFixtureItemSnapshot {
  const char* name{nullptr};
  IrFixtureItemState state{IrFixtureItemState::PENDING};
  std::string detail{"Waiting"};
};

struct IrFixtureSnapshot {
  IrFixtureRunState state{IrFixtureRunState::WAITING_FOR_FIXTURE};
  std::array<IrFixtureItemSnapshot, 2> items{};
  std::string headline{"Prepare IR fixture, then press Enter"};
  std::string error_message{};
  unsigned int revision{0};
};

class IrFixtureTestModel {
 public:
  IrFixtureTestModel();
  ~IrFixtureTestModel();

  IrFixtureTestModel(const IrFixtureTestModel&)            = delete;
  IrFixtureTestModel& operator=(const IrFixtureTestModel&) = delete;

  void start();
  void cancel();
  IrFixtureSnapshot snapshot() const;

 private:
  void run_();
  bool delay_(unsigned int milliseconds);
  void fail_(std::size_t item_index, const std::string& message);

  mutable std::mutex mutex_{};
  IrFixtureSnapshot snapshot_{};
  std::atomic_bool cancel_requested_{false};
  std::thread worker_{};
};

const char* ir_fixture_item_state_text(IrFixtureItemState state);

}  // namespace model
