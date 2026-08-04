/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "logger.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef isatty
#define isatty _isatty
#endif
#endif

namespace logger {
namespace {

constexpr const char* RESET             = "\033[0m";
constexpr const char* GRAY              = "\033[90m";
constexpr const char* CYAN              = "\033[36m";
constexpr const char* GREEN             = "\033[32m";
constexpr const char* YELLOW            = "\033[33m";
constexpr const char* RED               = "\033[31m";
constexpr const char* MAGENTA           = "\033[35m";
constexpr const char* K_LOG_FILE_PREFIX = "factory-test-";
constexpr const char* K_LOG_FILE_SUFFIX = ".log";

struct ManagedFile {
  std::filesystem::path path;
  std::uintmax_t size{0};
  std::filesystem::file_time_type write_time{};
};

struct LoggerState {
  std::mutex mutex;
  LogLevel level{LogLevel::DEBUG};
  std::string tag;
  ColorMode color_mode{ColorMode::AUTO};
  bool timestamp_enabled{false};

  bool file_logging_enabled{false};
  bool file_error_reported{false};
  std::filesystem::path directory;
  std::filesystem::path current_path;
  std::string run_name;
  std::size_t max_segment_size{FileLogConfig::K_DEFAULT_SEGMENT_SIZE};
  std::size_t max_directory_size{FileLogConfig::K_DEFAULT_DIRECTORY_SIZE};
  std::size_t current_size{0};
  unsigned int segment_index{0};
  std::FILE* file{nullptr};

  ~LoggerState() {
    if (file) {
      std::fflush(file);
      std::fclose(file);
    }
  }
};

LoggerState& logger_state() {
  static LoggerState state;
  return state;
}

std::atomic<unsigned long long> run_sequence{0};

long long process_id() {
#ifdef _WIN32
  return static_cast<long long>(_getpid());
#else
  return static_cast<long long>(getpid());
#endif
}

const char* levelto_color(LogLevel level) {
  switch (level) {
    case LogLevel::TRACE:
      return GRAY;
    case LogLevel::DEBUG:
      return CYAN;
    case LogLevel::INFO:
      return GREEN;
    case LogLevel::WARN:
      return YELLOW;
    case LogLevel::ERROR:
      return RED;
    case LogLevel::FATAL:
      return MAGENTA;
    default:
      return RESET;
  }
}

const char* levelto_string(LogLevel level) {
  switch (level) {
    case LogLevel::TRACE:
      return "T";
    case LogLevel::DEBUG:
      return "D";
    case LogLevel::INFO:
      return "I";
    case LogLevel::WARN:
      return "W";
    case LogLevel::ERROR:
      return "E";
    case LogLevel::FATAL:
      return "F";
    default:
      return "?";
  }
}

bool term_supports_color() {
  const char* term = getenv("TERM");
  if (!term) return false;

  return strstr(term, "xterm") || strstr(term, "color") || strstr(term, "ansi") ||
         strstr(term, "screen") || strstr(term, "tmux") || strstr(term, "rxvt");
}

bool has_prefix_and_suffix(const std::string& value, const char* prefix, const char* suffix) {
  const std::size_t prefix_size = std::strlen(prefix);
  const std::size_t suffix_size = std::strlen(suffix);
  return value.size() >= prefix_size + suffix_size && value.compare(0, prefix_size, prefix) == 0 &&
         value.compare(value.size() - suffix_size, suffix_size, suffix) == 0;
}

const char* base_name(const char* path) {
  if (!path || path[0] == '\0') {
    return nullptr;
  }

  const char* slash     = strrchr(path, '/');
  const char* backslash = strrchr(path, '\\');
  const char* sep       = slash;
  if (backslash && (!sep || backslash > sep)) {
    sep = backslash;
  }
  return sep ? sep + 1 : path;
}

std::tm utc_time(std::time_t value) {
  std::tm tm_now{};
#ifdef _WIN32
  gmtime_s(&tm_now, &value);
#else
  gmtime_r(&value, &tm_now);
#endif
  return tm_now;
}

std::string current_timestamp() {
  const auto now        = std::chrono::system_clock::now();
  const auto time_value = std::chrono::system_clock::to_time_t(now);
  const auto tm_now     = utc_time(time_value);
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  char buffer[32];
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                tm_now.tm_year + 1900,
                tm_now.tm_mon + 1,
                tm_now.tm_mday,
                tm_now.tm_hour,
                tm_now.tm_min,
                tm_now.tm_sec,
                static_cast<int>(milliseconds.count()));
  return buffer;
}

std::string run_timestamp() {
  const auto now        = std::chrono::system_clock::now();
  const auto time_value = std::chrono::system_clock::to_time_t(now);
  const auto tm_now     = utc_time(time_value);
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  char buffer[32];
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d%02d%02dT%02d%02d%02d.%03dZ",
                tm_now.tm_year + 1900,
                tm_now.tm_mon + 1,
                tm_now.tm_mday,
                tm_now.tm_hour,
                tm_now.tm_min,
                tm_now.tm_sec,
                static_cast<int>(milliseconds.count()));
  return buffer;
}

