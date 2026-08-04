/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <string>
#include <utility>

namespace logger {

enum class LogLevel { TRACE = 0, DEBUG, INFO, WARN, ERROR, FATAL, OFF };

enum class ColorMode { AUTO, ENABLE, DISABLE };

struct FileLogConfig {
  static constexpr std::size_t K_DEFAULT_SEGMENT_SIZE   = 5U * 1024U * 1024U;
  static constexpr std::size_t K_DEFAULT_DIRECTORY_SIZE = 50U * 1024U * 1024U;

  // An empty directory resolves to $HOME/.local/state/factory-test/logs.
  std::string directory{};
  std::size_t max_segment_size_bytes{K_DEFAULT_SEGMENT_SIZE};
  std::size_t max_directory_size_bytes{K_DEFAULT_DIRECTORY_SIZE};
  bool enabled{true};
};

class Logger {
 public:
  static void init();
  static void init(const FileLogConfig& config);
  static void shutdown();

  static bool file_logging_enabled();
  static std::string log_directory();
  static std::string current_log_path();

  static void set_level(LogLevel level);
  static LogLevel level();

  static void set_tag(const char* tag);
  static const char* tag();

  static void set_color_mode(ColorMode mode);

  static void set_timestamp_enabled(bool enabled);
  static bool timestamp_enabled();

  template <typename... Args>
  static void trace(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::TRACE, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void verbose(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::TRACE, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void debug(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::DEBUG, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void info(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::INFO, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void warn(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::WARN, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void error(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::ERROR, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void fatal(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::FATAL, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void trace_at(const char* file,
                       int line,
                       fmt::format_string<Args...> fmt_str,
                       Args&&... args) {
    log(LogLevel::TRACE, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
  }

  template <typename... Args>
  static void verbose_at(const char* file,
                         int line,
                         fmt::format_string<Args...> fmt_str,
                         Args&&... args) {
    log(LogLevel::TRACE, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
  }

  template <typename... Args>
  static void debug_at(const char* file,
                       int line,
                       fmt::format_string<Args...> fmt_str,
                       Args&&... args) {
    log(LogLevel::DEBUG, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
  }

  template <typename... Args>
  static void info_at(const char* file,
                      int line,
                      fmt::format_string<Args...> fmt_str,
                      Args&&... args) {
    log(LogLevel::INFO, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
  }

  template <typename... Args>
  static void warn_at(const char* file,
                      int line,
                      fmt::format_string<Args...> fmt_str,
                      Args&&... args) {
    log(LogLevel::WARN, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
  }

  template <typename... Args>
  static void error_at(const char* file,
                       int line,
                       fmt::format_string<Args...> fmt_str,
                       Args&&... args) {
    log(LogLevel::ERROR, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
  }

  template <typename... Args>
  static void fatal_at(const char* file,
                       int line,
                       fmt::format_string<Args...> fmt_str,
                       Args&&... args) {
    log(LogLevel::FATAL, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
  }

 protected:
  static void log(LogLevel level, const std::string& msg, const char* file = nullptr, int line = 0);

  static bool should_log(LogLevel level);
  static bool should_use_color();
};

}  // namespace logger

#define LOG_TRACE(...)   ::logger::Logger::trace_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_VERBOSE(...) ::logger::Logger::verbose_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...)   ::logger::Logger::debug_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)    ::logger::Logger::info_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)    ::logger::Logger::warn_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)   ::logger::Logger::error_at(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...)   ::logger::Logger::fatal_at(__FILE__, __LINE__, __VA_ARGS__)
