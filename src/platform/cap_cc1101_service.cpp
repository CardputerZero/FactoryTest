/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_cc1101_service.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#include "logger.h"
#include "spi_device.h"

namespace platform::cap_cc1101 {
namespace {

constexpr uint8_t K_CC_SRES        = 0x30;
constexpr uint8_t K_CC_SIDLE       = 0x36;
constexpr uint8_t K_CC_PARTNUM     = 0x30;
constexpr uint8_t K_CC_VERSION     = 0x31;
constexpr uint8_t K_CC_MARCSTATE   = 0x35;
constexpr uint8_t K_CC_PKTSTATUS   = 0x38;
constexpr uint8_t K_CC_STATUS_READ = 0xC0;
constexpr uint8_t K_CC_IDLE_STATE  = 0x01;

constexpr uint8_t K_ST_OPERATION_CONTROL = 0x02;
constexpr uint8_t K_ST_FIFO_STATUS1      = 0x1E;
constexpr uint8_t K_ST_FIFO_STATUS2      = 0x1F;
constexpr uint8_t K_ST_IDENTITY          = 0x3F;
constexpr uint8_t K_ST_REGISTER_READ     = 0x40;

std::string hex_byte(uint8_t value) {
  std::ostringstream stream;
  stream << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned int>(value);
  return stream.str();
}

std::string hex_bytes(const std::vector<uint8_t>& values) {
  std::ostringstream stream;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      stream << ' ';
    }
    stream << hex_byte(values[index]);
  }
  return stream.str();
}

class Cc1101Device {
 public:
  explicit Cc1101Device(std::unique_ptr<spi::Device> device)
      : device_(std::move(device)) {}

  bool strobe(uint8_t command, uint8_t* status, std::string& error) {
    const std::vector<uint8_t> tx{command};
    LOG_INFO("[CAP-CC1101][CC1101][SPI-TX] strobe={} bytes={}", hex_byte(command), hex_bytes(tx));
    std::vector<uint8_t> rx;
    if (!device_->transfer(tx, rx, error)) {
      LOG_ERROR("[CAP-CC1101][CC1101][SPI-RX] strobe={} failed: {}", hex_byte(command), error);
      return false;
    }
    if (rx.empty()) {
      error = "empty SPI response to strobe " + hex_byte(command);
      LOG_ERROR("[CAP-CC1101][CC1101][SPI-RX] {}", error);
      return false;
    }
    LOG_INFO("[CAP-CC1101][CC1101][SPI-RX] strobe={} bytes={}", hex_byte(command), hex_bytes(rx));
    if (status) {
      *status = rx[0];
    }
    return true;
  }

  bool read_status(uint8_t reg, uint8_t& value, std::string& error) {
    const std::vector<uint8_t> tx{static_cast<uint8_t>(reg | K_CC_STATUS_READ), 0x00};
    LOG_INFO("[CAP-CC1101][CC1101][SPI-TX] status-register={} bytes={}",
             hex_byte(reg),
             hex_bytes(tx));
    std::vector<uint8_t> rx;
    if (!device_->transfer(tx, rx, error)) {
      LOG_ERROR("[CAP-CC1101][CC1101][SPI-RX] status-register={} failed: {}", hex_byte(reg), error);
      return false;
    }
    if (rx.size() < 2) {
      error = "short SPI response from status register " + hex_byte(reg);
      LOG_ERROR("[CAP-CC1101][CC1101][SPI-RX] {}", error);
      return false;
    }
    LOG_INFO("[CAP-CC1101][CC1101][SPI-RX] status-register={} bytes={}",
             hex_byte(reg),
             hex_bytes(rx));
    value = rx[1];
    return true;
  }

 private:
  std::unique_ptr<spi::Device> device_;
};

