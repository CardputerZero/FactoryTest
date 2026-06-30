/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "process_service.h"

namespace platform::perf {

enum class TestKind { CPU, MEMORY, SD_CARD };
enum class TestStatus { PASS, FAIL, WARNING };

struct TestCommand {
  TestKind kind{TestKind::CPU};
  std::string name{};
  std::string executable{};
  std::vector<std::string> args{};
  std::string output_path{};
  std::string work_path{};
  int timeout_ms{0};
  int duration_seconds{0};
};

struct TestProgress {
  TestKind kind{TestKind::CPU};
  std::string line{};
  bool stderr_line{false};
  int elapsed_seconds{0};
  int percent{0};
};

using ProgressCallback = std::function<void(const TestProgress& progress)>;

struct TestResult {
  TestKind kind{TestKind::CPU};
  std::string name{};
  std::string command_line{};
  process::ProcessResult process{};
  process::ParseResult structured_output{};
  std::map<std::string, std::string> metrics{};
  std::string summary{};
  bool passed{false};
  TestStatus status{TestStatus::FAIL};
};

TestCommand make_cpu_sysbench_command();
TestCommand make_memory_stress_ng_command();
TestCommand make_sd_card_fio_command();

std::string command_to_string(const TestCommand& command);
TestResult run_cpu_sysbench(ProgressCallback progress_callback = {});
TestResult run_memory_stress_ng(ProgressCallback progress_callback = {});
TestResult run_sd_card_fio(ProgressCallback progress_callback = {});

}  // namespace platform::perf
