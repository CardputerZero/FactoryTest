/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "spi_service.h"

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

bool same_spi_device(const SpiDeviceInfo& left, const SpiDeviceInfo& right) {
  return left.name == right.name && left.path == right.path;
}

bool same_spi_result(const std::vector<SpiDeviceInfo>& left,
                     const std::vector<SpiDeviceInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_spi_device);
}

struct SpiLogSnapshot {
  bool initialized{false};
  std::vector<SpiDeviceInfo> devices{};
  std::string error_message{};
};

bool update_spi_log_snapshot(SpiLogSnapshot& snapshot,
                             const std::vector<SpiDeviceInfo>& devices,
                             const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_spi_result(snapshot.devices, devices)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.devices       = devices;
  snapshot.error_message = error_message;
  return true;
}

void log_spi_result(const std::vector<SpiDeviceInfo>& devices,
                    const std::string& error_message,
                    bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity spi result unchanged: {} device(s)", devices.size());
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity spi result message: {}", error_message);
  }

  LOG_INFO("connectivity spi result: {} device(s)", devices.size());
  for (const auto& device : devices) {
    LOG_INFO("connectivity spi device: name='{}' path='{}'", device.name, device.path);
  }
}


bool is_spidev_name(const std::string& name) {
  constexpr const char* PREFIX = "spidev";
  return name.rfind(PREFIX, 0) == 0 && name.size() > std::strlen(PREFIX);
}

}  // namespace

std::vector<SpiDeviceInfo> list_spi_devices(std::string& error_message) {
  static SpiLogSnapshot log_snapshot;
  error_message.clear();

#if defined(__linux__)
  LOG_TRACE("connectivity SPI device enumeration requested");
  std::vector<SpiDeviceInfo> devices;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator("/dev", ec)) {
    const auto name = entry.path().filename().string();
    if (!is_spidev_name(name)) {
      continue;
    }

    devices.push_back({name, entry.path().string()});
  }

  if (ec) {
    error_message = std::string("failed to enumerate /dev: ") + ec.message();
  }
  std::sort(devices.begin(), devices.end(), [](const auto& left, const auto& right) {
    return left.name < right.name;
  });
  if (devices.empty() && error_message.empty()) {
    error_message = "No SPI devices found";
  }

  const bool changed = update_spi_log_snapshot(log_snapshot, devices, error_message);
  log_spi_result(devices, error_message, changed);
  return devices;
#else
  LOG_TRACE("connectivity SPI device enumeration requested but Linux backend is unavailable");
  error_message = "Linux SPI backend is not available";
  const std::vector<SpiDeviceInfo> devices;
  const bool changed = update_spi_log_snapshot(log_snapshot, devices, error_message);
  log_spi_result(devices, error_message, changed);
  return {};
#endif
}
}  // namespace platform::connectivity
