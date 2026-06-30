/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cpu_benchmark_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

#include "logger.h"

namespace platform::perf {
namespace {

constexpr int K_CPU_DURATION_SECONDS    = 10;
constexpr int K_MEMORY_DURATION_SECONDS = 30;
constexpr int K_SD_DURATION_SECONDS     = 20;

constexpr const char* K_STRESS_NG_YAML_PATH = "/tmp/factory_test_mem_stress.yaml";
constexpr const char* K_FIO_DATA_PATH       = "/var/tmp/factory_test_sdcard.bin";

void mark_passed(TestResult& result, bool passed) {
  result.passed = passed;
  result.status = passed ? TestStatus::PASS : TestStatus::FAIL;
}

std::string quote_arg(const std::string& arg) {
  if (arg.empty()) {
    return "''";
  }
  if (arg.find_first_of(" \t\n\"'\\$`!&|;<>()[]{}*") == std::string::npos) {
    return arg;
  }

  std::string quoted = "'";
  for (const auto ch : arg) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

std::string number_string(double value, int precision = 2) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << value;
  return out.str();
}

std::string bytes_per_second_string(double bytes_per_second) {
  constexpr const char* units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
  std::size_t unit_index        = 0;
  while (bytes_per_second >= 1024.0 && unit_index + 1 < std::size(units)) {
    bytes_per_second /= 1024.0;
    ++unit_index;
  }
  return number_string(bytes_per_second, unit_index == 0 ? 0 : 2) + " " + units[unit_index];
}

std::string milliseconds_string_from_ns(double ns) {
  return number_string(ns / 1000000.0, 2) + " ms";
}

std::string trim(std::string value) {
  const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

bool executable_exists(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::exists(path, ec) && !std::filesystem::is_directory(path, ec);
}

bool executable_on_path(const std::string& executable) {
  if (executable.empty()) {
    return false;
  }

  if (executable.find('/') != std::string::npos) {
    return executable_exists(executable);
  }

  const char* raw_path = std::getenv("PATH");
  const std::string path = raw_path && raw_path[0] != '\0' ? raw_path : "/usr/local/bin:/usr/bin:/bin";
  std::size_t start = 0;
  while (start <= path.size()) {
    const auto end = path.find(':', start);
    const auto segment =
        path.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const auto dir = segment.empty() ? std::filesystem::path(".") : std::filesystem::path(segment);
    if (executable_exists(dir / executable)) {
      return true;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return false;
}

std::string binary_not_found_message(const TestCommand& command) {
  return command.executable.empty() ? "benchmark binary not found"
                                    : command.executable + " binary not found";
}

std::string final_json_document(const std::string& text) {
  std::size_t object_start = std::string::npos;
  std::size_t last_start   = std::string::npos;
  std::size_t last_end     = std::string::npos;
  int depth                = 0;
  bool in_string           = false;
  bool escaped             = false;

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        in_string = false;
      }
      continue;
    }

    if (ch == '"') {
      in_string = true;
      continue;
    }
    if (ch == '{') {
      if (depth == 0) {
        object_start = i;
      }
      ++depth;
    } else if (ch == '}' && depth > 0) {
      --depth;
      if (depth == 0 && object_start != std::string::npos) {
        last_start = object_start;
        last_end   = i;
      }
    }
  }

  if (last_start == std::string::npos || last_end == std::string::npos || last_end < last_start) {
    return text;
  }
  return text.substr(last_start, last_end - last_start + 1);
}

bool regex_metric(const std::string& text,
                  const std::regex& pattern,
                  std::string& value,
                  std::size_t index = 1) {
  std::smatch match;
  if (!std::regex_search(text, match, pattern) || index >= match.size()) {
    return false;
  }
  value = match[index].str();
  return true;
}

void add_if_matched(std::map<std::string, std::string>& metrics,
                    const std::string& key,
                    const std::string& text,
                    const std::regex& pattern,
                    const std::string& suffix = {}) {
  std::string value;
  if (regex_metric(text, pattern, value)) {
    metrics[key] = suffix.empty() ? value : value + suffix;
  }
}

void parse_sysbench_stdout(const std::string& output, TestResult& result) {
  add_if_matched(result.metrics,
                 "events_per_second",
                 output,
                 std::regex(R"(events per second:\s*([0-9.]+))"));
  add_if_matched(result.metrics,
                 "total_time",
                 output,
                 std::regex(R"(total time:\s*([0-9.]+)s)"),
                 " s");
  add_if_matched(result.metrics,
                 "total_events",
                 output,
                 std::regex(R"(total number of events:\s*([0-9]+))"));
  add_if_matched(result.metrics, "latency_avg", output, std::regex(R"(avg:\s*([0-9.]+))"), " ms");
  add_if_matched(result.metrics,
                 "latency_95th",
                 output,
                 std::regex(R"(95th percentile:\s*([0-9.]+))"),
                 " ms");

  const auto events = result.metrics.find("events_per_second");
  mark_passed(result, result.process.success() && events != result.metrics.end());
  if (events != result.metrics.end()) {
    result.summary = "CPU: " + events->second + " events/s";
  } else if (result.process.success()) {
    result.summary = "CPU: sysbench completed but no score was parsed";
  }
}

const process::OutputValue* object_child(const process::OutputValue& value,
                                         const std::string& key) {
  if (value.type != process::OutputValue::Type::Object) {
    return nullptr;
  }
  const auto it = value.object_values.find(key);
  return it == value.object_values.end() ? nullptr : &it->second;
}

const process::OutputValue* array_child(const process::OutputValue& value, std::size_t index) {
  if (value.type != process::OutputValue::Type::Array || index >= value.array_values.size()) {
    return nullptr;
  }
  return &value.array_values[index];
}

const process::OutputValue* child_at(const process::OutputValue& value,
                                     std::initializer_list<const char*> keys) {
  auto* current = &value;
  for (const auto* key : keys) {
    if (!current) {
      return nullptr;
    }
    current = object_child(*current, key);
  }
  return current;
}

bool number_at(const process::OutputValue& value,
               std::initializer_list<const char*> keys,
               double& number) {
  const auto* item = child_at(value, keys);
  if (!item || item->type != process::OutputValue::Type::Number) {
    return false;
  }
  number = item->number_value;
  return true;
}

bool string_at(const process::OutputValue& value,
               std::initializer_list<const char*> keys,
               std::string& text) {
  const auto* item = child_at(value, keys);
  if (!item || item->type != process::OutputValue::Type::String) {
    return false;
  }
  text = item->string_value;
  return true;
}

void parse_fio_json(TestResult& result) {
  if (!result.structured_output.success()) {
    return;
  }

  const auto* jobs = object_child(result.structured_output.value, "jobs");
  const auto* job  = jobs ? array_child(*jobs, 0) : nullptr;
  if (!job) {
    mark_passed(result, false);
    result.summary = "SD: fio JSON does not contain a job result";
    return;
  }

  double value = 0.0;
  if (number_at(*job, {"error"}, value) && value != 0.0) {
    mark_passed(result, false);
    result.summary = "SD: fio reported job error " + number_string(value, 0);
    return;
  }
  if (number_at(*job, {"write", "bw_bytes"}, value)) {
    result.metrics["write_bw"] = bytes_per_second_string(value);
  }
  if (number_at(*job, {"write", "iops"}, value)) {
    result.metrics["write_iops"] = number_string(value, 1);
  }
  if (number_at(*job, {"write", "clat_ns", "mean"}, value)) {
    result.metrics["write_clat_avg"] = milliseconds_string_from_ns(value);
  }
  if (number_at(*job, {"write", "clat_ns", "percentile", "95.000000"}, value)) {
    result.metrics["write_clat_95th"] = milliseconds_string_from_ns(value);
  }

  mark_passed(result,
              result.process.success() && result.metrics.find("write_bw") != result.metrics.end());
  const auto write_bw = result.metrics.find("write_bw");
  const auto iops     = result.metrics.find("write_iops");
  if (write_bw != result.metrics.end()) {
    result.summary = "SD: write " + write_bw->second;
    if (iops != result.metrics.end()) {
      result.summary += ", " + iops->second + " IOPS";
    }
  } else {
    result.summary = "SD: fio completed but no write bandwidth was parsed";
  }
}

bool parse_stress_stdout(const std::string& output, TestResult& result) {
  std::string value;
  bool saw_summary = false;
  if (regex_metric(output, std::regex(R"(passed:\s*([0-9]+))"), value)) {
    result.metrics["passed"] = value;
    saw_summary              = true;
  }
  if (regex_metric(output, std::regex(R"(failed:\s*([0-9]+))"), value)) {
    result.metrics["failed"] = value;
    saw_summary              = true;
  }
  if (regex_metric(output, std::regex(R"(metrics untrustworthy:\s*([0-9]+))"), value)) {
    result.metrics["untrustworthy"] = value;
    saw_summary                     = true;
  }

  const auto passed        = result.metrics.find("passed");
  const auto failed        = result.metrics.find("failed");
  const auto untrustworthy = result.metrics.find("untrustworthy");
  const bool reported_ok =
      passed != result.metrics.end() && std::atoi(passed->second.c_str()) > 0 &&
      (failed == result.metrics.end() || std::atoi(failed->second.c_str()) == 0) &&
      (untrustworthy == result.metrics.end() || std::atoi(untrustworthy->second.c_str()) == 0);
  if (reported_ok) {
    result.summary = "Memory: stress-ng passed " + passed->second + " worker(s)";
  }
  return saw_summary && reported_ok;
}

void parse_stress_yaml(TestResult& result) {
  if (!result.structured_output.success()) {
    return;
  }

  std::string stressor;
  double value        = 0.0;
  const auto* metrics = object_child(result.structured_output.value, "metrics");
  const auto* first   = metrics ? array_child(*metrics, 0) : nullptr;
  if (first) {
    if (string_at(*first, {"stressor"}, stressor)) {
      result.metrics["stressor"] = stressor;
    }
    if (number_at(*first, {"bogo-ops"}, value)) {
      result.metrics["bogo_ops"] = number_string(value, 0);
    }
    if (number_at(*first, {"bogo-ops-per-second-real-time"}, value)) {
      result.metrics["bogo_ops_per_sec"] = number_string(value, 2);
    }
    if (number_at(*first, {"wall-clock-time"}, value)) {
      result.metrics["wall_time"] = number_string(value, 2) + " s";
    }
    if (number_at(*first, {"max-rss"}, value)) {
      result.metrics["max_rss"] = number_string(value, 0) + " KB";
    }
  }
  if (number_at(result.structured_output.value, {"system-info", "totalram"}, value)) {
    result.metrics["total_ram"] = number_string(value / 1024.0 / 1024.0, 1) + " MB";
  }
  result.metrics["yaml_output"] = K_STRESS_NG_YAML_PATH;
  const bool stdout_ok =
      parse_stress_stdout(result.process.stdout_text + "\n" + result.process.stderr_text, result);
  mark_passed(result,
              !result.process.timed_out && result.process.error_message.empty() &&
                  (result.process.exit_code == 0 || stdout_ok) && (first != nullptr || stdout_ok));
  if (result.passed && result.summary.empty()) {
    result.summary = "Memory: stress-ng YAML metrics parsed";
  } else if (!result.passed && first == nullptr) {
    result.summary = "Memory: no stress-ng metrics found";
  }
}

void emit_progress_line(const TestCommand& command,
                        const ProgressCallback& progress_callback,
                        const std::string& line,
                        bool stderr_line,
                        std::chrono::steady_clock::time_point started_at) {
  if (!progress_callback) {
    return;
  }

  auto elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                                      std::chrono::steady_clock::now() - started_at)
                                      .count());
  std::string elapsed_text;
  if (regex_metric(line, std::regex(R"(^\[\s*([0-9]+)s\s*\])"), elapsed_text)) {
    elapsed = std::max(elapsed, std::atoi(elapsed_text.c_str()));
  }
  int percent = command.duration_seconds > 0
                    ? std::min(99, std::max(0, elapsed * 100 / command.duration_seconds))
                    : 0;
  std::string percent_text;
  if (regex_metric(line, std::regex(R"(([0-9]{1,3}(?:\.[0-9]+)?)\s*%)"), percent_text)) {
    percent = std::min(99, std::max(0, static_cast<int>(std::atof(percent_text.c_str()))));
  }
  progress_callback({command.kind, trim(line), stderr_line, elapsed, percent});
}

