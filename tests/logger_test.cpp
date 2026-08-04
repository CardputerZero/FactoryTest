/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
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

#include "logger.h"

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
         ("factory-test-logger-" + std::to_string(process_id) + "-" + std::to_string(timestamp));
}

std::vector<std::filesystem::path> log_files(const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> result;
  std::error_code ec;
  for (std::filesystem::directory_iterator iterator(directory, ec), end; !ec && iterator != end;
       iterator.increment(ec)) {
    const auto filename = iterator->path().filename().string();
    if (iterator->is_regular_file(ec) && filename.rfind("factory-test-", 0) == 0 &&
        iterator->path().extension() == ".log") {
      result.push_back(iterator->path());
    }
  }
  CHECK(!ec);
  std::sort(result.begin(), result.end());
  return result;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string read_logs(const std::filesystem::path& directory) {
  std::string result;
  for (const auto& path : log_files(directory)) {
    result += read_file(path);
  }
  return result;
}

std::size_t occurrence_count(const std::string& text, const std::string& needle) {
  std::size_t result = 0;
  std::size_t offset = 0;
  while ((offset = text.find(needle, offset)) != std::string::npos) {
    ++result;
    offset += needle.size();
  }
  return result;
}

void write_sized_file(const std::filesystem::path& path, std::size_t size) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << std::string(size, 'x');
}

void test_default_home_directory(const std::filesystem::path& root) {
  const auto home = root / "home";
#if defined(_WIN32)
  _putenv_s("HOME", home.string().c_str());
#else
  setenv("HOME", home.string().c_str(), 1);
#endif

  logger::Logger::init();
  const auto expected = home / ".local" / "state" / "factory-test" / "logs";
  CHECK(logger::Logger::file_logging_enabled());
  CHECK(std::filesystem::path(logger::Logger::log_directory()) == expected);
  LOG_INFO("default-home-path-check");
  logger::Logger::shutdown();
  CHECK(read_logs(expected).find("default-home-path-check") != std::string::npos);
}

void test_concurrent_log_completeness(const std::filesystem::path& root) {
  const auto directory = root / "concurrent";
  logger::FileLogConfig config;
  config.directory                = directory.string();
  config.max_segment_size_bytes   = 1024U * 1024U;
  config.max_directory_size_bytes = 2U * 1024U * 1024U;
  logger::Logger::init(config);
  logger::Logger::set_tag("complete-test");
  logger::Logger::set_color_mode(logger::ColorMode::DISABLE);

  constexpr int K_THREAD_COUNT        = 6;
  constexpr int K_MESSAGES_PER_THREAD = 100;
  std::vector<std::thread> workers;
  for (int thread = 0; thread < K_THREAD_COUNT; ++thread) {
    workers.emplace_back([thread]() {
      for (int message = 0; message < K_MESSAGES_PER_THREAD; ++message) {
        LOG_INFO("entry-{}-{}|", thread, message);
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  logger::Logger::shutdown();

  const auto contents = read_logs(directory);
  for (int thread = 0; thread < K_THREAD_COUNT; ++thread) {
    for (int message = 0; message < K_MESSAGES_PER_THREAD; ++message) {
      CHECK(occurrence_count(
                contents,
                "entry-" + std::to_string(thread) + "-" + std::to_string(message) + "|") == 1);
    }
  }
  CHECK(contents.find("[complete-test][I][") != std::string::npos);
  CHECK(contents.find("Z][logger_test.cpp:") != std::string::npos);
  CHECK(contents.find("][tid=") != std::string::npos);
}

void test_segmentation_and_directory_limit(const std::filesystem::path& root) {
  const auto directory = root / "segments";
  std::filesystem::create_directories(directory);
  write_sized_file(directory / "factory-test-old-0-part000.log", 1200);
  write_sized_file(directory / "factory-test-old-1-part000.log", 1200);
  write_sized_file(directory / "factory-test-old-2-part000.log", 1200);
  write_sized_file(directory / "keep.txt", 300);

  logger::FileLogConfig config;
  config.directory                = directory.string();
  config.max_segment_size_bytes   = 1024;
  config.max_directory_size_bytes = 4096;
  logger::Logger::init(config);
  logger::Logger::set_tag("segment-test");
  for (int index = 0; index < 80; ++index) {
    LOG_INFO("segment-entry-{:03d} {}", index, std::string(80, 'a' + (index % 26)));
  }
  logger::Logger::shutdown();

  const auto files = log_files(directory);
  CHECK(files.size() >= 2);
  CHECK(std::filesystem::is_regular_file(directory / "keep.txt"));
  CHECK(read_file(directory / "keep.txt").size() == 300);

  std::uintmax_t total_size = std::filesystem::file_size(directory / "keep.txt");
  for (const auto& path : files) {
    const auto size = std::filesystem::file_size(path);
    CHECK(size <= config.max_segment_size_bytes);
    total_size += size;
  }
  CHECK(total_size <= config.max_directory_size_bytes);
  CHECK(read_logs(directory).find("segment-entry-079") != std::string::npos);
  CHECK(!std::filesystem::exists(directory / "factory-test-old-0-part000.log"));
}

void test_file_failure_falls_back_to_console(const std::filesystem::path& root) {
  const auto blocker = root / "not-a-directory";
  write_sized_file(blocker, 16);

  logger::FileLogConfig config;
  config.directory = (blocker / "logs").string();
  logger::Logger::init(config);
  CHECK(!logger::Logger::file_logging_enabled());
  LOG_INFO("file-fallback-check");
  logger::Logger::shutdown();
  CHECK(std::filesystem::is_regular_file(blocker));
  CHECK(read_file(blocker).size() == 16);
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

  test_default_home_directory(root);
  test_concurrent_log_completeness(root);
  test_segmentation_and_directory_limit(root);
  test_file_failure_falls_back_to_console(root);

  logger::Logger::shutdown();
  std::filesystem::remove_all(root, ec);
  if (ec) {
    std::cerr << "failed to clean test root: " << ec.message() << '\n';
    ++failures;
  }

  if (failures != 0) {
    std::cerr << failures << " logger checks failed\n";
    return 1;
  }
  std::cout << "logger checks passed\n";
  return 0;
}