std::string make_prefix(LogLevel level,
                        const std::string& tag,
                        bool timestamp_enabled,
                        const char* file,
                        int line) {
  std::string prefix;
  if (!tag.empty()) {
    prefix += fmt::format("[{}]", tag);
  }

  prefix += fmt::format("[{}]", levelto_string(level));

  if (timestamp_enabled) {
    prefix += fmt::format("[{}]", current_timestamp());
  }

  const char* name = base_name(file);
  if (name && line > 0) {
    prefix += fmt::format("[{}:{}]", name, line);
  }

  return prefix;
}

std::filesystem::path configured_directory(const FileLogConfig& config) {
  if (!config.directory.empty()) {
    return config.directory;
  }
  if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
    return std::filesystem::path(home) / ".local" / "state" / "factory-test" / "logs";
  }
  return {};
}

void close_file_locked(LoggerState& state) {
  if (!state.file) {
    return;
  }
  std::fflush(state.file);
  std::fclose(state.file);
  state.file = nullptr;
  state.current_path.clear();
  state.current_size = 0;
}

void report_file_error_locked(LoggerState& state, const std::string& message) {
  close_file_locked(state);
  state.file_logging_enabled = false;
  if (!state.file_error_reported) {
    std::fprintf(stderr, "[factory-test][logger] file logging disabled: %s\n", message.c_str());
    std::fflush(stderr);
    state.file_error_reported = true;
  }
}

bool make_directory_room_locked(LoggerState& state, std::uintmax_t reserved_bytes) {
  if (reserved_bytes > state.max_directory_size) {
    return false;
  }

  std::error_code ec;
  std::uintmax_t total_size = 0;
  std::vector<ManagedFile> managed_files;
  for (std::filesystem::directory_iterator iterator(state.directory, ec), end;
       !ec && iterator != end;
       iterator.increment(ec)) {
    const auto status = iterator->symlink_status(ec);
    if (ec) {
      break;
    }
    if (!std::filesystem::is_regular_file(status)) {
      continue;
    }

    const auto size = iterator->file_size(ec);
    if (ec) {
      break;
    }
    total_size += size;

    const auto filename = iterator->path().filename().string();
    if (!has_prefix_and_suffix(filename, K_LOG_FILE_PREFIX, K_LOG_FILE_SUFFIX) ||
        iterator->path() == state.current_path) {
      continue;
    }

    const auto write_time = iterator->last_write_time(ec);
    if (ec) {
      break;
    }
    managed_files.push_back({iterator->path(), size, write_time});
  }
  if (ec) {
    return false;
  }

  const auto target_size = static_cast<std::uintmax_t>(state.max_directory_size) - reserved_bytes;
  std::sort(managed_files.begin(), managed_files.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.write_time != rhs.write_time) {
      return lhs.write_time < rhs.write_time;
    }
    return lhs.path.filename() < rhs.path.filename();
  });

  for (const auto& file : managed_files) {
    if (total_size <= target_size) {
      break;
    }
    std::filesystem::remove(file.path, ec);
    if (ec) {
      return false;
    }
    total_size -= file.size;
  }
  return total_size <= target_size;
}

