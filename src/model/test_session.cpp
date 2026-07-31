/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "test_session.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

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

constexpr int K_SESSION_SCHEMA_VERSION = 2;
constexpr const char* K_SEQUENCE_ID    = "cp0-full-v1";
constexpr const char* K_SESSION_SUFFIX = ".session.json";
constexpr const char* K_RESULT_SUFFIX  = ".result.json";

std::tm utc_time(std::time_t value) {
  std::tm result{};
#if defined(_WIN32)
  gmtime_s(&result, &value);
#else
  gmtime_r(&value, &result);
#endif
  return result;
}

std::string iso_timestamp(const std::chrono::system_clock::time_point& now) {
  const auto time_value = std::chrono::system_clock::to_time_t(now);
  const auto tm         = utc_time(time_value);

  char buffer[32]{};
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d-%02d-%02dT%02d:%02d:%02dZ",
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec);
  return buffer;
}

std::string session_timestamp(const std::chrono::system_clock::time_point& now) {
  const auto time_value = std::chrono::system_clock::to_time_t(now);
  const auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  const auto tm = utc_time(time_value);

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
                static_cast<int>(millis.count()));
  return buffer;
}

std::string now_iso_timestamp() { return iso_timestamp(std::chrono::system_clock::now()); }

std::uint64_t epoch_milliseconds(const std::chrono::system_clock::time_point& now) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

std::filesystem::path default_session_directory() {
  if (const char* configured = std::getenv("FACTORY_TEST_SESSION_DIR");
      configured && configured[0] != '\0') {
    return configured;
  }
  if (const char* state_home = std::getenv("XDG_STATE_HOME"); state_home && state_home[0] != '\0') {
    return std::filesystem::path(state_home) / "factory-test" / "sessions";
  }
  if (const char* user_home = std::getenv("HOME"); user_home && user_home[0] != '\0') {
    return std::filesystem::path(user_home) / ".local" / "state" / "factory-test" / "sessions";
  }
  return std::filesystem::path("/tmp") / "factory-test" / "sessions";
}

bool has_suffix(const std::string& value, const char* suffix) {
  const std::string suffix_text = suffix ? suffix : "";
  return value.size() >= suffix_text.size() &&
         value.compare(value.size() - suffix_text.size(), suffix_text.size(), suffix_text) == 0;
}

std::filesystem::path result_path_for_session(const std::filesystem::path& session_path) {
  auto filename = session_path.filename().string();
  if (has_suffix(filename, K_SESSION_SUFFIX)) {
    filename.erase(filename.size() - std::string(K_SESSION_SUFFIX).size());
  }
  return session_path.parent_path() / (filename + K_RESULT_SUFFIX);
}

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool sync_file(const std::filesystem::path& path) {
#if defined(__unix__) || defined(__APPLE__)
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return false;
  }
  const bool synced = ::fsync(fd) == 0;
  ::close(fd);
  return synced;
#else
  (void)path;
  return true;
#endif
}

bool write_text_file_atomic(const std::filesystem::path& path,
                            const std::string& text,
                            std::string& error_message) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error_message = "failed to create session directory: " + ec.message();
    return false;
  }

  auto temporary = path;
  temporary += ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
      error_message = "failed to open temporary session file";
      return false;
    }
    output << text;
    output.flush();
    if (!output) {
      error_message = "failed to write temporary session file";
      return false;
    }
  }

  if (!sync_file(temporary)) {
    error_message = "failed to sync temporary session file";
    return false;
  }

  std::filesystem::rename(temporary, path, ec);
#if defined(_WIN32)
  if (ec) {
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
  }
#endif
  if (ec) {
    error_message = "failed to replace session file: " + ec.message();
    return false;
  }
  sync_file(path.parent_path());
  return true;
}

std::string nonempty_or(std::string value, const char* fallback) {
  return value.empty() ? std::string(fallback ? fallback : "UNKNOWN") : std::move(value);
}

std::string short_commit(std::string value) {
  value = nonempty_or(std::move(value), "UNKNOWN");
  if (value.size() > 8) {
    value.resize(8);
  }
  return value;
}

