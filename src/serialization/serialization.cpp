/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "serialization.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#if APP_USE_LIBCJSON
#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#else
#include <cJSON.h>
#endif
#endif

#if APP_USE_LIBYAML
#include <yaml.h>
#endif

namespace platform::serialization {
namespace {

std::string trim(std::string value) {
  const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

std::string read_file(const std::string& path, std::string& error_message) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error_message = "failed to open file: " + path;
    return {};
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

bool parse_number(const std::string& text, double& value) {
  if (text.empty()) {
    return false;
  }

  char* end = nullptr;
  errno     = 0;
  value     = std::strtod(text.c_str(), &end);
  return errno == 0 && end && *end == '\0' && std::isfinite(value);
}

OutputValue scalar_from_text(const std::string& value) {
  const auto text = trim(value);
  if (text == "null" || text == "Null" || text == "NULL" || text == "~") {
    return OutputValue::null();
  }
  if (text == "true" || text == "True" || text == "TRUE") {
    return OutputValue::boolean(true);
  }
  if (text == "false" || text == "False" || text == "FALSE") {
    return OutputValue::boolean(false);
  }

  double number = 0.0;
  if (parse_number(text, number)) {
    return OutputValue::number(number);
  }
  return OutputValue::string(value);
}

std::string escape_string(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const auto ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

#if APP_USE_LIBCJSON
OutputValue output_value_from_json(const cJSON* item) {
  if (!item || cJSON_IsNull(item)) {
    return OutputValue::null();
  }
  if (cJSON_IsBool(item)) {
    return OutputValue::boolean(cJSON_IsTrue(item));
  }
  if (cJSON_IsNumber(item)) {
    return OutputValue::number(item->valuedouble);
  }
  if (cJSON_IsString(item)) {
    return OutputValue::string(item->valuestring ? item->valuestring : "");
  }
  if (cJSON_IsArray(item)) {
    std::vector<OutputValue> values;
    const auto size = cJSON_GetArraySize(item);
    values.reserve(static_cast<std::size_t>(std::max(size, 0)));
    cJSON* child = nullptr;
    cJSON_ArrayForEach(child, item) { values.push_back(output_value_from_json(child)); }
    return OutputValue::array(std::move(values));
  }
  if (cJSON_IsObject(item)) {
    std::map<std::string, OutputValue> values;
    cJSON* child = nullptr;
    cJSON_ArrayForEach(child, item) {
      values.emplace(child->string ? child->string : "", output_value_from_json(child));
    }
    return OutputValue::object(std::move(values));
  }
  return OutputValue::null();
}
#endif

#if APP_USE_LIBYAML
struct YamlEventOwner {
  yaml_event_t event{};
  bool initialized{false};

  ~YamlEventOwner() {
    if (initialized) {
      yaml_event_delete(&event);
    }
  }
};

bool next_yaml_event(yaml_parser_t& parser, YamlEventOwner& owner, std::string& error_message) {
  if (!yaml_parser_parse(&parser, &owner.event)) {
    error_message = parser.problem ? parser.problem : "failed to parse YAML";
    return false;
  }
  owner.initialized = true;
  return true;
}

OutputValue parse_yaml_node(yaml_parser_t& parser, std::string& error_message);

OutputValue parse_yaml_mapping(yaml_parser_t& parser, std::string& error_message) {
  std::map<std::string, OutputValue> values;
  while (error_message.empty()) {
    YamlEventOwner key_event;
    if (!next_yaml_event(parser, key_event, error_message)) {
      return OutputValue::null();
    }
    if (key_event.event.type == YAML_MAPPING_END_EVENT) {
      return OutputValue::object(std::move(values));
    }
    if (key_event.event.type != YAML_SCALAR_EVENT) {
      error_message = "YAML mapping key must be a scalar";
      return OutputValue::null();
    }

    auto* raw_value       = key_event.event.data.scalar.value;
    const auto raw_length = key_event.event.data.scalar.length;
    std::string key(reinterpret_cast<const char*>(raw_value), raw_length);
    auto value = parse_yaml_node(parser, error_message);
    if (!error_message.empty()) {
      return OutputValue::null();
    }
    values.emplace(std::move(key), std::move(value));
  }
  return OutputValue::null();
}

OutputValue parse_yaml_sequence(yaml_parser_t& parser, std::string& error_message) {
  std::vector<OutputValue> values;
  while (error_message.empty()) {
    YamlEventOwner event;
    if (!next_yaml_event(parser, event, error_message)) {
      return OutputValue::null();
    }
    if (event.event.type == YAML_SEQUENCE_END_EVENT) {
      return OutputValue::array(std::move(values));
    }

    switch (event.event.type) {
      case YAML_SCALAR_EVENT: {
        auto* raw_value       = event.event.data.scalar.value;
        const auto raw_length = event.event.data.scalar.length;
        values.push_back(
            scalar_from_text(std::string(reinterpret_cast<const char*>(raw_value), raw_length)));
        break;
      }
      case YAML_SEQUENCE_START_EVENT:
        values.push_back(parse_yaml_sequence(parser, error_message));
        break;
      case YAML_MAPPING_START_EVENT:
        values.push_back(parse_yaml_mapping(parser, error_message));
        break;
      default:
        error_message = "unsupported YAML sequence node";
        return OutputValue::null();
    }
  }
  return OutputValue::null();
}

OutputValue parse_yaml_node(yaml_parser_t& parser, std::string& error_message) {
  YamlEventOwner event;
  if (!next_yaml_event(parser, event, error_message)) {
    return OutputValue::null();
  }

  switch (event.event.type) {
    case YAML_SCALAR_EVENT: {
      auto* raw_value       = event.event.data.scalar.value;
      const auto raw_length = event.event.data.scalar.length;
      return scalar_from_text(std::string(reinterpret_cast<const char*>(raw_value), raw_length));
    }
    case YAML_SEQUENCE_START_EVENT:
      return parse_yaml_sequence(parser, error_message);
    case YAML_MAPPING_START_EVENT:
      return parse_yaml_mapping(parser, error_message);
    case YAML_DOCUMENT_END_EVENT:
      return OutputValue::null();
    default:
      error_message = "unsupported YAML node";
      return OutputValue::null();
  }
}
#endif

}  // namespace

OutputValue OutputValue::null() { return {}; }

OutputValue OutputValue::boolean(bool value) {
  OutputValue result;
  result.type       = Type::Boolean;
  result.bool_value = value;
  return result;
}

OutputValue OutputValue::number(double value) {
  OutputValue result;
  result.type         = Type::Number;
  result.number_value = value;
  return result;
}

OutputValue OutputValue::string(std::string value) {
  OutputValue result;
  result.type         = Type::String;
  result.string_value = std::move(value);
  return result;
}

OutputValue OutputValue::array(std::vector<OutputValue> value) {
  OutputValue result;
  result.type         = Type::Array;
  result.array_values = std::move(value);
  return result;
}

OutputValue OutputValue::object(std::map<std::string, OutputValue> value) {
  OutputValue result;
  result.type          = Type::Object;
  result.object_values = std::move(value);
  return result;
}

ParseResult parse_json_output(const std::string& text) {
  ParseResult result;
#if APP_USE_LIBCJSON
  cJSON* root = cJSON_ParseWithLength(text.data(), text.size());
  if (!root) {
    const char* error = cJSON_GetErrorPtr();
    result.error_message =
        error ? std::string("JSON parse error near: ") + error : "failed to parse JSON";
    return result;
  }

  result.value = output_value_from_json(root);
  cJSON_Delete(root);
  return result;
#else
  (void)text;
  result.error_message = "cJSON library is not available";
  return result;
#endif
}

ParseResult parse_yaml_output(const std::string& text) {
  ParseResult result;
#if APP_USE_LIBYAML
  yaml_parser_t parser{};
  if (!yaml_parser_initialize(&parser)) {
    result.error_message = "failed to initialize YAML parser";
    return result;
  }

  yaml_parser_set_input_string(&parser,
                               reinterpret_cast<const unsigned char*>(text.data()),
                               text.size());

  YamlEventOwner stream_start;
  if (!next_yaml_event(parser, stream_start, result.error_message) ||
      stream_start.event.type != YAML_STREAM_START_EVENT) {
    yaml_parser_delete(&parser);
    if (result.error_message.empty()) {
      result.error_message = "invalid YAML stream";
    }
    return result;
  }

  YamlEventOwner document_start;
  if (!next_yaml_event(parser, document_start, result.error_message) ||
      document_start.event.type != YAML_DOCUMENT_START_EVENT) {
    yaml_parser_delete(&parser);
    if (result.error_message.empty()) {
      result.error_message = "invalid YAML document";
    }
    return result;
  }

  result.value = parse_yaml_node(parser, result.error_message);
  yaml_parser_delete(&parser);
  return result;
#else
  (void)text;
  result.error_message = "libyaml is not available";
  return result;
#endif
}

ParseResult parse_json_file(const std::string& path) {
  std::string error_message;
  auto content = read_file(path, error_message);
  if (!error_message.empty()) {
    ParseResult result;
    result.error_message = std::move(error_message);
    return result;
  }
  return parse_json_output(content);
}

ParseResult parse_yaml_file(const std::string& path) {
  std::string error_message;
  auto content = read_file(path, error_message);
  if (!error_message.empty()) {
    ParseResult result;
    result.error_message = std::move(error_message);
    return result;
  }
  return parse_yaml_output(content);
}

std::string output_value_to_string(const OutputValue& value) {
  switch (value.type) {
    case OutputValue::Type::Null:
      return "null";
    case OutputValue::Type::Boolean:
      return value.bool_value ? "true" : "false";
    case OutputValue::Type::Number: {
      std::ostringstream out;
      out << value.number_value;
      return out.str();
    }
    case OutputValue::Type::String:
      return "\"" + escape_string(value.string_value) + "\"";
    case OutputValue::Type::Array: {
      std::string out = "[";
      bool first      = true;
      for (const auto& item : value.array_values) {
        if (!first) {
          out += ", ";
        }
        first = false;
        out += output_value_to_string(item);
      }
      out += "]";
      return out;
    }
    case OutputValue::Type::Object: {
      std::string out = "{";
      bool first      = true;
      for (const auto& item : value.object_values) {
        if (!first) {
          out += ", ";
        }
        first = false;
        out += "\"" + escape_string(item.first) + "\": " + output_value_to_string(item.second);
      }
      out += "}";
      return out;
    }
  }
  return "null";
}

}  // namespace platform::serialization
