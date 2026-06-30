/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "test_session.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "logger.h"

namespace model {
namespace {

std::string json_escape(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.size() + 8);
  for (unsigned char ch : text) {
    switch (ch) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
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
        if (ch < 0x20) {
          char buffer[7]{};
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(ch));
          escaped += buffer;
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  return escaped;
}

std::tm local_time(std::time_t value) {
  std::tm result{};
#if defined(_WIN32)
  localtime_s(&result, &value);
#else
  localtime_r(&value, &result);
#endif
  return result;
}

std::string session_timestamp(const std::chrono::system_clock::time_point& now) {
  const auto time_t = std::chrono::system_clock::to_time_t(now);
  const auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  const auto tm = local_time(time_t);

  char buffer[32]{};
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d%02d%02d-%02d%02d%02d-%03d",
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec,
                static_cast<int>(ms.count()));
  return buffer;
}

std::string iso_timestamp(const std::chrono::system_clock::time_point& now) {
  const auto time_t = std::chrono::system_clock::to_time_t(now);
  const auto tm     = local_time(time_t);

  char buffer[32]{};
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d-%02d-%02dT%02d:%02d:%02d",
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec);
  return buffer;
}

std::filesystem::path default_output_path() {
  if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
    return std::filesystem::path(home) / "cardputer_factory_test.json";
  }
  return std::filesystem::path("/tmp") / "cardputer_factory_test.json";
}

const char* result_text(TestResult result) {
  switch (result) {
    case TestResult::PASS:
      return "pass";
    case TestResult::SKIPPED:
      return "skipped";
    case TestResult::FAILED:
    default:
      return "Failed";
  }
}

}  // namespace

void TestSession::start() {
  const auto now = std::chrono::system_clock::now();
  session_id_    = "factorytest-" + session_timestamp(now);
  test_time_     = iso_timestamp(now);
  output_path_   = default_output_path().string();
  active_        = true;

  std::ofstream stream(output_path_, std::ios::trunc);
  if (!stream) {
    LOG_WARN("failed to create test result file: {}", output_path_);
    return;
  }

  stream << "{\n"
         << "  \"test_session\": \"" << json_escape(session_id_) << "\",\n"
         << "  \"test_time\": \"" << json_escape(test_time_) << "\",\n"
         << "  \"results\": []\n"
         << "}\n";
  LOG_INFO("test session started: {} path={}", session_id_, output_path_);
}

bool TestSession::active() const { return active_; }

const std::string& TestSession::session_id() const { return session_id_; }

const std::string& TestSession::test_time() const { return test_time_; }

const std::string& TestSession::output_path() const { return output_path_; }

void TestSession::append_result(const char* test_name, TestResult result) {
  if (!active_) {
    start();
  }

  struct ResultEntry {
    std::string name;
    std::string result;
  };

  std::vector<ResultEntry> entries;
  std::ifstream input(output_path_);
  if (input) {
    std::stringstream buffer;
    buffer << input.rdbuf();
    const auto text = buffer.str();
    std::size_t pos = 0;
    while (true) {
      const auto name_key = text.find("\"test_name\"", pos);
      if (name_key == std::string::npos) {
        break;
      }
      const auto name_colon = text.find(':', name_key);
      const auto name_begin = text.find('"', name_colon + 1);
      const auto name_end   = name_begin == std::string::npos ? std::string::npos
                                                               : text.find('"', name_begin + 1);
      const auto result_key = text.find("\"test_result\"", name_end);
      const auto result_colon = text.find(':', result_key);
      const auto result_begin = text.find('"', result_colon + 1);
      const auto result_end   = result_begin == std::string::npos
                                    ? std::string::npos
                                    : text.find('"', result_begin + 1);
      if (name_begin == std::string::npos || name_end == std::string::npos ||
          result_key == std::string::npos || result_begin == std::string::npos ||
          result_end == std::string::npos) {
        break;
      }
      entries.push_back({text.substr(name_begin + 1, name_end - name_begin - 1),
                         text.substr(result_begin + 1, result_end - result_begin - 1)});
      pos = result_end + 1;
    }
  }

  entries.push_back({test_name ? test_name : "Unknown Test", result_text(result)});

  std::ofstream output(output_path_, std::ios::trunc);
  if (!output) {
    LOG_WARN("failed to write test result file: {}", output_path_);
    return;
  }

  output << "{\n"
         << "  \"test_session\": \"" << json_escape(session_id_) << "\",\n"
         << "  \"test_time\": \"" << json_escape(test_time_) << "\",\n"
         << "  \"results\": [\n";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    output << "    {\n"
           << "      \"test_name\": \"" << json_escape(entries[i].name) << "\",\n"
           << "      \"test_result\": \"" << json_escape(entries[i].result) << "\"\n"
           << "    }" << (i + 1 < entries.size() ? "," : "") << "\n";
  }
  output << "  ]\n"
         << "}\n";
  LOG_INFO("test result recorded: {} {}", test_name ? test_name : "Unknown Test", result_text(result));
}

}  // namespace model