SessionState session_state_from_text(const char* value) {
  if (!value) {
    return SessionState::NONE;
  }
  if (std::string(value) == "in_progress") {
    return SessionState::IN_PROGRESS;
  }
  if (std::string(value) == "completed") {
    return SessionState::COMPLETED;
  }
  if (std::string(value) == "abandoned") {
    return SessionState::ABANDONED;
  }
  return SessionState::NONE;
}

TestResult test_result_from_text(const char* value) {
  return value && std::string(value) == "PASS" ? TestResult::PASS : TestResult::FAIL;
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

const char* json_string(const cJSON* object, const char* key) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(item) && item->valuestring ? item->valuestring : nullptr;
}

bool json_bool(const cJSON* object, const char* key, bool fallback = false) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}

std::size_t json_size(const cJSON* object, const char* key, std::size_t fallback = 0) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsNumber(item) && item->valuedouble >= 0.0
             ? static_cast<std::size_t>(item->valuedouble)
             : fallback;
}

bool add_evidence_value(cJSON* object, const std::string& key, const EvidenceValue& value) {
  switch (value.type) {
    case EvidenceValue::Type::BOOLEAN:
      return cJSON_AddBoolToObject(object, key.c_str(), value.bool_value) != nullptr;
    case EvidenceValue::Type::NUMBER:
      return cJSON_AddNumberToObject(object, key.c_str(), value.number_value) != nullptr;
    case EvidenceValue::Type::STRING:
    default:
      return cJSON_AddStringToObject(object, key.c_str(), value.string_value.c_str()) != nullptr;
  }
}

bool add_evidence_object(cJSON* parent,
                         const char* key,
                         const std::map<std::string, EvidenceValue>& evidence) {
  cJSON* object = cJSON_AddObjectToObject(parent, key);
  if (!object) {
    return false;
  }
  for (const auto& entry : evidence) {
    if (!add_evidence_value(object, entry.first, entry.second)) {
      return false;
    }
  }
  return true;
}

bool append_details(cJSON* parent, const std::vector<NamedTestResult>& details) {
  cJSON* array = cJSON_AddArrayToObject(parent, "details");
  if (!array) {
    return false;
  }
  for (const auto& detail : details) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !cJSON_AddStringToObject(item, "name", detail.test_name.c_str()) ||
        !cJSON_AddStringToObject(item, "result", test_result_text(detail.result)) ||
        !cJSON_AddItemToArray(array, item)) {
      cJSON_Delete(item);
      return false;
    }
  }
  return true;
}

std::string print_json(cJSON* root, bool formatted) {
  JsonStringOwner output(formatted ? cJSON_Print(root) : cJSON_PrintUnformatted(root));
  return output.get() ? std::string(output.get()) + (formatted ? "\n" : "") : std::string{};
}

bool parse_evidence(const cJSON* item, std::map<std::string, EvidenceValue>& evidence) {
  evidence.clear();
  if (!item) {
    return true;
  }
  if (!cJSON_IsObject(item)) {
    return false;
  }
  const cJSON* child = nullptr;
  cJSON_ArrayForEach(child, item) {
    const std::string key = child->string ? child->string : "";
    if (key.empty()) {
      return false;
    }
    if (cJSON_IsBool(child)) {
      evidence.emplace(key, EvidenceValue::boolean(cJSON_IsTrue(child)));
    } else if (cJSON_IsNumber(child) && std::isfinite(child->valuedouble)) {
      evidence.emplace(key, EvidenceValue::number(child->valuedouble));
    } else if (cJSON_IsString(child) && child->valuestring) {
      evidence.emplace(key, EvidenceValue::string(child->valuestring));
    } else {
      return false;
    }
  }
  return true;
}

bool parse_details(const cJSON* item, std::vector<NamedTestResult>& details) {
  details.clear();
  if (!item) {
    return true;
  }
  if (!cJSON_IsArray(item)) {
    return false;
  }
  const cJSON* child = nullptr;
  cJSON_ArrayForEach(child, item) {
    const char* name   = json_string(child, "name");
    const char* result = json_string(child, "result");
    if (!cJSON_IsObject(child) || !name || !result) {
      return false;
    }
    details.push_back({name, test_result_from_text(result)});
  }
  return true;
}