std::string truncate_line(std::string line, std::size_t maximum_size) {
  constexpr const char* marker  = "... [truncated]\n";
  const std::size_t marker_size = std::strlen(marker);
  if (line.size() <= maximum_size) {
    return line;
  }
  if (maximum_size <= marker_size) {
    line.resize(maximum_size);
    return line;
  }
  line.resize(maximum_size - marker_size);
  line += marker;
  return line;
}

bool write_bytes_locked(LoggerState& state, const std::string& text) {
  if (!state.file || text.empty()) {
    return state.file != nullptr;
  }
  const auto written = std::fwrite(text.data(), 1, text.size(), state.file);
  if (written != text.size() || std::fflush(state.file) != 0) {
    report_file_error_locked(state, "failed to write " + state.current_path.string());
    return false;
  }
  state.current_size += written;
  return true;
}

std::FILE* open_exclusive_file(const std::filesystem::path& path) {
#ifdef _WIN32
  const int descriptor = _open(path.string().c_str(),
                               _O_BINARY | _O_WRONLY | _O_CREAT | _O_EXCL,
                               _S_IREAD | _S_IWRITE);
  if (descriptor < 0) {
    return nullptr;
  }
  std::FILE* file = _fdopen(descriptor, "wb");
  if (!file) {
    _close(descriptor);
  }
#else
  const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (descriptor < 0) {
    return nullptr;
  }
  std::FILE* file = fdopen(descriptor, "wb");
  if (!file) {
    ::close(descriptor);
  }
#endif
  return file;
}

bool open_next_segment_locked(LoggerState& state) {
  close_file_locked(state);
  if (!make_directory_room_locked(state, state.max_segment_size)) {
    report_file_error_locked(state, "cannot enforce log directory size limit");
    return false;
  }

  const std::string base_run_name = state.run_name;
  unsigned int collision_index    = 0;
  unsigned int opened_segment     = state.segment_index;
  while (!state.file) {
    state.current_path =
        state.directory /
        fmt::format("{}-part{:03d}{}", state.run_name, opened_segment, K_LOG_FILE_SUFFIX);
    errno      = 0;
    state.file = open_exclusive_file(state.current_path);
    if (state.file) {
      state.segment_index = opened_segment + 1;
      break;
    }
    if (errno != EEXIST) {
      report_file_error_locked(state, "failed to open " + state.current_path.string());
      return false;
    }
    if (state.segment_index == 0) {
      state.run_name = fmt::format("{}-c{}", base_run_name, ++collision_index);
    } else {
      ++opened_segment;
    }
  }

  const auto header =
      truncate_line(fmt::format("[logger][I][{}][tid={:x}] run={} pid={} segment={}\n",
                                current_timestamp(),
                                std::hash<std::thread::id>{}(std::this_thread::get_id()),
                                state.run_name,
                                process_id(),
                                opened_segment),
                    state.max_segment_size);
  return write_bytes_locked(state, header);
}

std::string file_log_line(LogLevel level,
                          const std::string& tag,
                          const std::string& message,
                          const char* file,
                          int line) {
  std::string prefix = make_prefix(level, tag, true, file, line);
  prefix += fmt::format("[tid={:x}]", std::hash<std::thread::id>{}(std::this_thread::get_id()));
  return fmt::format("{} {}\n", prefix, message);
}

