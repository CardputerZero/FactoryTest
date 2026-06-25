/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "hdmi_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "logger.h"

#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace platform::connectivity {
namespace {

bool same_hdmi_info(const HdmiInfo& left, const HdmiInfo& right) {
  return left.connector_name == right.connector_name && left.status == right.status &&
         left.enabled == right.enabled && left.resolution == right.resolution &&
         left.sys_path == right.sys_path && left.connected == right.connected;
}

std::string lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string trim(const std::string& value) {
  const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  });
  const auto end   = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
                     return std::isspace(ch) != 0;
                     }).base();
  return begin < end ? std::string(begin, end) : std::string{};
}

struct HdmiLogSnapshot {
  bool initialized{false};
  HdmiInfo info{};
  std::string error_message{};
};

bool update_hdmi_log_snapshot(HdmiLogSnapshot& snapshot,
                              const HdmiInfo& info,
                              const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_hdmi_info(snapshot.info, info)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.info          = info;
  snapshot.error_message = error_message;
  return true;
}

void log_hdmi_result(const HdmiInfo& info, const std::string& error_message, bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity HDMI result unchanged");
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity HDMI result message: {}", error_message);
  }

  LOG_INFO("connectivity HDMI result: connector='{}' status='{}' enabled='{}' resolution='{}'",
           info.connector_name,
           info.status,
           info.enabled,
           info.resolution);
}

std::string read_first_line(const std::filesystem::path& path) {
  std::ifstream file(path);
  std::string line;
  if (file.is_open()) {
    std::getline(file, line);
  }
  return line;
}

bool is_hdmi_connector_name(const std::string& name) {
  return lower_copy(name).find("hdmi") != std::string::npos;
}

std::string connector_display_name(const std::filesystem::path& path) {
  auto name       = path.filename().string();
  const auto dash = name.find('-');
  if (dash != std::string::npos && dash + 1 < name.size()) {
    return name.substr(dash + 1);
  }
  return name;
}

std::string first_hdmi_mode(const std::filesystem::path& connector_path) {
  std::ifstream file(connector_path / "modes");
  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (!line.empty()) {
      return line;
    }
  }
  return {};
}

std::optional<std::filesystem::path> choose_hdmi_connector(std::string& error_message) {
  std::error_code ec;
  std::vector<std::filesystem::path> candidates;
  for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm", ec)) {
    const auto name = entry.path().filename().string();
    if (is_hdmi_connector_name(name) && std::filesystem::exists(entry.path() / "status")) {
      candidates.push_back(entry.path());
    }
  }

  if (ec) {
    error_message = std::string("failed to enumerate DRM connectors: ") + ec.message();
    return std::nullopt;
  }
  if (candidates.empty()) {
    error_message = "No HDMI connector found";
    return std::nullopt;
  }

  std::sort(candidates.begin(), candidates.end());
  for (const auto& candidate : candidates) {
    if (trim(read_first_line(candidate / "status")) == "connected") {
      return candidate;
    }
  }
  return candidates.front();
}

}  // namespace

HdmiInfo read_hdmi_info(std::string& error_message) {
  static HdmiLogSnapshot log_snapshot;
  error_message.clear();
  HdmiInfo info;

#if defined(__linux__)
  LOG_TRACE("connectivity HDMI info requested");
  const auto connector_path = choose_hdmi_connector(error_message);
  if (!connector_path) {
    const bool changed = update_hdmi_log_snapshot(log_snapshot, info, error_message);
    log_hdmi_result(info, error_message, changed);
    return info;
  }

  info.connector_name = connector_display_name(*connector_path);
  info.sys_path       = connector_path->string();
  info.status         = trim(read_first_line(*connector_path / "status"));
  info.enabled        = trim(read_first_line(*connector_path / "enabled"));
  info.connected      = info.status == "connected";
  info.resolution     = first_hdmi_mode(*connector_path);

  if (!info.connected) {
    error_message = "HDMI disconnected";
  } else if (info.resolution.empty()) {
    error_message = "HDMI connected, no mode detected";
  }

  const bool changed = update_hdmi_log_snapshot(log_snapshot, info, error_message);
  log_hdmi_result(info, error_message, changed);
  return info;
#else
  LOG_TRACE("connectivity HDMI info requested but Linux DRM sysfs is unavailable");
  error_message      = "Linux DRM sysfs is not available";
  const bool changed = update_hdmi_log_snapshot(log_snapshot, info, error_message);
  log_hdmi_result(info, error_message, changed);
  return info;
#endif
}
}  // namespace platform::connectivity
