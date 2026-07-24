/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "i2c_service.h"

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

bool same_i2c_address(const I2cAddressInfo& left, const I2cAddressInfo& right) {
  return left.address == right.address && left.state == right.state;
}

bool same_i2c_result(const std::vector<I2cAddressInfo>& left,
                     const std::vector<I2cAddressInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_i2c_address);
}

struct I2cLogSnapshot {
  bool initialized{false};
  std::vector<I2cAddressInfo> addresses{};
  std::string error_message{};
};

bool update_i2c_log_snapshot(I2cLogSnapshot& snapshot,
                             const std::vector<I2cAddressInfo>& addresses,
                             const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_i2c_result(snapshot.addresses, addresses)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.addresses     = addresses;
  snapshot.error_message = error_message;
  return true;
}

void log_i2c_result(int bus_number,
                    const std::vector<I2cAddressInfo>& addresses,
                    const std::string& error_message,
                    bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity i2c{} result unchanged: {} address(es)", bus_number, addresses.size());
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity i2c{} result message: {}", bus_number, error_message);
  }

  std::size_t present_count = 0;
  std::size_t busy_count    = 0;
  for (const auto& address : addresses) {
    if (address.state == I2cAddressState::PRESENT) {
      ++present_count;
    } else if (address.state == I2cAddressState::KERNEL_DRIVER) {
      ++busy_count;
    }
  }
  LOG_INFO("connectivity i2c{} result: {} present, {} kernel-owned",
           bus_number,
           present_count,
           busy_count);
}

#if defined(__linux__)
struct FileDescriptorOwner {
  int fd{-1};

  ~FileDescriptorOwner() {
    if (fd >= 0) {
      close(fd);
    }
  }
};

std::string i2c_bus_device_path(int bus_number) {
  char buffer[32]{};
  std::snprintf(buffer, sizeof(buffer), "/dev/i2c-%d", bus_number);
  return buffer;
}

int i2c_smbus_access(int fd,
                     char read_write,
                     uint8_t command,
                     int size,
                     union i2c_smbus_data* data) {
  i2c_smbus_ioctl_data args{};
  args.read_write = read_write;
  args.command    = command;
  args.size       = size;
  args.data       = data;
  return ioctl(fd, I2C_SMBUS, &args);
}

bool i2c_smbus_read_byte(int fd) {
  union i2c_smbus_data data{};
  return i2c_smbus_access(fd, static_cast<char>(I2C_SMBUS_READ), 0, I2C_SMBUS_BYTE, &data) >= 0;
}

bool i2c_smbus_read_byte_data(int fd, uint8_t command, uint8_t& value) {
  union i2c_smbus_data data{};
  if (i2c_smbus_access(fd, static_cast<char>(I2C_SMBUS_READ), command, I2C_SMBUS_BYTE_DATA, &data) <
      0) {
    return false;
  }
  value = data.byte;
  return true;
}

I2cAddressState probe_i2c_address(int fd, uint8_t address) {
  if (ioctl(fd, I2C_SLAVE, address) < 0) {
    return errno == EBUSY ? I2cAddressState::KERNEL_DRIVER : I2cAddressState::ABSENT;
  }
  return i2c_smbus_read_byte(fd) ? I2cAddressState::PRESENT : I2cAddressState::ABSENT;
}
#endif

}  // namespace

std::vector<I2cAddressInfo> scan_i2c_bus(int bus_number, std::string& error_message) {
  static I2cLogSnapshot log_snapshot;
  error_message.clear();

#if defined(__linux__)
  LOG_TRACE("connectivity I2C bus {} scan requested", bus_number);
  const auto path = i2c_bus_device_path(bus_number);
  FileDescriptorOwner bus{open(path.c_str(), O_RDWR)};
  if (bus.fd < 0) {
    error_message = std::string("failed to open ") + path + ": " + std::strerror(errno);
    const std::vector<I2cAddressInfo> addresses;
    const bool changed = update_i2c_log_snapshot(log_snapshot, addresses, error_message);
    log_i2c_result(bus_number, addresses, error_message, changed);
    return {};
  }

  std::vector<I2cAddressInfo> addresses;
  addresses.reserve(0x78 - 0x03);
  for (uint8_t address = 0x03; address <= 0x77; ++address) {
    addresses.push_back({address, probe_i2c_address(bus.fd, address)});
  }

  const bool changed = update_i2c_log_snapshot(log_snapshot, addresses, error_message);
  log_i2c_result(bus_number, addresses, error_message, changed);
  return addresses;
#else
  LOG_TRACE("connectivity I2C bus {} scan requested but Linux I2C backend is unavailable",
            bus_number);
  error_message = "Linux I2C backend is not available";
  const std::vector<I2cAddressInfo> addresses;
  const bool changed = update_i2c_log_snapshot(log_snapshot, addresses, error_message);
  log_i2c_result(bus_number, addresses, error_message, changed);
  return {};
#endif
}

bool read_i2c_byte_data(int bus_number,
                        uint8_t address,
                        uint8_t command,
                        uint8_t& value,
                        std::string& error_message,
                        I2cAddressAccess access) {
  error_message.clear();
  if (bus_number < 0 || address > 0x7F) {
    error_message = "invalid I2C bus or address";
    return false;
  }

#if defined(__linux__)
  const auto path = i2c_bus_device_path(bus_number);
  FileDescriptorOwner bus{open(path.c_str(), O_RDWR)};
  if (bus.fd < 0) {
    error_message = std::string("failed to open ") + path + ": " + std::strerror(errno);
    return false;
  }

  if (ioctl(bus.fd, I2C_SLAVE, address) < 0) {
    const int select_error = errno;
    if (select_error != EBUSY || access != I2cAddressAccess::FORCE_IF_BUSY) {
      error_message = std::string("failed to select I2C address: ") + std::strerror(select_error);
      return false;
    }
    if (ioctl(bus.fd, I2C_SLAVE_FORCE, address) < 0) {
      error_message =
          std::string("failed to force-select busy I2C address: ") + std::strerror(errno);
      return false;
    }
  }
  if (!i2c_smbus_read_byte_data(bus.fd, command, value)) {
    error_message = std::string("failed to read I2C register: ") + std::strerror(errno);
    return false;
  }
  return true;
#else
  (void)bus_number;
  (void)address;
  (void)command;
  (void)value;
  (void)access;
  error_message = "Linux I2C backend is not available";
  return false;
#endif
}
}  // namespace platform::connectivity
