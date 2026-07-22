/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "cap_cc1101_service.h"

namespace model {

enum class CapCc1101RunState { WAITING_FOR_CAP, RUNNING, PASSED, FAILED, CANCELED };
enum class CapCc1101ItemState { PENDING, RUNNING, PASSED, FAILED };

struct CapCc1101ItemSnapshot {
  const char* name{nullptr};
  CapCc1101ItemState state{CapCc1101ItemState::PENDING};
  std::string detail{"Waiting"};
};

struct CapCc1101Snapshot {
  CapCc1101RunState state{CapCc1101RunState::WAITING_FOR_CAP};
  std::array<CapCc1101ItemSnapshot, 2> items{};
  std::string headline{"Install CAP-CC1101, then press Enter"};
  std::string error_message{};
  unsigned int revision{0};
};

class CapCc1101TestModel {
 public:
  CapCc1101TestModel();
  ~CapCc1101TestModel();

  CapCc1101TestModel(const CapCc1101TestModel&)            = delete;
  CapCc1101TestModel& operator=(const CapCc1101TestModel&) = delete;

  void start();
  void cancel();
  CapCc1101Snapshot snapshot() const;

 private:
  void run_();
  void publish_(const std::function<void(CapCc1101Snapshot&)>& update);

  mutable std::mutex mutex_{};
  CapCc1101Snapshot snapshot_{};
  std::atomic_bool cancel_requested_{false};
  std::thread worker_{};
};

const char* cap_cc1101_item_state_text(CapCc1101ItemState state);

}  // namespace model
