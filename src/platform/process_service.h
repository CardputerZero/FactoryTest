/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace platform::process {

using OutputHandler     = std::function<void(const char* data, std::size_t size)>;
using OutputLineHandler = std::function<void(const std::string& line)>;

struct ProcessOptions {
  // Non-zero values kill the child process if it runs longer than the limit.
  int timeout_ms{0};
  std::size_t max_output_bytes{1024U * 1024U};
  OutputHandler stdout_handler{};
  OutputHandler stderr_handler{};
  OutputLineHandler stdout_line_handler{};
  OutputLineHandler stderr_line_handler{};
};

struct ProcessResult {
  int exit_code{-1};
  int term_signal{0};
  bool exited{false};
  bool timed_out{false};
  bool output_truncated{false};
  std::string stdout_text{};
  std::string stderr_text{};
  std::string error_message{};

  bool success() const { return exited && exit_code == 0 && !timed_out && error_message.empty(); }
};

struct OutputValue {
  enum class Type { Null, Boolean, Number, String, Array, Object };

  Type type{Type::Null};
  bool bool_value{false};
  double number_value{0.0};
  std::string string_value{};
  std::vector<OutputValue> array_values{};
  std::map<std::string, OutputValue> object_values{};

  static OutputValue null();
  static OutputValue boolean(bool value);
  static OutputValue number(double value);
  static OutputValue string(std::string value);
  static OutputValue array(std::vector<OutputValue> value);
  static OutputValue object(std::map<std::string, OutputValue> value);
};

struct ParseResult {
  OutputValue value{};
  std::string error_message{};

  bool success() const { return error_message.empty(); }
};

ProcessResult run_command(const std::string& executable,
                          const std::vector<std::string>& args = {},
                          const ProcessOptions& options        = {});
ProcessResult run_shell(const std::string& command, const ProcessOptions& options = {});

ParseResult parse_json_output(const std::string& text);
ParseResult parse_yaml_output(const std::string& text);
ParseResult parse_json_file(const std::string& path);
ParseResult parse_yaml_file(const std::string& path);

std::string output_value_to_string(const OutputValue& value);

}  // namespace platform::process