struct LoadedSession {
  SessionState state{SessionState::NONE};
  std::string id;
  std::uint64_t sequence_number{0};
  std::string started_at;
  std::string updated_at;
  std::string completed_at;
  std::string current_test_id;
  std::string result_path;
  SessionMetadata metadata;
  std::vector<TestRecord> tests;
};

bool parse_session_document(const std::string& text,
                            LoadedSession& loaded,
                            std::string& error_message) {
  JsonOwner root(cJSON_ParseWithLength(text.data(), text.size()));
  if (!cJSON_IsObject(root.get())) {
    error_message = "invalid session JSON";
    return false;
  }

  const cJSON* schema = cJSON_GetObjectItemCaseSensitive(root.get(), "schema_version");
  if (!cJSON_IsNumber(schema) || schema->valueint != K_SESSION_SCHEMA_VERSION) {
    error_message = "unsupported session schema";
    return false;
  }

  const cJSON* session = cJSON_GetObjectItemCaseSensitive(root.get(), "session");
  const cJSON* device  = cJSON_GetObjectItemCaseSensitive(root.get(), "device");
  const cJSON* tests   = cJSON_GetObjectItemCaseSensitive(root.get(), "tests");
  if (!cJSON_IsObject(session) || !cJSON_IsObject(device) || !cJSON_IsArray(tests)) {
    error_message = "session JSON is missing required sections";
    return false;
  }

  const char* id         = json_string(session, "id");
  const char* state      = json_string(session, "state");
  const char* started_at = json_string(session, "started_at");
  if (!id || !state || !started_at) {
    error_message = "session JSON is missing identity fields";
    return false;
  }

  loaded.state           = session_state_from_text(state);
  loaded.id              = id;
  loaded.sequence_number = static_cast<std::uint64_t>(json_size(session, "sequence_number"));
  loaded.started_at      = started_at;
  loaded.updated_at =
      nonempty_or(json_string(session, "updated_at") ? json_string(session, "updated_at") : "",
                  started_at);
  loaded.completed_at =
      json_string(session, "completed_at") ? json_string(session, "completed_at") : "";
  loaded.current_test_id =
      json_string(session, "current_test") ? json_string(session, "current_test") : "";
  loaded.result_path =
      json_string(root.get(), "result_path") ? json_string(root.get(), "result_path") : "";
  loaded.metadata.sku = json_string(device, "sku") ? json_string(device, "sku") : "UNKNOWN";
  loaded.metadata.serial_number = json_string(device, "sn") ? json_string(device, "sn") : "UNKNOWN";
  loaded.metadata.station =
      json_string(device, "station") ? json_string(device, "station") : "AUTO_TEST";
  loaded.metadata.firmware =
      json_string(device, "firmware") ? json_string(device, "firmware") : "UNKNOWN";
  loaded.metadata.commit =
      json_string(device, "commit") ? json_string(device, "commit") : "UNKNOWN";

  const cJSON* test = nullptr;
  cJSON_ArrayForEach(test, tests) {
    if (!cJSON_IsObject(test)) {
      error_message = "session test entry is invalid";
      return false;
    }
    const char* test_id   = json_string(test, "id");
    const char* test_name = json_string(test, "name");
    if (!test_id || !test_name) {
      error_message = "session test identity is missing";
      return false;
    }

    TestRecord record;
    record.id            = test_id;
    record.name          = test_name;
    record.completed     = json_bool(test, "completed");
    record.attempted     = json_bool(test, "attempted");
    record.attempt_count = json_size(test, "attempt_count");
    record.attempt_started_at =
        json_string(test, "attempt_started_at") ? json_string(test, "attempt_started_at") : "";
    record.attempt_finished_at =
        json_string(test, "attempt_finished_at") ? json_string(test, "attempt_finished_at") : "";
    record.completed_at =
        json_string(test, "completed_at") ? json_string(test, "completed_at") : "";
    if (record.completed) {
      record.result = test_result_from_text(json_string(test, "result"));
    }
    if (!parse_evidence(cJSON_GetObjectItemCaseSensitive(test, "evidence"), record.evidence) ||
        !parse_details(cJSON_GetObjectItemCaseSensitive(test, "details"), record.details)) {
      error_message = "session test evidence is invalid";
      return false;
    }
    loaded.tests.push_back(std::move(record));
  }
  return true;
}
#endif

}  // namespace