TestResult run_command_and_collect(const TestCommand& command,
                                   const ProgressCallback& progress_callback) {
  TestResult result;
  result.kind         = command.kind;
  result.name         = command.name;
  result.command_line = command_to_string(command);

  if (!executable_on_path(command.executable)) {
    result.status  = TestStatus::WARNING;
    result.passed  = false;
    result.summary = binary_not_found_message(command);
    result.process.error_message = result.summary;
    if (progress_callback) {
      progress_callback({command.kind, {}, false, 0, 100});
    }
    return result;
  }

  process::ProcessOptions options;
  options.timeout_ms       = command.timeout_ms;
  options.max_output_bytes = 512U * 1024U;

  const auto started_at       = std::chrono::steady_clock::now();
  options.stdout_line_handler = [&](const std::string& line) {
    if (!trim(line).empty()) {
      emit_progress_line(command, progress_callback, line, false, started_at);
    }
  };
  options.stderr_line_handler = [&](const std::string& line) {
    if (!trim(line).empty()) {
      emit_progress_line(command, progress_callback, line, true, started_at);
    }
  };

  LOG_INFO("running performance test command: {}", result.command_line);
  result.process = process::run_command(command.executable, command.args, options);
  if (progress_callback) {
    progress_callback({command.kind, {}, false, command.duration_seconds, 100});
  }
  mark_passed(result, result.process.success());
  if (!result.passed) {
    result.summary =
        result.process.error_message.empty() ? "command failed" : result.process.error_message;
  }
  return result;
}

}  // namespace

