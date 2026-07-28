/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "power_service.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "logger.h"
#include "process_service.h"

#if defined(__linux__)
#include <unistd.h>
#endif

#ifndef APP_POWER_SUPPLY_ROOT
#define APP_POWER_SUPPLY_ROOT "/sys/class/power_supply"
#endif

namespace platform::power {
namespace {

std::string trim(std::string value) {
  const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

bool read_text_file(const std::filesystem::path& path, std::string& value) {
  std::ifstream file(path);
  if (!file) {
    return false;
  }

  std::string raw;
  std::getline(file, raw);
  value = trim(raw);
  return true;
}

bool read_int_file(const std::filesystem::path& path, int64_t& value) {
  std::ifstream file(path);
  if (!file) {
    return false;
  }

  int64_t parsed = 0;
  file >> parsed;
  if (!file) {
    return false;
  }

  value = parsed;
  return true;
}

int32_t read_optional_i32(const std::filesystem::path& path, int32_t fallback = -1) {
  int64_t value = 0;
  if (!read_int_file(path, value)) {
    return fallback;
  }
  return static_cast<int32_t>(value);
}

int64_t read_optional_i64(const std::filesystem::path& path) {
  int64_t value = 0;
  return read_int_file(path, value) ? value : -1;
}

std::filesystem::path find_battery_path(std::string& error_message) {
  const std::filesystem::path root(APP_POWER_SUPPLY_ROOT);
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) {
    error_message = "Power supply sysfs root not found.";
    return {};
  }

  std::filesystem::path fallback;
  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    if (ec) {
      break;
    }

    const auto& path = entry.path();
    std::string type;
    if (read_text_file(path / "type", type) && type == "Battery") {
      return path;
    }
    if (fallback.empty()) {
      fallback = path;
    }
  }

  if (fallback.empty()) {
    error_message = "No power supply device found.";
  }
  return fallback;
}

bool request_power_action(const char* action, std::string& error_message) {
#if defined(USE_DESKTOP) && USE_DESKTOP
  error_message = "Power operations are disabled in desktop builds.";
  LOG_WARN("refusing {} request in desktop build", action);
  return false;
#elif defined(__linux__)
  LOG_INFO("requesting safe system {}", action);
  ::sync();

  process::ProcessOptions options;
  options.timeout_ms       = 5000;
  options.max_output_bytes = 4096;
  const auto result =
      process::run_command("systemctl", {"--no-block", "--no-ask-password", action}, options);
  if (result.success()) {
    error_message.clear();
    return true;
  }

  error_message = !result.error_message.empty()
                      ? result.error_message
                      : (!result.stderr_text.empty() ? result.stderr_text
                                                     : "systemctl request failed");
  LOG_ERROR("safe system {} request failed: {}", action, error_message);
  return false;
#else
  error_message = "Power operations are not supported on this platform.";
  LOG_WARN("unsupported safe system {} request", action);
  return false;
#endif
}

}  // namespace

bool read_battery_info(PowerSupplyInfo& info, std::string& error_message) {
  const auto path = find_battery_path(error_message);
  if (path.empty()) {
    static bool logged = false;
    if (!logged) {
      LOG_WARN("failed to find battery power supply: {}", error_message);
      logged = true;
    }
    return false;
  }

  static std::filesystem::path logged_path;
  if (logged_path != path) {
    LOG_INFO("using power supply path: {}", path.string());
    logged_path = path;
  }

  PowerSupplyInfo next{};
  next.device_name = path.filename().string();
  read_text_file(path / "type", next.type);
  read_text_file(path / "status", next.status);
  read_text_file(path / "health", next.health);
  read_text_file(path / "technology", next.technology);
  read_text_file(path / "manufacturer", next.manufacturer);
  read_text_file(path / "capacity_level", next.capacity_level);
  next.capacity_percent       = read_optional_i32(path / "capacity");
  next.present                = read_optional_i32(path / "present");
  next.cycle_count            = read_optional_i32(path / "cycle_count");
  next.voltage_now_uv         = read_optional_i64(path / "voltage_now");
  next.voltage_instant_uv     = read_optional_i64(path / "voltage_instant");
  next.current_now_ua         = read_optional_i64(path / "current_now");
  next.current_instant_ua     = read_optional_i64(path / "current_instant");
  next.charge_now_uah         = read_optional_i64(path / "charge_now");
  next.charge_full_uah        = read_optional_i64(path / "charge_full");
  next.charge_full_design_uah = read_optional_i64(path / "charge_full_design");
  next.power_avg_uw           = read_optional_i64(path / "power_avg");
  next.temp_decic             = read_optional_i32(path / "temp", -100000);

  info = std::move(next);
  error_message.clear();
  return true;
}

bool safe_shutdown(std::string& error_message) {
  return request_power_action("poweroff", error_message);
}

bool safe_reboot(std::string& error_message) {
  return request_power_action("reboot", error_message);
}

}  // namespace platform::power