EvidenceValue EvidenceValue::boolean(bool value) {
  EvidenceValue result;
  result.type       = Type::BOOLEAN;
  result.bool_value = value;
  return result;
}

EvidenceValue EvidenceValue::number(double value) {
  EvidenceValue result;
  result.type         = Type::NUMBER;
  result.number_value = value;
  return result;
}

EvidenceValue EvidenceValue::string(std::string value) {
  EvidenceValue result;
  result.type         = Type::STRING;
  result.string_value = std::move(value);
  return result;
}

const char* test_result_text(TestResult result) {
  return result == TestResult::PASS ? "PASS" : "FAIL";
}

const char* session_state_text(SessionState state) {
  switch (state) {
    case SessionState::IN_PROGRESS:
      return "in_progress";
    case SessionState::COMPLETED:
      return "completed";
    case SessionState::ABANDONED:
      return "abandoned";
    case SessionState::NONE:
    default:
      return "none";
  }
}

bool SessionManager::start_new(const std::vector<TestDefinition>& tests, SessionMetadata metadata) {
  last_error_.clear();
  if (tests.empty()) {
    set_error_("test plan is empty");
    return false;
  }

  std::set<std::string> test_ids;
  for (const auto& test : tests) {
    if (test.id.empty() || test.name.empty() || !test_ids.insert(test.id).second) {
      set_error_("test plan contains an invalid or duplicate id");
      return false;
    }
  }

  if (state_ == SessionState::IN_PROGRESS && !abandon()) {
    return false;
  }
  reset_();

  const auto now         = std::chrono::system_clock::now();
  session_id_            = "factorytest-" + session_timestamp(now);
  sequence_number_       = epoch_milliseconds(now);
  started_at_            = iso_timestamp(now);
  updated_at_            = started_at_;
  state_                 = SessionState::IN_PROGRESS;
  metadata.sku           = nonempty_or(std::move(metadata.sku), "UNKNOWN");
  metadata.serial_number = nonempty_or(std::move(metadata.serial_number), "UNKNOWN");
  metadata.station       = nonempty_or(std::move(metadata.station), "AUTO_TEST");
  metadata.firmware      = nonempty_or(std::move(metadata.firmware), "UNKNOWN");
  metadata.commit        = short_commit(std::move(metadata.commit));
  metadata_              = std::move(metadata);

  const auto session_directory = default_session_directory();
  session_path_                = (session_directory / (session_id_ + K_SESSION_SUFFIX)).string();
  result_path_                 = result_path_for_session(session_path_).string();
  tests_.reserve(tests.size());
  for (const auto& test : tests) {
    TestRecord record;
    record.id   = test.id;
    record.name = test.name;
    tests_.push_back(std::move(record));
  }

  if (!persist_()) {
    const auto error = last_error_;
    reset_();
    last_error_ = error;
    return false;
  }
  LOG_INFO("test session started: {} path={}", session_id_, session_path_);
  return true;
}