TestCommand make_cpu_sysbench_command() {
  return {
      TestKind::CPU,
      "CPU sysbench",
      "sysbench",
      {
          "cpu",
          "--threads=4",
          "--time=10",
          "--cpu-max-prime=20000",
          "--report-interval=1",
          "run",
      },
      {},
      {},
      (K_CPU_DURATION_SECONDS + 5) * 1000,
      K_CPU_DURATION_SECONDS,
  };
}

TestCommand make_memory_stress_ng_command() {
  return {
      TestKind::MEMORY,
      "Memory stress-ng",
      "stress-ng",
      {
          "--vm",
          "2",
          "--vm-bytes",
          "256M",
          "--vm-keep",
          "--vm-populate",
          "--memcpy",
          "2",
          "--timeout",
          "30s",
          "--progress",
          "--metrics-brief",
          "--yaml",
          K_STRESS_NG_YAML_PATH,
      },
      K_STRESS_NG_YAML_PATH,
      {},
      (K_MEMORY_DURATION_SECONDS + 30) * 1000,
      K_MEMORY_DURATION_SECONDS,
  };
}

TestCommand make_sd_card_fio_command() {
  return {
      TestKind::SD_CARD,
      "SD card fio",
      "fio",
      {
          "--name=sd_randwrite",
          "--ioengine=libaio",
          "--iodepth=4",
          "--rw=randwrite",
          "--bs=4k",
          "--direct=1",
          "--size=64M",
          "--runtime=20",
          "--time_based",
          std::string("--filename=") + K_FIO_DATA_PATH,
          "--status-interval=1",
          "--output-format=json",
      },
      {},
      K_FIO_DATA_PATH,
      (K_SD_DURATION_SECONDS + 10) * 1000,
      K_SD_DURATION_SECONDS,
  };
}

