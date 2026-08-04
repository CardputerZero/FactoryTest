/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "app_config.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                    \
  do {                                                                                      \
    if (!(condition)) {                                                                     \
      std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " << #condition << '\n'; \
      ++failures;                                                                           \
    }                                                                                       \
  } while (false)

std::filesystem::path unique_test_root() {
#if defined(_WIN32)
  const auto process_id = static_cast<long long>(_getpid());
#else
  const auto process_id = static_cast<long long>(getpid());
#endif
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("factory-test-app-config-" + std::to_string(process_id) + "-" +
          std::to_string(timestamp));
}

void set_home(const std::filesystem::path& home) {
#if defined(_WIN32)
  _putenv_s("HOME", home.string().c_str());
#else
  setenv("HOME", home.string().c_str(), 1);
#endif
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
}

void check_no_temporary_files(const std::filesystem::path& directory) {
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    CHECK(entry.path().filename().string().find("config.json.tmp-") == std::string::npos);
  }
}

void test_missing_file_creates_defaults(const std::filesystem::path& root) {
  const auto home = root / "missing";
  set_home(home);

  model::AppConfigStore store;
  CHECK(store.load());
  CHECK(store.path() == home / ".config" / "factory-test" / "config.json");
  CHECK(std::filesystem::is_regular_file(store.path()));
  const auto text = read_file(store.path());
  CHECK(text.find("\n  \"factory\"") != std::string::npos);
  CHECK(text.find("\"schema_version\": 1") != std::string::npos);
  CHECK(text.find("\"dark_mode\": true") != std::string::npos);
  CHECK(text.find("\"iperf_host\": \"192.168.10.187\"") != std::string::npos);
  CHECK(text.find("\"max_directory_size_bytes\": 5.24288e+07") == std::string::npos);
  CHECK(store.config().logging.max_directory_size_bytes == 50U * 1024U * 1024U);
}

void test_valid_round_trip(const std::filesystem::path& root) {
  const auto home = root / "round-trip";
  set_home(home);

  model::AppConfigStore writer;
  CHECK(writer.load());
  writer.config().ui.dark_mode                     = false;
  writer.config().ui.language                      = "zh_CN";
  writer.config().ui.key_click_enabled             = false;
  writer.config().ui.key_click_volume_percent      = 25;
  writer.config().network.iperf_host               = "10.0.0.8";
  writer.config().network.iperf_port               = 6201;
  writer.config().uart.baud_rate                   = 115200;
  writer.config().logging.level                    = "warn";
  writer.config().logging.max_segment_size_bytes   = 128U * 1024U;
  writer.config().logging.max_directory_size_bytes = 512U * 1024U;
  writer.config().factory.station_id               = "LINE_A";
  CHECK(writer.save());

  model::AppConfigStore reader;
  CHECK(reader.load());
  CHECK(!reader.config().ui.dark_mode);
  CHECK(reader.config().ui.language == "zh_CN");
  CHECK(!reader.config().ui.key_click_enabled);
  CHECK(reader.config().ui.key_click_volume_percent == 25);
  CHECK(reader.config().network.iperf_host == "10.0.0.8");
  CHECK(reader.config().network.iperf_port == 6201);
  CHECK(reader.config().uart.baud_rate == 115200);
  CHECK(reader.config().logging.level == "warn");
  CHECK(reader.config().logging.max_segment_size_bytes == 128U * 1024U);
  CHECK(reader.config().logging.max_directory_size_bytes == 512U * 1024U);
  CHECK(reader.config().factory.station_id == "LINE_A");
  check_no_temporary_files(reader.path().parent_path());
}

void test_partial_invalid_config_uses_and_writes_defaults(const std::filesystem::path& root) {
  const auto home = root / "partial";
  set_home(home);
  model::AppConfigStore initial;
  write_file(
      initial.path(),
      R"({"schema_version":1,"ui":{"dark_mode":false,"language":"xx","key_click_volume_percent":150},"network":{"iperf_host":"","iperf_port":70000},"uart":{"baud_rate":12345},"logging":{"level":"nope","max_segment_size_bytes":12,"max_directory_size_bytes":8},"factory":{"station_id":""}})");

  model::AppConfigStore store;
  CHECK(store.load());
  CHECK(!store.config().ui.dark_mode);
  CHECK(store.config().ui.language == "en");
  CHECK(store.config().ui.key_click_enabled);
  CHECK(store.config().ui.key_click_volume_percent == 50);
  CHECK(store.config().network.iperf_host == "192.168.10.187");
  CHECK(store.config().network.iperf_port == 5201);
  CHECK(store.config().uart.baud_rate == 9600);
  CHECK(store.config().logging.level == "debug");
  CHECK(store.config().factory.station_id == "AUTO_TEST");
  CHECK(!store.diagnostics().empty());

  model::AppConfigStore normalized;
  CHECK(normalized.load());
  CHECK(normalized.diagnostics().empty());
  check_no_temporary_files(normalized.path().parent_path());
}

void test_malformed_config_is_not_overwritten(const std::filesystem::path& root) {
  const auto home = root / "malformed";
  set_home(home);
  model::AppConfigStore store;
  const std::string malformed = "{not json\n";
  write_file(store.path(), malformed);

  CHECK(store.load());
  CHECK(store.config().ui.dark_mode);
  CHECK(!store.diagnostics().empty());
  CHECK(read_file(store.path()) == malformed);
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

  test_missing_file_creates_defaults(root);
  test_valid_round_trip(root);
  test_partial_invalid_config_uses_and_writes_defaults(root);
  test_malformed_config_is_not_overwritten(root);

  std::filesystem::remove_all(root, ec);
  if (ec) {
    std::cerr << "failed to clean test root: " << ec.message() << '\n';
    ++failures;
  }

  if (failures != 0) {
    std::cerr << failures << " app config checks failed\n";
    return 1;
  }
  std::cout << "app config checks passed\n";
  return 0;
}