bool SessionManager::load_latest_recoverable(const std::vector<TestDefinition>& expected_tests) {
  reset_();
#if APP_USE_LIBCJSON
  const auto directory = default_session_directory();
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec)) {
    return false;
  }

  std::filesystem::path latest_path;
  std::filesystem::file_time_type latest_time{};
  bool found = false;
  for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
    if (ec || !entry.is_regular_file(ec) ||
        !has_suffix(entry.path().filename().string(), K_SESSION_SUFFIX)) {
      continue;
    }
    const auto write_time = entry.last_write_time(ec);
    if (ec) {
      ec.clear();
      continue;
    }
    if (!found || write_time > latest_time ||
        (write_time == latest_time && entry.path().filename() > latest_path.filename())) {
      latest_path = entry.path();
      latest_time = write_time;
      found       = true;
    }
  }
  if (!found) {
    return false;
  }

  const auto text = read_text_file(latest_path);
  LoadedSession loaded;
  std::string parse_error;
  if (text.empty() || !parse_session_document(text, loaded, parse_error)) {
    LOG_WARN("failed to load latest test session {}: {}", latest_path.string(), parse_error);
    return false;
  }
  if (loaded.state != SessionState::IN_PROGRESS) {
    return false;
  }
  if (loaded.tests.size() != expected_tests.size()) {
    LOG_WARN("test session plan does not match the current test sequence: {}",
             latest_path.string());
    return false;
  }
  for (std::size_t i = 0; i < expected_tests.size(); ++i) {
    if (loaded.tests[i].id != expected_tests[i].id) {
      LOG_WARN("test session plan id mismatch at index {}: {}", i, latest_path.string());
      return false;
    }
    loaded.tests[i].name = expected_tests[i].name;
  }

  state_           = loaded.state;
  session_id_      = std::move(loaded.id);
  sequence_number_ = loaded.sequence_number;
  started_at_      = std::move(loaded.started_at);
  updated_at_      = std::move(loaded.updated_at);
  completed_at_    = std::move(loaded.completed_at);
  current_test_id_ = std::move(loaded.current_test_id);
  session_path_    = latest_path.string();
  result_path_     = loaded.result_path.empty() ? result_path_for_session(latest_path).string()
                                                : std::move(loaded.result_path);
  metadata_        = std::move(loaded.metadata);
  tests_           = std::move(loaded.tests);
  LOG_INFO("recoverable test session loaded: {} completed={}/{}",
           session_id_,
           summary().completed,
           tests_.size());
  return true;
#else
  (void)expected_tests;
  set_error_("cJSON library is not available");
  return false;
#endif
}

bool SessionManager::abandon() {
  if (state_ != SessionState::IN_PROGRESS) {
    return true;
  }
  const auto previous_state        = state_;
  const auto previous_current_test = current_test_id_;
  const auto previous_updated_at   = updated_at_;
  state_                           = SessionState::ABANDONED;
  current_test_id_.clear();
  updated_at_ = now_iso_timestamp();
  if (!persist_()) {
    state_           = previous_state;
    current_test_id_ = previous_current_test;
    updated_at_      = previous_updated_at;
    return false;
  }
  LOG_INFO("test session abandoned: {}", session_id_);
  return true;
}

bool SessionManager::active() const { return state_ == SessionState::IN_PROGRESS; }

bool SessionManager::recoverable() const { return active() && !session_path_.empty(); }

SessionState SessionManager::state() const { return state_; }

const std::string& SessionManager::session_id() const { return session_id_; }

const std::string& SessionManager::started_at() const { return started_at_; }

const std::string& SessionManager::completed_at() const { return completed_at_; }

const std::string& SessionManager::session_path() const { return session_path_; }

const std::string& SessionManager::result_path() const { return result_path_; }

const std::string& SessionManager::last_error() const { return last_error_; }

const std::vector<TestRecord>& SessionManager::tests() const { return tests_; }

SessionSummary SessionManager::summary() const {
  SessionSummary result;
  result.state = state_;
  result.total = tests_.size();
  for (const auto& test : tests_) {
    if (!test.completed) {
      continue;
    }
    ++result.completed;
    if (test.result == TestResult::PASS) {
      ++result.passed;
    } else {
      ++result.failed;
    }
    if (!test.has_evidence()) {
      ++result.missing_evidence;
    }
  }
  return result;
}

std::size_t SessionManager::next_incomplete_index() const {
  const auto it = std::find_if(tests_.begin(), tests_.end(), [](const TestRecord& test) {
    return !test.completed;
  });
  return static_cast<std::size_t>(std::distance(tests_.begin(), it));
}

