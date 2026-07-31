/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "test_session.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                    \
  do {                                                                                      \
    if (!(condition)) {                                                                     \
      std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " << #condition << '\n'; \
      ++failures;                                                                           \
    }                                                                                       \
  } while (false)

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string without_whitespace(std::string value) {
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](unsigned char character) { return std::isspace(character); }),
              value.end());
  return value;
}

void set_session_directory(const std::filesystem::path& path) {
#if defined(_WIN32)
  _putenv_s("FACTORY_TEST_SESSION_DIR", path.string().c_str());
#else
  setenv("FACTORY_TEST_SESSION_DIR", path.string().c_str(), 1);
#endif
}

std::filesystem::path unique_test_root() {
#if defined(_WIN32)
  const auto process_id = static_cast<long long>(_getpid());
#else
  const auto process_id = static_cast<long long>(getpid());
#endif
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("factory-test-session-manager-" + std::to_string(process_id) + "-" +
          std::to_string(timestamp));
}

model::SessionMetadata metadata() {
  return {"CardputerZero", "SN-TEST-001", "AUTO_TEST", "0.2.9", "1234567890"};
}

const std::vector<model::TestDefinition> kPlan = {
    {"input", "Input Test"},
    {"display", "Display Test"},
};

void check_no_temporary_files(const std::filesystem::path& directory) {
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    CHECK(entry.path().extension() != ".tmp");
  }
}

void test_persistence_evidence_and_result(const std::filesystem::path& root) {
  const auto directory = root / "persistence";
  set_session_directory(directory);

  model::SessionManager session;
  CHECK(session.start_new(kPlan, metadata()));
  CHECK(session.active());
  CHECK(std::filesystem::is_regular_file(session.session_path()));
  CHECK(session.summary().completed == 0);
  CHECK(session.build_result_json(false).find("\"status\":\"FAIL\"") != std::string::npos);

  CHECK(session.set_current_test("input"));
  CHECK(session.complete_test("input", model::TestResult::PASS));
  CHECK(session.active());
  CHECK(session.summary().passed == 1);
  CHECK(session.summary().missing_evidence == 1);
  CHECK(!session.tests()[0].attempted);
  CHECK(!session.tests()[0].has_evidence());

  model::SessionManager recovered;
  CHECK(recovered.load_latest_recoverable(kPlan));
  CHECK(recovered.next_incomplete_index() == 1);
  CHECK(recovered.tests()[0].result == model::TestResult::PASS);
  CHECK(!recovered.tests()[0].has_evidence());

  model::TestEvidence evidence;
  evidence.emplace("pattern_rendered", model::EvidenceValue::boolean(true));
  evidence.emplace("frame_count", model::EvidenceValue::number(6));
  evidence.emplace("mode", model::EvidenceValue::string("RGB"));
  CHECK(recovered.record_evidence("display", evidence));
  CHECK(recovered.tests()[1].attempted);
  CHECK(recovered.tests()[1].attempt_count == 1);
  CHECK(recovered.tests()[1].has_evidence());
  CHECK(recovered.complete_test("display", model::TestResult::FAIL));

  CHECK(recovered.state() == model::SessionState::COMPLETED);
  CHECK(recovered.summary().failed == 1);
  CHECK(std::filesystem::is_regular_file(recovered.result_path()));
  const auto result         = read_file(recovered.result_path());
  const auto compact_result = without_whitespace(result);
  CHECK(!result.empty());
  CHECK(compact_result.find("\"status\":\"FAIL\"") != std::string::npos);
  CHECK(compact_result.find("\"result\":\"PASS\"") != std::string::npos);
  CHECK(compact_result.find("\"result\":\"FAIL\"") != std::string::npos);
  CHECK(compact_result.find("\"has_evidence\":false") != std::string::npos);
  CHECK(compact_result.find("\"pattern_rendered\":true") != std::string::npos);
  CHECK(compact_result.find("\"commit\":\"12345678\"") != std::string::npos);
  CHECK(result.find("FAILED") == std::string::npos);
  check_no_temporary_files(directory);
}

void test_only_latest_session_is_recoverable(const std::filesystem::path& root) {
  const auto directory = root / "latest";
  set_session_directory(directory);

  model::SessionManager older;
  CHECK(older.start_new(kPlan, metadata()));
  const auto older_id   = older.session_id();
  const auto older_path = std::filesystem::path(older.session_path());

  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  model::SessionManager newer;
  CHECK(newer.start_new(kPlan, metadata()));
  CHECK(newer.session_id() != older_id);
  CHECK(newer.complete_test("input", model::TestResult::PASS));

  const auto now = std::filesystem::file_time_type::clock::now();
  std::filesystem::last_write_time(older_path, now - std::chrono::seconds(10));
  std::filesystem::last_write_time(newer.session_path(), now + std::chrono::seconds(10));

  model::SessionManager recovered;
  CHECK(recovered.load_latest_recoverable(kPlan));
  CHECK(recovered.session_id() == newer.session_id());
  CHECK(recovered.next_incomplete_index() == 1);

  CHECK(newer.complete_test("display", model::TestResult::PASS));
  std::filesystem::last_write_time(newer.session_path(), now + std::chrono::seconds(20));

  model::SessionManager completed_latest;
  CHECK(!completed_latest.load_latest_recoverable(kPlan));
  CHECK(completed_latest.state() == model::SessionState::NONE);
  CHECK(std::filesystem::is_regular_file(older_path));
  check_no_temporary_files(directory);
}

void test_missing_session_and_plan_mismatch(const std::filesystem::path& root) {
  set_session_directory(root / "empty");
  model::SessionManager empty;
  CHECK(!empty.load_latest_recoverable(kPlan));

  const auto directory = root / "mismatch";
  set_session_directory(directory);
  model::SessionManager session;
  CHECK(session.start_new(kPlan, metadata()));

  model::SessionManager mismatch;
  CHECK(!mismatch.load_latest_recoverable({{"different", "Different Test"}}));
  CHECK(mismatch.state() == model::SessionState::NONE);
}

}  // namespace

int main() {
  const auto root = unique_test_root();
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  if (ec) {
    std::cerr << "failed to create test root: " << ec.message() << '\n';
    return 1;
  }

  test_persistence_evidence_and_result(root);
  test_only_latest_session_is_recoverable(root);
  test_missing_session_and_plan_mismatch(root);

  std::filesystem::remove_all(root, ec);
  if (ec) {
    std::cerr << "failed to clean test root: " << ec.message() << '\n';
    ++failures;
  }

  if (failures != 0) {
    std::cerr << failures << " session manager checks failed\n";
    return 1;
  }
  std::cout << "session manager checks passed\n";
  return 0;
}
