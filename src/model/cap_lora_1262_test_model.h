/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "cap_lora_1262_service.h"

namespace model {

enum class CapLora1262RunState {
  WAITING_FOR_CAP,
  RUNNING_LORA,
  RUNNING_GPS,
  PASSED,
  FAILED,
  CANCELED,
};

enum class CapLora1262ItemState {
  PENDING,
  RUNNING,
  PASSED,
  FAILED,
};

struct CapLora1262ItemSnapshot {
  const char* name{nullptr};
  CapLora1262ItemState state{CapLora1262ItemState::PENDING};
  std::string detail{"Waiting"};
};

struct CapLora1262Snapshot {
  CapLora1262RunState state{CapLora1262RunState::WAITING_FOR_CAP};
  std::array<CapLora1262ItemSnapshot, 2> items{};
  std::string headline{"Install CAP LoRa-1262, then press Enter"};
  std::string error_message{};
  unsigned int revision{0};
};

class CapLora1262TestModel {
 public:
  CapLora1262TestModel();
  ~CapLora1262TestModel();

  CapLora1262TestModel(const CapLora1262TestModel&)            = delete;
  CapLora1262TestModel& operator=(const CapLora1262TestModel&) = delete;

  void start();
  void cancel();
  CapLora1262Snapshot snapshot() const;

 private:
  void run_();
  void publish_(const std::function<void(CapLora1262Snapshot&)>& update);

  mutable std::mutex mutex_{};
  CapLora1262Snapshot snapshot_{};
  std::atomic_bool cancel_requested_{false};
  std::thread worker_{};
};

const char* cap_lora_1262_item_state_text(CapLora1262ItemState state);

}  // namespace model