bool SessionManager::set_current_test(const std::string& test_id) {
  if (!active() || !find_test_(test_id)) {
    set_error_("cannot select a test outside an active session");
    return false;
  }
  if (current_test_id_ == test_id) {
    return true;
  }
  const auto previous_current_test = current_test_id_;
  const auto previous_updated_at   = updated_at_;
  current_test_id_                 = test_id;
  updated_at_                      = now_iso_timestamp();
  if (!persist_()) {
    current_test_id_ = previous_current_test;
    updated_at_      = previous_updated_at;
    return false;
  }
  return true;
}

bool SessionManager::begin_test(const std::string& test_id) {
  auto* test = active() ? find_test_(test_id) : nullptr;
  if (!test || test->completed) {
    set_error_("cannot begin an unavailable test");
    return false;
  }
  const auto previous_test         = *test;
  const auto previous_current_test = current_test_id_;
  const auto previous_updated_at   = updated_at_;
  const auto now                   = now_iso_timestamp();
  current_test_id_                 = test_id;
  test->attempted                  = true;
  ++test->attempt_count;
  test->attempt_started_at = now;
  test->attempt_finished_at.clear();
  test->evidence.clear();
  updated_at_ = now;
  if (!persist_()) {
    *test            = previous_test;
    current_test_id_ = previous_current_test;
    updated_at_      = previous_updated_at;
    return false;
  }
  return true;
}

bool SessionManager::record_evidence(const std::string& test_id,
                                     const std::string& key,
                                     EvidenceValue value) {
  TestEvidence evidence;
  evidence.emplace(key, std::move(value));
  return record_evidence(test_id, evidence);
}

bool SessionManager::record_evidence(const std::string& test_id, const TestEvidence& evidence) {
  auto* test = active() ? find_test_(test_id) : nullptr;
  if (!test || test->completed || evidence.empty()) {
    set_error_("cannot record evidence for an unavailable test");
    return false;
  }
  for (const auto& entry : evidence) {
    if (entry.first.empty() || (entry.second.type == EvidenceValue::Type::NUMBER &&
                                !std::isfinite(entry.second.number_value))) {
      set_error_("test evidence contains an invalid key or number");
      return false;
    }
  }
  const auto previous_test       = *test;
  const auto previous_updated_at = updated_at_;
  if (!test->attempted) {
    test->attempted          = true;
    test->attempt_count      = 1;
    test->attempt_started_at = now_iso_timestamp();
  }
  for (const auto& entry : evidence) {
    test->evidence[entry.first] = entry.second;
  }
  updated_at_ = now_iso_timestamp();
  if (!persist_()) {
    *test       = previous_test;
    updated_at_ = previous_updated_at;
    return false;
  }
  return true;
}

bool SessionManager::complete_test(const std::string& test_id,
                                   TestResult result,
                                   const std::vector<NamedTestResult>& details) {
  auto* test = active() ? find_test_(test_id) : nullptr;
  if (!test) {
    set_error_("cannot complete a test outside an active session");
    return false;
  }

  const auto previous_test       = *test;
  const auto previous_updated_at = updated_at_;
  const auto now                 = now_iso_timestamp();
  test->completed                = true;
  test->result                   = result;
  test->completed_at             = now;
  test->details                  = details;
  if (test->attempted) {
    test->attempt_finished_at = now;
  }
  updated_at_ = now;
  if (!persist_()) {
    *test       = previous_test;
    updated_at_ = previous_updated_at;
    return false;
  }

  if (next_incomplete_index() >= tests_.size()) {
    return finalize();
  }
  return true;
}