bool probe_cc1101(const Cc1101Config& config,
                  const std::atomic_bool& cancel_requested,
                  const ProgressCallback& progress,
                  Cc1101Result& result) {
  std::string error;
  LOG_INFO("[CAP-CC1101][CC1101][INIT] path={} mode=0 speed={}Hz",
           config.spi_path,
           config.spi_speed_hz);
  auto device = spi::Device::open({config.spi_path, config.spi_speed_hz, 0, 8, false}, error);
  if (!device) {
    LOG_ERROR("[CAP-CC1101][CC1101][INIT] failed: {}", error);
    result.error_message = error;
    return false;
  }
  Cc1101Device radio(std::move(device));
  if (progress) {
    progress("Resetting CC1101 and reading identity/status");
  }
  if (!radio.strobe(K_CC_SRES, nullptr, error)) {
    result.error_message = error;
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  if (cancel_requested.load()) {
    result.error_message = "test canceled";
    return false;
  }
  if (!radio.strobe(K_CC_SIDLE, &result.chip_status, error) ||
      !radio.read_status(K_CC_PARTNUM, result.part_number, error) ||
      !radio.read_status(K_CC_VERSION, result.version, error) ||
      !radio.read_status(K_CC_MARCSTATE, result.marc_state, error) ||
      !radio.read_status(K_CC_PKTSTATUS, result.packet_status, error)) {
    result.error_message = error;
    return false;
  }
  result.detected = result.part_number == 0x00 &&
                    (result.version == 0x04 || result.version == 0x14 || result.version == 0x17);
  if (!result.detected) {
    result.error_message = "unexpected PARTNUM=" + hex_byte(result.part_number) +
                           " VERSION=" + hex_byte(result.version);
    return false;
  }
  if ((result.chip_status & 0x80U) != 0 || (result.marc_state & 0x1FU) != K_CC_IDLE_STATE) {
    result.error_message = "unexpected status=" + hex_byte(result.chip_status) +
                           " MARCSTATE=" + hex_byte(result.marc_state);
    return false;
  }
  result.initialized = true;
  LOG_INFO("[CAP-CC1101][CC1101][INIT] complete");
  LOG_INFO("[CAP-CC1101] CC1101 PARTNUM={} VERSION={} STATUS={} MARCSTATE={} PKTSTATUS={}",
           hex_byte(result.part_number),
           hex_byte(result.version),
           hex_byte(result.chip_status),
           hex_byte(result.marc_state),
           hex_byte(result.packet_status));
  return true;
}

bool transfer_with_software_cs(spi::Device& device,
                               gpio::OutputLine& chip_select,
                               const std::vector<uint8_t>& tx,
                               std::vector<uint8_t>& rx,
                               std::string& error) {
  if (!chip_select.set_value(false, error)) {
    error = "assert software CS: " + error;
    return false;
  }

  const bool transfer_ok = device.transfer(tx, rx, error);
  std::string deassert_error;
  if (!chip_select.set_value(true, deassert_error)) {
    if (!error.empty()) {
      error += " | ";
    }
    error += "deassert software CS: " + deassert_error;
    return false;
  }
  return transfer_ok;
}

bool read_st_register(spi::Device& device,
                      gpio::OutputLine& chip_select,
                      uint8_t reg,
                      uint8_t& value,
                      std::string& error) {
  const std::vector<uint8_t> tx{static_cast<uint8_t>(K_ST_REGISTER_READ | reg), 0x00};
  LOG_INFO("[CAP-CC1101][ST25R3916][SPI-TX] register={} bytes={}", hex_byte(reg), hex_bytes(tx));
  std::vector<uint8_t> rx;
  if (!transfer_with_software_cs(device, chip_select, tx, rx, error)) {
    LOG_ERROR("[CAP-CC1101][ST25R3916][SPI-RX] register={} failed: {}", hex_byte(reg), error);
    return false;
  }
  if (rx.size() < 2) {
    error = "short SPI response from register " + hex_byte(reg);
    LOG_ERROR("[CAP-CC1101][ST25R3916][SPI-RX] {}", error);
    return false;
  }
  LOG_INFO("[CAP-CC1101][ST25R3916][SPI-RX] register={} bytes={}", hex_byte(reg), hex_bytes(rx));
  value = rx[1];
  return true;
}

}  // namespace

Cc1101Result run_cc1101_test(const Cc1101Config& config,
                             const std::atomic_bool& cancel_requested,
                             const ProgressCallback& progress) {
  Cc1101Result result;
  probe_cc1101(config, cancel_requested, progress, result);
  return result;
}

St25r3916Result run_st25r3916_test(const St25r3916Config& config,
                                   const std::atomic_bool& cancel_requested,
                                   const ProgressCallback& progress) {
  St25r3916Result result;
  std::string error;
  LOG_INFO("[CAP-CC1101][ST25R3916][INIT] path={} mode=1 speed={}Hz software-cs={}:{}",
           config.spi_path,
           config.spi_speed_hz,
           config.software_cs.chip_path,
           config.software_cs.line_offset);
  gpio::OutputLine chip_select(config.software_cs);
  if (!chip_select.set_value(true, error)) {
    LOG_ERROR("[CAP-CC1101][ST25R3916][INIT] software CS failed: {}", error);
    result.error_message = "initialize GPIO22 software CS: " + error;
    return result;
  }

  auto device = spi::Device::open({config.spi_path, config.spi_speed_hz, 1, 8, false, true}, error);
  if (!device) {
    LOG_ERROR("[CAP-CC1101][ST25R3916][INIT] failed: {}", error);
    result.error_message = error;
    return result;
  }
  if (progress) {
    progress("Reading ST25R3916 identity/status registers");
  }
  if (!read_st_register(*device, chip_select, K_ST_IDENTITY, result.identity, error) ||
      !read_st_register(*device,
                        chip_select,
                        K_ST_OPERATION_CONTROL,
                        result.operation_control,
                        error) ||
      !read_st_register(*device, chip_select, K_ST_FIFO_STATUS1, result.fifo_status1, error) ||
      !read_st_register(*device, chip_select, K_ST_FIFO_STATUS2, result.fifo_status2, error) ||
      !read_st_register(*device, chip_select, K_ST_IDENTITY, result.identity_readback, error)) {
    result.error_message = error;
    return result;
  }
  if (cancel_requested.load()) {
    result.error_message = "test canceled";
    return result;
  }
  result.detected               = (result.identity >> 3U) == 0x05;
  result.communication_verified = result.detected && result.identity_readback == result.identity;
  if (!result.communication_verified) {
    result.error_message = result.detected
                               ? "identity readback mismatch: " + hex_byte(result.identity_readback)
                               : "unexpected IC identity " + hex_byte(result.identity);
    return result;
  }
  LOG_INFO("[CAP-CC1101][ST25R3916][INIT] complete");
  LOG_INFO("[CAP-CC1101] ST25R3916 ID={} OP_CONTROL={} FIFO_STATUS1={} FIFO_STATUS2={}",
           hex_byte(result.identity),
           hex_byte(result.operation_control),
           hex_byte(result.fifo_status1),
           hex_byte(result.fifo_status2));
  return result;
}

}  // namespace platform::cap_cc1101
