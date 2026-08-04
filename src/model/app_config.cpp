/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_config.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "serialization.h"

namespace model {
namespace {

using OutputValue = platform::serialization::OutputValue;

constexpr std::size_t K_MIN_LOG_SEGMENT_SIZE = 64U * 1024U;
constexpr std::array<int, 5> K_UART_BAUD_RATES{9600, 19200, 38400, 57600, 115200};
const std::set<std::string> K_LOG_LEVELS{"trace", "debug", "info", "warn", "error", "fatal", "off"};
const std::set<std::string> K_LANGUAGES{"en", "zh_CN"};

std::atomic<unsigned long long> temporary_sequence{0};

long long process_id() {
#if defined(_WIN32)
  return static_cast<long long>(_getpid());
#else
  return static_cast<long long>(getpid());
#endif
}

std::filesystem::path default_config_path() {
  const char* home = std::getenv("HOME");
  if (!home || home[0] == '\0') {
    return {};
  }
  return std::filesystem::path(home) / ".config" / "factory-test" / "config.json";
}

const OutputValue* object_member(const OutputValue* object, const char* key) {
  if (!object || object->type != OutputValue::Type::Object || !key) {
    return nullptr;
  }
  const auto it = object->object_values.find(key);
  return it == object->object_values.end() ? nullptr : &it->second;
}

void use_default(std::vector<std::string>& diagnostics, const char* field) {
  diagnostics.emplace_back(std::string("config field '") + field +
                           "' is missing or invalid; using default");
}

bool read_bool(const OutputValue* object,
               const char* key,
               const char* field,
               bool fallback,
               bool& corrected,
               std::vector<std::string>& diagnostics) {
  const auto* value = object_member(object, key);
  if (value && value->type == OutputValue::Type::Boolean) {
    return value->bool_value;
  }
  corrected = true;
  use_default(diagnostics, field);
  return fallback;
}

std::string read_string(const OutputValue* object,
                        const char* key,
                        const char* field,
                        const std::string& fallback,
                        bool& corrected,
                        std::vector<std::string>& diagnostics,
                        const std::set<std::string>* allowed = nullptr) {
  const auto* value = object_member(object, key);
  if (value && value->type == OutputValue::Type::String && !value->string_value.empty() &&
      (!allowed || allowed->find(value->string_value) != allowed->end())) {
    return value->string_value;
  }
  corrected = true;
  use_default(diagnostics, field);
  return fallback;
}

bool integer_value(const OutputValue* value,
                   long long minimum,
                   long long maximum,
                   long long& result) {
  if (!value || value->type != OutputValue::Type::Number || !std::isfinite(value->number_value) ||
      std::floor(value->number_value) != value->number_value || value->number_value < minimum ||
      value->number_value > maximum) {
    return false;
  }
  result = static_cast<long long>(value->number_value);
  return true;
}

int read_int(const OutputValue* object,
             const char* key,
             const char* field,
             int fallback,
             int minimum,
             int maximum,
             bool& corrected,
             std::vector<std::string>& diagnostics) {
  long long value = 0;
  if (integer_value(object_member(object, key), minimum, maximum, value)) {
    return static_cast<int>(value);
  }
  corrected = true;
  use_default(diagnostics, field);
  return fallback;
}

std::size_t read_size(const OutputValue* object,
                      const char* key,
                      const char* field,
                      std::size_t fallback,
                      std::size_t minimum,
                      bool& corrected,
                      std::vector<std::string>& diagnostics) {
  long long value                                   = 0;
  constexpr std::uintmax_t K_MAX_EXACT_JSON_INTEGER = 9007199254740991ULL;
  const auto maximum                                = static_cast<long long>(
      std::min<std::uintmax_t>(std::numeric_limits<std::size_t>::max(), K_MAX_EXACT_JSON_INTEGER));
  if (integer_value(object_member(object, key), static_cast<long long>(minimum), maximum, value)) {
    return static_cast<std::size_t>(value);
  }
  corrected = true;
  use_default(diagnostics, field);
  return fallback;
}

bool is_supported_baud_rate(int baud_rate) {
  return std::find(K_UART_BAUD_RATES.begin(), K_UART_BAUD_RATES.end(), baud_rate) !=
         K_UART_BAUD_RATES.end();
}

OutputValue config_to_output(const AppConfig& config) {
  return OutputValue::object({
      {"factory",
       OutputValue::object({{"station_id", OutputValue::string(config.factory.station_id)}})},
      {"logging",
       OutputValue::object({
           {"level", OutputValue::string(config.logging.level)},
           {"max_directory_size_bytes",
            OutputValue::number(static_cast<double>(config.logging.max_directory_size_bytes))},
           {"max_segment_size_bytes",
            OutputValue::number(static_cast<double>(config.logging.max_segment_size_bytes))},
       })},
      {"network",
       OutputValue::object({
           {"iperf_host", OutputValue::string(config.network.iperf_host)},
           {"iperf_port", OutputValue::number(config.network.iperf_port)},
       })},
      {"schema_version", OutputValue::number(config.schema_version)},
      {"uart", OutputValue::object({{"baud_rate", OutputValue::number(config.uart.baud_rate)}})},
      {"ui",
       OutputValue::object({
           {"dark_mode", OutputValue::boolean(config.ui.dark_mode)},
           {"key_click_enabled", OutputValue::boolean(config.ui.key_click_enabled)},
           {"key_click_volume_percent", OutputValue::number(config.ui.key_click_volume_percent)},
           {"language", OutputValue::string(config.ui.language)},
       })},
  });
}

bool write_all(int descriptor, const std::string& text) {
  std::size_t offset = 0;
  while (offset < text.size()) {
#if defined(_WIN32)
    const int written =
        _write(descriptor, text.data() + offset, static_cast<unsigned int>(text.size() - offset));
#else
    const auto written = ::write(descriptor, text.data() + offset, text.size() - offset);
#endif
    if (written <= 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

bool sync_descriptor(int descriptor) {
#if defined(_WIN32)
  return _commit(descriptor) == 0;
#else
  return ::fsync(descriptor) == 0;
#endif
}

void close_descriptor(int descriptor) {
#if defined(_WIN32)
  _close(descriptor);
#else
  ::close(descriptor);
#endif
}

bool sync_directory(const std::filesystem::path& directory) {
#if defined(__unix__) || defined(__APPLE__)
  const int descriptor = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY);
  if (descriptor < 0) {
    return false;
  }
  const bool synced = ::fsync(descriptor) == 0;
  ::close(descriptor);
  return synced;
#else
  (void)directory;
  return true;
#endif
}

bool write_atomic(const std::filesystem::path& path,
                  const std::string& text,
                  std::string& error_message) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error_message = "failed to create config directory: " + ec.message();
    return false;
  }
#if !defined(_WIN32)
  std::filesystem::permissions(path.parent_path(),
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace,
                               ec);
  if (ec) {
    error_message = "failed to secure config directory: " + ec.message();
    return false;
  }
#endif

  auto temporary = path;
  temporary += ".tmp-" + std::to_string(process_id()) + "-" +
               std::to_string(temporary_sequence.fetch_add(1, std::memory_order_relaxed));

#if defined(_WIN32)
  const int descriptor = _open(temporary.string().c_str(),
                               _O_BINARY | _O_WRONLY | _O_CREAT | _O_EXCL,
                               _S_IREAD | _S_IWRITE);
#else
  const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
  if (descriptor < 0) {
    error_message = "failed to create temporary config file: " + std::string(std::strerror(errno));
    return false;
  }

  const bool written = write_all(descriptor, text);
  const bool synced  = written && sync_descriptor(descriptor);
  close_descriptor(descriptor);
  if (!written || !synced) {
    std::filesystem::remove(temporary, ec);
    error_message =
        written ? "failed to sync temporary config file" : "failed to write temporary config file";
    return false;
  }

  std::filesystem::rename(temporary, path, ec);
#if defined(_WIN32)
  if (ec) {
    std::error_code remove_error;
    std::filesystem::remove(path, remove_error);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
  }
#endif
  if (ec) {
    const auto rename_error = ec.message();
    std::error_code remove_error;
    std::filesystem::remove(temporary, remove_error);
    error_message = "failed to replace config file: " + rename_error;
    return false;
  }

  if (!sync_directory(path.parent_path())) {
    error_message = "failed to sync config directory";
    return false;
  }
  return true;
}

}  // namespace

AppConfigStore::AppConfigStore()
    : path_(default_config_path()) {}

bool AppConfigStore::load() {
  config_ = AppConfig{};
  last_error_.clear();
  diagnostics_.clear();

  if (path_.empty()) {
    last_error_ = "HOME is not set; cannot resolve config path";
    diagnostics_.push_back(last_error_);
    return false;
  }

  std::error_code ec;
  const bool exists = std::filesystem::exists(path_, ec);
  if (ec) {
    last_error_ = "failed to inspect config file: " + ec.message();
    diagnostics_.push_back(last_error_);
    return false;
  }
  if (!exists) {
    if (!save()) {
      diagnostics_.push_back(last_error_);
      return false;
    }
    return true;
  }

  const auto parsed = platform::serialization::parse_json_file(path_.string());
  if (!parsed.success() || parsed.value.type != OutputValue::Type::Object) {
    last_error_ = parsed.success() ? "config root must be a JSON object" : parsed.error_message;
    diagnostics_.push_back(last_error_ + "; using defaults without overwriting the file");
    return true;
  }

  bool corrected           = false;
  const auto& defaults     = AppConfig{};
  long long schema_version = 0;
  if (!integer_value(object_member(&parsed.value, "schema_version"),
                     AppConfig::K_SCHEMA_VERSION,
                     AppConfig::K_SCHEMA_VERSION,
                     schema_version)) {
    diagnostics_.push_back("unsupported or missing config schema_version; using defaults");
    return true;
  }
  config_.schema_version = static_cast<int>(schema_version);

  const auto* ui = object_member(&parsed.value, "ui");
  config_.ui.dark_mode =
      read_bool(ui, "dark_mode", "ui.dark_mode", defaults.ui.dark_mode, corrected, diagnostics_);
  config_.ui.language                 = read_string(ui,
                                                    "language",
                                                    "ui.language",
                                                    defaults.ui.language,
                                                    corrected,
                                                    diagnostics_,
                                                    &K_LANGUAGES);
  config_.ui.key_click_enabled        = read_bool(ui,
                                                  "key_click_enabled",
                                                  "ui.key_click_enabled",
                                                  defaults.ui.key_click_enabled,
                                                  corrected,
                                                  diagnostics_);
  config_.ui.key_click_volume_percent = read_int(ui,
                                                 "key_click_volume_percent",
                                                 "ui.key_click_volume_percent",
                                                 defaults.ui.key_click_volume_percent,
                                                 0,
                                                 100,
                                                 corrected,
                                                 diagnostics_);

  const auto* network        = object_member(&parsed.value, "network");
  config_.network.iperf_host = read_string(network,
                                           "iperf_host",
                                           "network.iperf_host",
                                           defaults.network.iperf_host,
                                           corrected,
                                           diagnostics_);
  config_.network.iperf_port = read_int(network,
                                        "iperf_port",
                                        "network.iperf_port",
                                        defaults.network.iperf_port,
                                        1,
                                        65535,
                                        corrected,
                                        diagnostics_);

  const auto* uart       = object_member(&parsed.value, "uart");
  config_.uart.baud_rate = read_int(uart,
                                    "baud_rate",
                                    "uart.baud_rate",
                                    defaults.uart.baud_rate,
                                    1,
                                    1000000,
                                    corrected,
                                    diagnostics_);
  if (!is_supported_baud_rate(config_.uart.baud_rate)) {
    config_.uart.baud_rate = defaults.uart.baud_rate;
    corrected              = true;
    use_default(diagnostics_, "uart.baud_rate");
  }

  const auto* logging                      = object_member(&parsed.value, "logging");
  config_.logging.level                    = read_string(logging,
                                                         "level",
                                                         "logging.level",
                                                         defaults.logging.level,
                                                         corrected,
                                                         diagnostics_,
                                                         &K_LOG_LEVELS);
  config_.logging.max_segment_size_bytes   = read_size(logging,
                                                       "max_segment_size_bytes",
                                                       "logging.max_segment_size_bytes",
                                                       defaults.logging.max_segment_size_bytes,
                                                       K_MIN_LOG_SEGMENT_SIZE,
                                                       corrected,
                                                       diagnostics_);
  config_.logging.max_directory_size_bytes = read_size(logging,
                                                       "max_directory_size_bytes",
                                                       "logging.max_directory_size_bytes",
                                                       defaults.logging.max_directory_size_bytes,
                                                       K_MIN_LOG_SEGMENT_SIZE,
                                                       corrected,
                                                       diagnostics_);
  if (config_.logging.max_directory_size_bytes < config_.logging.max_segment_size_bytes) {
    config_.logging.max_directory_size_bytes = defaults.logging.max_directory_size_bytes;
    config_.logging.max_segment_size_bytes   = defaults.logging.max_segment_size_bytes;
    corrected                                = true;
    diagnostics_.push_back(
        "logging.max_directory_size_bytes must be at least max_segment_size_bytes; using defaults");
  }

  const auto* factory        = object_member(&parsed.value, "factory");
  config_.factory.station_id = read_string(factory,
                                           "station_id",
                                           "factory.station_id",
                                           defaults.factory.station_id,
                                           corrected,
                                           diagnostics_);

  if (corrected && !save()) {
    diagnostics_.push_back(last_error_);
    return false;
  }
  return true;
}

bool AppConfigStore::save() {
  last_error_.clear();
  if (path_.empty()) {
    last_error_ = "HOME is not set; cannot resolve config path";
    return false;
  }

  std::string text =
      platform::serialization::output_value_to_pretty_string(config_to_output(config_));
  text.push_back('\n');
  return write_atomic(path_, text, last_error_);
}

const AppConfig& AppConfigStore::config() const { return config_; }

AppConfig& AppConfigStore::config() { return config_; }

const std::filesystem::path& AppConfigStore::path() const { return path_; }

const std::string& AppConfigStore::last_error() const { return last_error_; }

const std::vector<std::string>& AppConfigStore::diagnostics() const { return diagnostics_; }

}  // namespace model