bool SessionManager::finalize() {
  if (state_ == SessionState::COMPLETED) {
    return true;
  }
  if (!active() || next_incomplete_index() < tests_.size()) {
    set_error_("cannot finalize an incomplete test session");
    return false;
  }

  const auto previous_state        = state_;
  const auto previous_completed_at = completed_at_;
  const auto previous_updated_at   = updated_at_;
  const auto previous_current_test = current_test_id_;
  state_                           = SessionState::COMPLETED;
  completed_at_                    = now_iso_timestamp();
  updated_at_                      = completed_at_;
  current_test_id_.clear();
  if (!write_result_() || !persist_()) {
    state_           = previous_state;
    completed_at_    = previous_completed_at;
    updated_at_      = previous_updated_at;
    current_test_id_ = previous_current_test;
    return false;
  }
  LOG_INFO("test session completed: {} result={}", session_id_, result_path_);
  return true;
}

std::string SessionManager::build_result_json(bool formatted) const {
#if APP_USE_LIBCJSON
  if (session_id_.empty() || tests_.empty()) {
    return {};
  }

  JsonOwner root(cJSON_CreateObject());
  cJSON* details = root.get() ? cJSON_AddObjectToObject(root.get(), "test_details") : nullptr;
  cJSON* tests   = details ? cJSON_AddObjectToObject(details, "tests") : nullptr;
  const auto session_summary = summary();
  const bool passed =
      session_summary.completed == session_summary.total && session_summary.failed == 0;
  if (!root.get() || !details || !tests ||
      !cJSON_AddNumberToObject(root.get(), "seq", static_cast<double>(sequence_number_)) ||
      !cJSON_AddStringToObject(root.get(), "sku", metadata_.sku.c_str()) ||
      !cJSON_AddStringToObject(root.get(), "sn", metadata_.serial_number.c_str()) ||
      !cJSON_AddStringToObject(root.get(), "station", metadata_.station.c_str()) ||
      !cJSON_AddStringToObject(root.get(), "status", passed ? "PASS" : "FAIL") ||
      !cJSON_AddStringToObject(root.get(), "firmware", metadata_.firmware.c_str()) ||
      !cJSON_AddStringToObject(root.get(), "commit", metadata_.commit.c_str()) ||
      !cJSON_AddStringToObject(details, "detail_schema", K_SEQUENCE_ID) ||
      !cJSON_AddStringToObject(details, "session_id", session_id_.c_str()) ||
      !cJSON_AddStringToObject(details, "started_at", started_at_.c_str()) ||
      !cJSON_AddStringToObject(details, "completed_at", completed_at_.c_str())) {
    return {};
  }

  for (const auto& test : tests_) {
    cJSON* item = cJSON_AddObjectToObject(tests, test.id.c_str());
    if (!item ||
        !cJSON_AddStringToObject(item,
                                 "result",
                                 test.completed ? test_result_text(test.result) : "FAIL") ||
        !cJSON_AddBoolToObject(item, "attempted", test.attempted) ||
        !cJSON_AddBoolToObject(item, "has_evidence", test.has_evidence()) ||
        (!test.evidence.empty() && !add_evidence_object(item, "evidence", test.evidence)) ||
        (!test.details.empty() && !append_details(item, test.details))) {
      return {};
    }
  }
  return print_json(root.get(), formatted);
#else
  (void)formatted;
  return {};
#endif
}

TestRecord* SessionManager::find_test_(const std::string& test_id) {
  const auto it = std::find_if(tests_.begin(), tests_.end(), [&](const TestRecord& test) {
    return test.id == test_id;
  });
  return it == tests_.end() ? nullptr : &*it;
}

const TestRecord* SessionManager::find_test_(const std::string& test_id) const {
  const auto it = std::find_if(tests_.begin(), tests_.end(), [&](const TestRecord& test) {
    return test.id == test_id;
  });
  return it == tests_.end() ? nullptr : &*it;
}

