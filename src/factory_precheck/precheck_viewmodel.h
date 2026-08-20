/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "precheck_model.h"

namespace precheck {

class PrecheckViewModel {
 public:
  explicit PrecheckViewModel(PrecheckModel& model) : model_(model) {}

  void start() { model_.start(); }
  void cancel() { model_.cancel(); }
  void retry_failed() { model_.retry_failed(); }

  bool safe_poweroff(std::string& error_message) { return model_.safe_poweroff(error_message); }
  bool safe_reboot(std::string& error_message) { return model_.safe_reboot(error_message); }
  Snapshot snapshot() const { return model_.snapshot(); }

 private:
  PrecheckModel& model_;
};

}  // namespace precheck
