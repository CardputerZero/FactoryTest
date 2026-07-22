/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "device_info_service.h"

#include <sys/statvfs.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "i2c_service.h"

#if defined(__linux__)
#include <fcntl.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace platform::device_info {
namespace {

constexpr const char* K_EMPTY_VALUE          = "--";
constexpr const char* K_HW_REVISION_RAW_PATH = "/sys/bus/iio/devices/iio:device0/in_voltage0_raw";
constexpr double K_ADC_REFERENCE_MV          = 3356.0;
constexpr double K_ADC_RESOLUTION_STEPS      = 4096.0;
constexpr double K_HW_REVISION_SCALE_MV      = K_ADC_REFERENCE_MV / K_ADC_RESOLUTION_STEPS;
constexpr double K_HW_REVISION_TOLERANCE_MV  = 100.0;
constexpr std::size_t K_HW_REVISION_SAMPLE_COUNT     = 5;
constexpr std::size_t K_HW_REVISION_MIN_SAMPLE_COUNT = 3;
constexpr uint32_t K_HW_REVISION_SAMPLE_INTERVAL_MS  = 10;
constexpr int K_PY32_I2C_BUS                         = 1;
constexpr uint8_t K_PY32_I2C_ADDRESS                 = 0x4F;
constexpr uint8_t K_CP0_MINOR_VERSION                = 0xBA;

struct HardwareRevisionVoltage {
  double millivolts;
  const char* revision;
};

constexpr std::array<HardwareRevisionVoltage, 2> K_HARDWARE_REVISIONS = {{
    {2500.0, "0.3"},
    {2000.0, "0.6"},
}};

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

bool read_devicetree_string(const std::filesystem::path& path, std::string& value) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  value = buffer.str();
  value.erase(std::remove(value.begin(), value.end(), '\0'), value.end());
  value = trim(value);
  return !value.empty();
}

std::string read_or_empty(const std::filesystem::path& path) {
  std::string value;
  return read_text_file(path, value) ? value : K_EMPTY_VALUE;
}

std::string read_device_model() { return "CardputerZero"; }

std::string read_soc_serial_number() {
  std::string value;
  if (read_devicetree_string("/sys/firmware/devicetree/base/serial-number", value)) {
    return value;
  }
  return K_EMPTY_VALUE;
}

bool read_number_file(const std::filesystem::path& path, double& value) {
  std::ifstream file(path);
  file >> value;
  return static_cast<bool>(file) && std::isfinite(value);
}

bool read_median_number_file(const std::filesystem::path& path, double& value) {
  std::array<double, K_HW_REVISION_SAMPLE_COUNT> samples{};
  std::size_t sample_count = 0;

  for (std::size_t i = 0; i < K_HW_REVISION_SAMPLE_COUNT; ++i) {
    double sample = 0.0;
    if (read_number_file(path, sample)) {
      samples[sample_count++] = sample;
    }

    if (i + 1 < K_HW_REVISION_SAMPLE_COUNT) {
      std::this_thread::sleep_for(std::chrono::milliseconds(K_HW_REVISION_SAMPLE_INTERVAL_MS));
    }
  }

  if (sample_count < K_HW_REVISION_MIN_SAMPLE_COUNT) {
    return false;
  }

  std::sort(samples.begin(), samples.begin() + sample_count);
  const std::size_t middle = sample_count / 2;
  value = sample_count % 2 == 0 ? (samples[middle - 1] + samples[middle]) / 2.0 : samples[middle];
  return true;
}

std::string read_hardware_revision() {
  double raw = 0.0;
  if (!read_median_number_file(K_HW_REVISION_RAW_PATH, raw)) {
    return K_EMPTY_VALUE;
  }

  const double millivolts = raw * K_HW_REVISION_SCALE_MV;
  for (const auto& mapping : K_HARDWARE_REVISIONS) {
    if (std::abs(millivolts - mapping.millivolts) <= K_HW_REVISION_TOLERANCE_MV) {
      return mapping.revision;
    }
  }
  return K_EMPTY_VALUE;
}

std::string read_py32_firmware_version() {
  uint8_t version = 0;
  std::string error_message;
  if (!platform::connectivity::read_i2c_byte_data(K_PY32_I2C_BUS,
                                                  K_PY32_I2C_ADDRESS,
                                                  K_CP0_MINOR_VERSION,
                                                  version,
                                                  error_message)) {
    return K_EMPTY_VALUE;
  }

  char value[5]{};
  std::snprintf(value, sizeof(value), "0x%02X", static_cast<unsigned int>(version));
  return value;
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

#if defined(__linux__)
std::string format_local_time(std::time_t value) {
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &value);
#else
  if (!localtime_r(&value, &local)) {
    return K_EMPTY_VALUE;
  }
#endif

  char buffer[24];
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d-%02d-%02d %02d:%02d:%02d",
                local.tm_year + 1900,
                local.tm_mon + 1,
                local.tm_mday,
                local.tm_hour,
                local.tm_min,
                local.tm_sec);
  return buffer;
}

std::string format_rtc_time_as_local(std::tm rtc_utc) {
  rtc_utc.tm_isdst = 0;
  const std::time_t utc_time = timegm(&rtc_utc);
  if (utc_time == static_cast<std::time_t>(-1)) {
    return K_EMPTY_VALUE;
  }
  return format_local_time(utc_time);
}