bool use_color_locked(const LoggerState& state) {
  switch (state.color_mode) {
    case ColorMode::DISABLE:
      return false;
    case ColorMode::ENABLE:
      return true;
    case ColorMode::AUTO:
    default:
      break;
  }

  if (getenv("NO_COLOR")) return false;
  if (isatty(STDOUT_FILENO) == 0) return false;
  return term_supports_color();
}

}  // namespace

void Logger::init() { init(FileLogConfig{}); }

void Logger::init(const FileLogConfig& config) {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);

  close_file_locked(state);
  state.file_logging_enabled = false;
  state.file_error_reported  = false;
  state.directory.clear();
  state.run_name.clear();
  state.segment_index = 0;

  if (!config.enabled) {
    return;
  }
  if (config.max_segment_size_bytes == 0 || config.max_directory_size_bytes == 0) {
    report_file_error_locked(state, "log size limits must be greater than zero");
    return;
  }

  state.directory = configured_directory(config);
  if (state.directory.empty()) {
    report_file_error_locked(state, "HOME is not set");
    return;
  }
  state.max_directory_size = config.max_directory_size_bytes;
  state.max_segment_size = std::min(config.max_segment_size_bytes, config.max_directory_size_bytes);

  std::error_code ec;
  std::filesystem::create_directories(state.directory, ec);
  if (ec || !std::filesystem::is_directory(state.directory, ec)) {
    report_file_error_locked(state, "failed to create " + state.directory.string());
    return;
  }

  state.run_name             = fmt::format("{}{}-p{}-r{}",
                                           K_LOG_FILE_PREFIX,
                                           run_timestamp(),
                                           process_id(),
                                           run_sequence.fetch_add(1, std::memory_order_relaxed));
  state.file_logging_enabled = true;
  open_next_segment_locked(state);
}

void Logger::shutdown() {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  close_file_locked(state);
  state.file_logging_enabled = false;
}

bool Logger::file_logging_enabled() {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.file_logging_enabled;
}

std::string Logger::log_directory() {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.directory.string();
}

std::string Logger::current_log_path() {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.current_path.string();
}

void Logger::set_level(LogLevel level) {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.level = level;
}

LogLevel Logger::level() {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.level;
}

void Logger::set_tag(const char* tag) {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.tag = tag ? tag : "";
}

const char* Logger::tag() {
  static thread_local std::string tag_copy;
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  tag_copy = state.tag;
  return tag_copy.c_str();
}

void Logger::set_color_mode(ColorMode mode) {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.color_mode = mode;
}

void Logger::set_timestamp_enabled(bool enabled) {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.timestamp_enabled = enabled;
}

bool Logger::timestamp_enabled() {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.timestamp_enabled;
}

// conditional log
bool Logger::should_log(LogLevel level) {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  return level >= state.level && state.level != LogLevel::OFF;
}

bool Logger::should_use_color() {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  return use_color_locked(state);
}

void Logger::log(LogLevel level, const std::string& msg, const char* file, int line) {
  auto& state = logger_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (level < state.level || state.level == LogLevel::OFF) return;

  std::string prefix = make_prefix(level, state.tag, state.timestamp_enabled, file, line);

  if (use_color_locked(state)) {
    printf("%s%s %s%s\n", levelto_color(level), prefix.c_str(), msg.c_str(), RESET);
  } else {
    printf("%s %s\n", prefix.c_str(), msg.c_str());
  }
  std::fflush(stdout);

  if (!state.file_logging_enabled) {
    return;
  }

  auto log_line = file_log_line(level, state.tag, msg, file, line);
  if (log_line.size() > state.max_segment_size) {
    log_line = truncate_line(std::move(log_line), state.max_segment_size);
  }
  if (state.current_size > 0 && state.current_size + log_line.size() > state.max_segment_size) {
    if (!open_next_segment_locked(state)) {
      return;
    }
  }
  const auto remaining = state.max_segment_size - state.current_size;
  write_bytes_locked(state, truncate_line(std::move(log_line), remaining));
}

}  // namespace logger
