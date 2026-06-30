/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace model {

enum class TestResult {
  PASS,
  FAILED,
  SKIPPED,
};

class TestSession {
 public:
  void start();
  bool active() const;
  const std::string& session_id() const;
  const std::string& test_time() const;
  const std::string& output_path() const;
  void append_result(const char* test_name, TestResult result);

 private:
  std::string session_id_{};
  std::string test_time_{};
  std::string output_path_{};
  bool active_{false};
};

}  // namespace model
