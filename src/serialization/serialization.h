/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <map>
#include <string>
#include <vector>

namespace platform::serialization {

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

ParseResult parse_json_output(const std::string& text);
ParseResult parse_yaml_output(const std::string& text);
ParseResult parse_json_file(const std::string& path);
ParseResult parse_yaml_file(const std::string& path);

std::string output_value_to_string(const OutputValue& value);

}  // namespace platform::serialization
