/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "spi_device.h"

#include <cerrno>
#include <cstring>
#include <utility>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace platform::spi {
namespace {

std::string errno_message(const std::string& operation) {
  return operation + ": " + std::strerror(errno);
}

}  // namespace

Device::Device(int fd, DeviceConfig config)
    : fd_(fd),
      config_(std::move(config)) {}

Device::~Device() {
#if defined(__linux__)
  if (fd_ >= 0) {
    close(fd_);
  }
#endif
}

std::unique_ptr<Device> Device::open(const DeviceConfig& config, std::string& error_message) {
  error_message.clear();
#if defined(__linux__)
  if (config.path.empty()) {
    error_message = "SPI path is empty";
    return nullptr;
  }

  const int fd = ::open(config.path.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    error_message = errno_message("open " + config.path);
    return nullptr;
  }

  uint8_t mode = config.mode;
  if (config.no_chip_select) {
    mode |= SPI_NO_CS;
  }
  uint8_t bits      = config.bits_per_word;
  uint8_t lsb_first = config.lsb_first ? 1 : 0;
  uint32_t speed    = config.speed_hz;
  if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 || ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
      ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb_first) < 0 ||
      ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
    error_message = errno_message("configure " + config.path);
    close(fd);
    return nullptr;
  }

  uint8_t actual_mode = 0;
  if (ioctl(fd, SPI_IOC_RD_MODE, &actual_mode) < 0 ||
      (config.no_chip_select && (actual_mode & SPI_NO_CS) == 0)) {
    error_message = config.no_chip_select
                        ? "SPI controller did not accept SPI_NO_CS on " + config.path
                        : errno_message("read mode from " + config.path);
    close(fd);
    return nullptr;
  }

  return std::unique_ptr<Device>(new Device(fd, config));
#else
  (void)config;
  error_message = "Linux SPI backend is not available";
  return nullptr;
#endif
}

bool Device::transfer(const std::vector<uint8_t>& tx,
                      std::vector<uint8_t>& rx,
                      std::string& error_message) {
  error_message.clear();
  if (tx.empty()) {
    error_message = "SPI transfer buffer is empty";
    return false;
  }
#if defined(__linux__)
  if (fd_ < 0) {
    error_message = "SPI device is not open";
    return false;
  }

  rx.assign(tx.size(), 0);
  spi_ioc_transfer transfer{};
  transfer.tx_buf        = reinterpret_cast<__u64>(tx.data());
  transfer.rx_buf        = reinterpret_cast<__u64>(rx.data());
  transfer.len           = static_cast<__u32>(tx.size());
  transfer.speed_hz      = config_.speed_hz;
  transfer.bits_per_word = config_.bits_per_word;
  transfer.cs_change     = 0;
  if (ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) < 0) {
    error_message = errno_message("SPI_IOC_MESSAGE on " + config_.path);
    return false;
  }
  return true;
#else
  (void)rx;
  error_message = "Linux SPI backend is not available";
  return false;
#endif
}

}  // namespace platform::spi
