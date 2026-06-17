/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "device_info_service.h"

#include <sys/statvfs.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace platform::device_info {
namespace {

constexpr const char* K_EMPTY_VALUE = "--";

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
  return !value.empty();
}

std::string read_or_empty(const std::filesystem::path& path) {
  std::string value;
  return read_text_file(path, value) ? value : K_EMPTY_VALUE;
}

std::string read_device_model() { return "CardputerZero"; }

std::string read_device_sn() { return "0608106478"; }

std::string read_cpuinfo_field(const std::string& key) {
  std::ifstream file("/proc/cpuinfo");
  std::string line;
  while (std::getline(file, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }

    if (trim(line.substr(0, colon)) == key) {
      return trim(line.substr(colon + 1));
    }
  }
  return K_EMPTY_VALUE;
}

std::string read_hardware_revision() { return read_or_empty("/proc/cardputerzero_hw_id"); }

std::string read_soc_id() {
  std::string value;
  if (read_text_file("/sys/devices/soc0/soc_id", value)) {
    return value;
  }
  if (read_text_file("/sys/devices/soc0/machine", value)) {
    return value;
  }
  value = read_cpuinfo_field("Hardware");
  return value.empty() ? K_EMPTY_VALUE : value;
}

std::string format_bytes(double bytes) {
  constexpr const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  std::size_t unit_index        = 0;
  while (bytes >= 1024.0 && unit_index + 1 < std::size(units)) {
    bytes /= 1024.0;
    ++unit_index;
  }

  std::ostringstream out;
  if (bytes >= 10.0 || unit_index == 0) {
    out << static_cast<int>(bytes + 0.5);
  } else {
    out.setf(std::ios::fixed);
    out.precision(1);
    out << bytes;
  }
  out << units[unit_index];
  return out.str();
}

std::string read_ram() {
  std::ifstream file("/proc/meminfo");
  std::string key;
  int64_t kb = 0;
  std::string unit;
  while (file >> key >> kb >> unit) {
    if (key == "MemTotal:") {
      return format_bytes(static_cast<double>(kb) * 1024.0);
    }
  }
  return K_EMPTY_VALUE;
}

std::string read_storage() {
  struct statvfs stat{};
  if (statvfs("/", &stat) != 0) {
    return K_EMPTY_VALUE;
  }

  const double total = static_cast<double>(stat.f_blocks) * static_cast<double>(stat.f_frsize);
  const double free  = static_cast<double>(stat.f_bavail) * static_cast<double>(stat.f_frsize);
  const double used  = std::max(0.0, total - free);
  return format_bytes(used) + "/" + format_bytes(total);
}

std::string read_mac() {
  const std::filesystem::path net_root("/sys/class/net");
  std::error_code ec;
  if (!std::filesystem::exists(net_root, ec)) {
    return K_EMPTY_VALUE;
  }

  std::string fallback;
  for (const auto& entry : std::filesystem::directory_iterator(net_root, ec)) {
    if (ec) {
      break;
    }

    const auto name = entry.path().filename().string();
    if (name == "lo") {
      continue;
    }

    std::string address;
    if (!read_text_file(entry.path() / "address", address) || address == "00:00:00:00:00:00") {
      continue;
    }

    std::string operstate;
    if (read_text_file(entry.path() / "operstate", operstate) && operstate == "up") {
      return address;
    }
    if (fallback.empty()) {
      fallback = address;
    }
  }

  return fallback.empty() ? K_EMPTY_VALUE : fallback;
}

std::string unquote(std::string value) {
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::string read_os() {
  std::ifstream file("/etc/os-release");
  std::string line;
  while (std::getline(file, line)) {
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    if (line.substr(0, equals) == "PRETTY_NAME") {
      return unquote(trim(line.substr(equals + 1)));
    }
  }
  return K_EMPTY_VALUE;
}

std::string read_uptime() {
  std::ifstream file("/proc/uptime");
  double seconds = 0.0;
  file >> seconds;
  if (!file) {
    return K_EMPTY_VALUE;
  }

  auto total      = static_cast<int64_t>(seconds);
  const auto days = total / 86400;
  total %= 86400;
  const auto hours = total / 3600;
  total %= 3600;
  const auto minutes = total / 60;

  std::ostringstream out;
  if (days > 0) {
    out << days << "d ";
  }
  out << hours << "h " << minutes << "m";
  return out.str();
}

std::string read_rtc() {
#if defined(__linux__)
  int fd = open("/dev/rtc", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    fd = open("/dev/rtc0", O_RDONLY | O_CLOEXEC);
  }
  if (fd < 0) {
    return K_EMPTY_VALUE;
  }

  struct rtc_time rtc{};
  const int result = ioctl(fd, RTC_RD_TIME, &rtc);
  close(fd);
  if (result < 0) {
    return K_EMPTY_VALUE;
  }

  char buffer[24];
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d-%02d-%02d %02d:%02d:%02d",
                rtc.tm_year + 1900,
                rtc.tm_mon + 1,
                rtc.tm_mday,
                rtc.tm_hour,
                rtc.tm_min,
                rtc.tm_sec);
  return buffer;
#else
  return K_EMPTY_VALUE;
#endif
}

}  // namespace

std::vector<DeviceInfoField> read_device_info_fields() {
  return {
      {"Model", read_device_model()},
      {"Hardware Rev", read_hardware_revision()},
      {"Serial Number", read_device_sn()},
      {"SoC ID", read_soc_id()},
      {"RAM", read_ram()},
      {"Storage", read_storage()},
      {"MAC", read_mac()},
      {"OS", read_os()},
      {"Kernel", read_or_empty("/proc/sys/kernel/osrelease")},
      {"Uptime", read_uptime()},
      {"RTC", read_rtc()},
  };
}

}  // namespace platform::device_info