bool SessionManager::persist_() {
#if APP_USE_LIBCJSON
  if (session_path_.empty()) {
    set_error_("session path is empty");
    return false;
  }

  updated_at_ = updated_at_.empty() ? now_iso_timestamp() : updated_at_;
  JsonOwner root(cJSON_CreateObject());
  cJSON* session = root.get() ? cJSON_AddObjectToObject(root.get(), "session") : nullptr;
  cJSON* device  = root.get() ? cJSON_AddObjectToObject(root.get(), "device") : nullptr;
  cJSON* tests   = root.get() ? cJSON_AddArrayToObject(root.get(), "tests") : nullptr;
  if (!root.get() || !session || !device || !tests ||
      !cJSON_AddNumberToObject(root.get(), "schema_version", K_SESSION_SCHEMA_VERSION) ||
      !cJSON_AddStringToObject(session, "id", session_id_.c_str()) ||
      !cJSON_AddStringToObject(session, "state", session_state_text(state_)) ||
      !cJSON_AddStringToObject(session, "sequence", K_SEQUENCE_ID) ||
      !cJSON_AddNumberToObject(session, "sequence_number", static_cast<double>(sequence_number_)) ||
      !cJSON_AddStringToObject(session, "started_at", started_at_.c_str()) ||
      !cJSON_AddStringToObject(session, "updated_at", updated_at_.c_str()) ||
      !cJSON_AddStringToObject(session, "completed_at", completed_at_.c_str()) ||
      !cJSON_AddStringToObject(session, "current_test", current_test_id_.c_str()) ||
      !cJSON_AddStringToObject(root.get(), "result_path", result_path_.c_str()) ||
      !cJSON_AddStringToObject(device, "sku", metadata_.sku.c_str()) ||
      !cJSON_AddStringToObject(device, "sn", metadata_.serial_number.c_str()) ||
      !cJSON_AddStringToObject(device, "station", metadata_.station.c_str()) ||
      !cJSON_AddStringToObject(device, "firmware", metadata_.firmware.c_str()) ||
      !cJSON_AddStringToObject(device, "commit", metadata_.commit.c_str())) {
    set_error_("failed to build session JSON");
    return false;
  }

  for (const auto& test : tests_) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !cJSON_AddStringToObject(item, "id", test.id.c_str()) ||
        !cJSON_AddStringToObject(item, "name", test.name.c_str()) ||
        !cJSON_AddBoolToObject(item, "completed", test.completed) ||
        !cJSON_AddBoolToObject(item, "attempted", test.attempted) ||
        !cJSON_AddNumberToObject(item, "attempt_count", static_cast<double>(test.attempt_count)) ||
        !cJSON_AddStringToObject(item, "attempt_started_at", test.attempt_started_at.c_str()) ||
        !cJSON_AddStringToObject(item, "attempt_finished_at", test.attempt_finished_at.c_str()) ||
        !cJSON_AddStringToObject(item, "completed_at", test.completed_at.c_str()) ||
        (test.completed &&
         !cJSON_AddStringToObject(item, "result", test_result_text(test.result))) ||
        (!test.evidence.empty() && !add_evidence_object(item, "evidence", test.evidence)) ||
        (!test.details.empty() && !append_details(item, test.details)) ||
        !cJSON_AddItemToArray(tests, item)) {
      cJSON_Delete(item);
      set_error_("failed to build session test JSON");
      return false;
    }
  }

  const auto text = print_json(root.get(), true);
  std::string write_error;
  if (text.empty() || !write_text_file_atomic(session_path_, text, write_error)) {
    set_error_(write_error.empty() ? "failed to format session JSON" : write_error);
    return false;
  }
  last_error_.clear();
  return true;
#else
  set_error_("cJSON library is not available");
  return false;
#endif
}

bool SessionManager::write_result_() {
  const auto text = build_result_json(true);
  if (text.empty()) {
    set_error_("failed to build protocol result JSON");
    return false;
  }
  std::string write_error;
  if (!write_text_file_atomic(result_path_, text, write_error)) {
    set_error_(write_error);
    return false;
  }
  return true;
}

void SessionManager::reset_() {
  state_ = SessionState::NONE;
  session_id_.clear();
  sequence_number_ = 0;
  started_at_.clear();
  updated_at_.clear();
  completed_at_.clear();
  current_test_id_.clear();
  session_path_.clear();
  result_path_.clear();
  last_error_.clear();
  metadata_ = {};
  tests_.clear();
}

void SessionManager::set_error_(std::string message) {
  last_error_ = std::move(message);
  LOG_WARN("test session error: {}", last_error_);
}

}  // namespace model