std::string command_to_string(const TestCommand& command) {
  std::string text = quote_arg(command.executable);
  for (const auto& arg : command.args) {
    text += " ";
    text += quote_arg(arg);
  }
  return text;
}

TestResult run_cpu_sysbench(ProgressCallback progress_callback) {
  auto result = run_command_and_collect(make_cpu_sysbench_command(), progress_callback);
  if (result.status == TestStatus::WARNING) {
    return result;
  }
  parse_sysbench_stdout(result.process.stdout_text, result);
  return result;
}

TestResult run_memory_stress_ng(ProgressCallback progress_callback) {
  auto result = run_command_and_collect(make_memory_stress_ng_command(), progress_callback);
  if (result.status == TestStatus::WARNING) {
    return result;
  }
  const bool stdout_ok =
      parse_stress_stdout(result.process.stdout_text + "\n" + result.process.stderr_text, result);
  if (result.process.success()) {
    result.structured_output = process::parse_yaml_file(K_STRESS_NG_YAML_PATH);
    const bool yaml_ok       = result.structured_output.success();
    if (yaml_ok) {
      parse_stress_yaml(result);
    } else {
      mark_passed(result,
                  stdout_ok && !result.process.timed_out && result.process.error_message.empty());
      if (!result.passed) {
        result.summary = "Memory: failed to parse stress-ng YAML output";
      }
    }
  } else if (stdout_ok && !result.process.timed_out && result.process.error_message.empty()) {
    mark_passed(result, true);
  }
  return result;
}

TestResult run_sd_card_fio(ProgressCallback progress_callback) {
  auto result = run_command_and_collect(make_sd_card_fio_command(), progress_callback);
  if (result.status == TestStatus::WARNING) {
    return result;
  }
  if (result.process.success()) {
    result.structured_output =
        process::parse_json_output(final_json_document(result.process.stdout_text));
    mark_passed(result, result.structured_output.success());
    if (result.passed) {
      parse_fio_json(result);
    } else {
      result.summary = "SD: failed to parse fio JSON stdout";
    }
  }
  if (std::remove(K_FIO_DATA_PATH) != 0) {
    LOG_TRACE("fio data file cleanup skipped or failed: {}", K_FIO_DATA_PATH);
  }
  return result;
}

}  // namespace platform::perf
