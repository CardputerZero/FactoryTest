/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace model {

enum class TestResult {
  PASS,
  FAIL,
};

struct NamedTestResult {
  std::string test_name;
  TestResult result{TestResult::FAIL};
};

struct TestDefinition {
  std::string id;
  std::string name;
};

struct SessionMetadata {
  std::string sku;
  std::string serial_number;
  std::string station{"AUTO_TEST"};
  std::string firmware;
  std::string commit;
};

struct EvidenceValue {
  enum class Type {
    BOOLEAN,
    NUMBER,
    STRING,
  };

  Type type{Type::STRING};
  bool bool_value{false};
  double number_value{0.0};
  std::string string_value{};

  static EvidenceValue boolean(bool value);
  static EvidenceValue number(double value);
  static EvidenceValue string(std::string value);
};

using TestEvidence = std::map<std::string, EvidenceValue>;

struct TestRecord {
  std::string id;
  std::string name;
  bool completed{false};
  TestResult result{TestResult::FAIL};
  bool attempted{false};
  std::size_t attempt_count{0};
  std::string attempt_started_at;
  std::string attempt_finished_at;
  std::string completed_at;
  TestEvidence evidence;
  std::vector<NamedTestResult> details;

  bool has_evidence() const { return !evidence.empty(); }
};

enum class SessionState {
  NONE,
  IN_PROGRESS,
  COMPLETED,
  ABANDONED,
};

struct SessionSummary {
  SessionState state{SessionState::NONE};
  std::size_t total{0};
  std::size_t completed{0};
  std::size_t passed{0};
  std::size_t failed{0};
  std::size_t missing_evidence{0};
};

class SessionManager {
 public:
  bool start_new(const std::vector<TestDefinition>& tests, SessionMetadata metadata);
  bool load_latest_recoverable(const std::vector<TestDefinition>& expected_tests);
  bool abandon();

  bool active() const;
  bool recoverable() const;
  SessionState state() const;
  const std::string& session_id() const;
  const std::string& started_at() const;
  const std::string& completed_at() const;
  const std::string& session_path() const;
  const std::string& result_path() const;
  const std::string& last_error() const;
  const std::vector<TestRecord>& tests() const;
  SessionSummary summary() const;

  std::size_t next_incomplete_index() const;
  bool set_current_test(const std::string& test_id);
  bool begin_test(const std::string& test_id);
  bool record_evidence(const std::string& test_id, const std::string& key, EvidenceValue value);
  bool record_evidence(const std::string& test_id, const TestEvidence& evidence);
  bool complete_test(const std::string& test_id,
                     TestResult result,
                     const std::vector<NamedTestResult>& details = {});
  bool finalize();

  std::string build_result_json(bool formatted = true) const;

 private:
  TestRecord* find_test_(const std::string& test_id);
  const TestRecord* find_test_(const std::string& test_id) const;
  bool persist_();
  bool write_result_();
  void reset_();
  void set_error_(std::string message);

  SessionState state_{SessionState::NONE};
  std::string session_id_{};
  std::uint64_t sequence_number_{0};
  std::string started_at_{};
  std::string updated_at_{};
  std::string completed_at_{};
  std::string current_test_id_{};
  std::string session_path_{};
  std::string result_path_{};
  std::string last_error_{};
  SessionMetadata metadata_{};
  std::vector<TestRecord> tests_{};
};

const char* test_result_text(TestResult result);
const char* session_state_text(SessionState state);

}  // namespace model