std::string format_rtc_time_as_local(const rtc_time& rtc) {
  std::tm rtc_utc{};
  rtc_utc.tm_sec  = rtc.tm_sec;
  rtc_utc.tm_min  = rtc.tm_min;
  rtc_utc.tm_hour = rtc.tm_hour;
  rtc_utc.tm_mday = rtc.tm_mday;
  rtc_utc.tm_mon  = rtc.tm_mon;
  rtc_utc.tm_year = rtc.tm_year;
  return format_rtc_time_as_local(rtc_utc);
}

bool parse_rtc_datetime(const std::string& value, std::tm& rtc_utc) {
  int year  = 0;
  int month = 0;
  int day   = 0;
  int hour  = 0;
  int min   = 0;
  int sec   = 0;
  if (std::sscanf(value.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) !=
      6) {
    return false;
  }

  rtc_utc         = {};
  rtc_utc.tm_year = year - 1900;
  rtc_utc.tm_mon  = month - 1;
  rtc_utc.tm_mday = day;
  rtc_utc.tm_hour = hour;
  rtc_utc.tm_min  = min;
  rtc_utc.tm_sec  = sec;
  return true;
}

std::string read_rtc_ioctl() {
  int fd = open("/dev/rtc", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    fd = open("/dev/rtc0", O_RDONLY | O_CLOEXEC);
  }
  if (fd < 0) {
    return {};
  }

  struct rtc_time rtc{};
  const int result = ioctl(fd, RTC_RD_TIME, &rtc);
  close(fd);
  if (result < 0) {
    return {};
  }

  return format_rtc_time_as_local(rtc);
}

std::string read_rtc_sysfs() {
  const std::array<const char*, 2> rtc_names = {"rtc0", "rtc"};
  for (const auto* rtc_name : rtc_names) {
    const auto root = std::filesystem::path("/sys/class/rtc") / rtc_name;
    std::string date;
    std::string time;
    if (read_text_file(root / "date", date) && read_text_file(root / "time", time)) {
      std::tm rtc_utc{};
      const auto value = date + " " + time;
      return parse_rtc_datetime(value, rtc_utc) ? format_rtc_time_as_local(rtc_utc) : value;
    }
  }
  return {};
}

std::string read_hwclock_command() {
  const std::array<const char*, 3> commands = {
      "/usr/sbin/hwclock -r 2>/dev/null",
      "/sbin/hwclock -r 2>/dev/null",
      "hwclock -r 2>/dev/null",
  };

  for (const auto* command : commands) {
    FILE* pipe = popen(command, "r");
    if (!pipe) {
      continue;
    }

    char buffer[128] = {};
    std::string output;
    if (std::fgets(buffer, sizeof(buffer), pipe)) {
      output = trim(buffer);
    }
    pclose(pipe);
    if (!output.empty()) {
      return output;
    }
  }
  return {};
}
#endif

std::string timezone_from_path(std::filesystem::path path) {
  const auto text = path.lexically_normal().string();
  constexpr const char* marker = "zoneinfo/";
  const auto pos              = text.find(marker);
  if (pos == std::string::npos) {
    return {};
  }
  return text.substr(pos + std::char_traits<char>::length(marker));
}

std::string read_timezone() {
  if (const char* env_tz = std::getenv("TZ"); env_tz && env_tz[0] != '\0') {
    std::string value = trim(env_tz);
    if (!value.empty() && value.front() == ':') {
      value.erase(value.begin());
    }
    if (!value.empty()) {
      return value;
    }
  }

  std::string value;
  if (read_text_file("/etc/timezone", value)) {
    return value;
  }

  std::error_code ec;
  if (const auto symlink = std::filesystem::read_symlink("/etc/localtime", ec); !ec) {
    if (const auto timezone = timezone_from_path(symlink); !timezone.empty()) {
      return timezone;
    }
  }

  ec.clear();
  if (const auto canonical = std::filesystem::canonical("/etc/localtime", ec); !ec) {
    if (const auto timezone = timezone_from_path(canonical); !timezone.empty()) {
      return timezone;
    }
  }

  return K_EMPTY_VALUE;
}

std::string read_rtc() {
#if defined(__linux__)
  if (const auto value = read_rtc_ioctl(); !value.empty()) {
    return value;
  }
  if (const auto value = read_rtc_sysfs(); !value.empty()) {
    return value;
  }
  if (const auto value = read_hwclock_command(); !value.empty()) {
    return value;
  }
#endif
  return K_EMPTY_VALUE;
}

}  // namespace

std::vector<DeviceInfoField> read_device_info_fields() {
  return {
      {"Model", read_device_model()},
      {"Hardware Revision", read_hardware_revision()},
      {"Serial Number", read_soc_serial_number()},
      {"RAM", read_ram()},
      {"Storage", read_storage()},
      {"MAC", read_mac()},
      {"OS", read_os()},
      {"Kernel", read_or_empty("/proc/sys/kernel/osrelease")},
      {"Uptime", read_uptime()},
      {"RTC", read_rtc()},
      {"Timezone", read_timezone()},
  };
}

}  // namespace platform::device_info
