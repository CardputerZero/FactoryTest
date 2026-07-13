/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "test_session.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "logger.h"

#if APP_USE_LIBCJSON
#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#else
#include <cJSON.h>
#endif
#endif

namespace model {
namespace {

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

#if APP_USE_LIBCJSON
struct JsonOwner {
  cJSON* value{nullptr};

  explicit JsonOwner(cJSON* item = nullptr)
      : value(item) {}
  JsonOwner(const JsonOwner&)            = delete;
  JsonOwner& operator=(const JsonOwner&) = delete;
  JsonOwner(JsonOwner&& other) noexcept
      : value(other.value) {
    other.value = nullptr;
  }
  JsonOwner& operator=(JsonOwner&& other) noexcept {
    if (this != &other) {
      cJSON_Delete(value);
      value       = other.value;
      other.value = nullptr;
    }
    return *this;
  }
  ~JsonOwner() { cJSON_Delete(value); }

  cJSON* get() const { return value; }
};

struct JsonStringOwner {
  char* value{nullptr};

  explicit JsonStringOwner(char* text = nullptr)
      : value(text) {}
  JsonStringOwner(const JsonStringOwner&)            = delete;
  JsonStringOwner& operator=(const JsonStringOwner&) = delete;
  ~JsonStringOwner() { cJSON_free(value); }

  char* get() const { return value; }
};

JsonOwner make_session_document(const std::string& session_id, const std::string& test_time) {
  JsonOwner root(cJSON_CreateObject());
  if (!root.get()) {
    return JsonOwner();
  }

  if (!cJSON_AddStringToObject(root.get(), "test_session", session_id.c_str()) ||
      !cJSON_AddStringToObject(root.get(), "test_time", test_time.c_str()) ||
      !cJSON_AddArrayToObject(root.get(), "results")) {
    return JsonOwner();
  }
  return root;
}

JsonOwner read_json_file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return JsonOwner();
  }

  const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (text.empty()) {
    return JsonOwner();
  }
  return JsonOwner(cJSON_ParseWithLength(text.data(), text.size()));
}

cJSON* ensure_results_array(cJSON* root) {
  cJSON* results = cJSON_GetObjectItemCaseSensitive(root, "results");
  if (cJSON_IsArray(results)) {
    return results;
  }
  if (results) {
    cJSON_DeleteItemFromObjectCaseSensitive(root, "results");
  }
  return cJSON_AddArrayToObject(root, "results");
}

bool replace_string(cJSON* root, const char* name, const std::string& value) {
  cJSON* item = cJSON_CreateString(value.c_str());
  if (!item) {
    return false;
  }
  if (cJSON_GetObjectItemCaseSensitive(root, name)) {
    const bool replaced = cJSON_ReplaceItemInObjectCaseSensitive(root, name, item) != 0;
    if (!replaced) {
      cJSON_Delete(item);
    }
    return replaced;
  }
  const bool added = cJSON_AddItemToObject(root, name, item) != 0;
  if (!added) {
    cJSON_Delete(item);
  }
  return added;
}

bool write_json_file(const std::string& path, const cJSON* root) {
  JsonStringOwner text(cJSON_Print(root));
  if (!text.get()) {
    return false;
  }

  std::ofstream output(path, std::ios::trunc | std::ios::binary);
  if (!output) {
    return false;
  }

  output << text.get() << '\n';
  return static_cast<bool>(output);
}
#endif

const char* result_text(TestResult result) {
  switch (result) {
    case TestResult::PASS:
      return "pass";
    case TestResult::SKIPPED:
      return "skipped";
    case TestResult::FAILED:
    default:
      return "failed";
  }
}

}  // namespace

void TestSession::start() {
  const auto now = std::chrono::system_clock::now();
  session_id_    = "factorytest-" + session_timestamp(now);
  test_time_     = iso_timestamp(now);
  output_path_   = default_output_path().string();
  active_        = true;

#if APP_USE_LIBCJSON
  auto root = make_session_document(session_id_, test_time_);
  if (!root.get() || !write_json_file(output_path_, root.get())) {
    LOG_WARN("failed to create test result file: {}", output_path_);
    return;
  }
#else
  std::ofstream stream(output_path_, std::ios::trunc);
  if (!stream) {
    LOG_WARN("failed to create test result file: {}", output_path_);
    return;
  }
  LOG_WARN("test result JSON output requires cJSON");
#endif
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

#if APP_USE_LIBCJSON
  auto root = read_json_file(output_path_);
  if (!cJSON_IsObject(root.get())) {
    LOG_WARN("test result document is incomplete or corrupted; rebuilding: {}", output_path_);
    root = make_session_document(session_id_, test_time_);
  } else if (!replace_string(root.get(), "test_session", session_id_) ||
             !replace_string(root.get(), "test_time", test_time_)) {
    LOG_WARN("failed to write test result file: {}", output_path_);
    return;
  }

  if (!root.get()) {
    LOG_WARN("failed to write test result file: {}", output_path_);
    return;
  }

  cJSON* results = ensure_results_array(root.get());
  cJSON* entry   = cJSON_CreateObject();
  if (!results || !entry ||
      !cJSON_AddStringToObject(entry, "test_name", test_name ? test_name : "Unknown Test") ||
      !cJSON_AddStringToObject(entry, "test_result", result_text(result))) {
    cJSON_Delete(entry);
    LOG_WARN("failed to build test result JSON");
    return;
  }
  if (!cJSON_AddItemToArray(results, entry)) {
    cJSON_Delete(entry);
    LOG_WARN("failed to append test result JSON");
    return;
  }

  if (!write_json_file(output_path_, root.get())) {
    LOG_WARN("failed to write test result file: {}", output_path_);
    return;
  }
#else
  LOG_WARN("failed to record test result: cJSON library is not available");
#endif
  LOG_INFO("test result recorded: {} {}",
           test_name ? test_name : "Unknown Test",
           result_text(result));
}

}  // namespace model
