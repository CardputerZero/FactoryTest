/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_fixture_test_model.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <thread>
#include <vector>

#include "cap_fixture_service.h"
#include "gpio_service.h"
#include "logger.h"
#include "uart_service.h"

namespace model {
namespace {

constexpr std::size_t K_POWER_ITEM      = 0;
constexpr std::size_t K_I2C_ITEM        = 1;
constexpr std::size_t K_GROVE_I2C_ITEM  = 2;
constexpr std::size_t K_UART_ITEM       = 3;
constexpr std::size_t K_GROVE_UART_ITEM = 4;
constexpr std::size_t K_SPI_ITEM        = 5;
constexpr std::size_t K_USB_ITEM        = 6;
constexpr std::size_t K_GPIO_ITEM       = 7;

constexpr uint16_t K_USB_VENDOR_ID  = 0x303A;
constexpr uint16_t K_USB_PRODUCT_ID = 0x4005;

constexpr unsigned int K_FEATURE_GUARD_DELAY_MS             = 100;
constexpr unsigned int K_COMMAND_DELAY_MS                   = 100;
constexpr unsigned int K_SOFTWARE_RESET_DELAY_MS            = 2000;
constexpr unsigned int K_SOFTWARE_RESET_RETRY_MS            = 200;
constexpr unsigned int K_SOFTWARE_RESET_ATTEMPTS            = 5;
constexpr unsigned int K_CHANNEL_SWITCH_DELAY_MS            = 200;
constexpr unsigned int K_CHANNEL_SWITCH_TIMEOUT_MS          = 2000;
// Grove UART only needs a short electrical settle after CAP reports IDLE.
constexpr unsigned int K_GROVE_UART_SIGNAL_STABLE_DELAY_MS = 200;
constexpr unsigned int K_UART_COMMAND_READY_TIMEOUT_MS      = 1000;
constexpr unsigned int K_UART_COMMAND_READY_POLL_DELAY_MS  = 100;
constexpr unsigned int K_GROVE_I2C_SWITCH_DELAY_MS         = 3000;
constexpr unsigned int K_FIXTURE_POWER_SETTLE_MS           = 300;
constexpr unsigned int K_SYSFS_SWITCH_DELAY_MS             = 150;
constexpr unsigned int K_USB_FUNCTION_SETTLE_DELAY_MS      = 500;
constexpr unsigned int K_USB_INITIAL_ENUM_TIMEOUT_MS        = 6000;
constexpr unsigned int K_USB_RECOVERY_ENUM_TIMEOUT_MS       = 6000;
constexpr unsigned int K_USB_HUB_RESET_SETTLE_DELAY_MS      = 500;
constexpr unsigned int K_INTER_TEST_DELAY_MS               = 1000;
constexpr unsigned int K_GROVE_I2C_TO_UART_DELAY_MS        = 1500;
constexpr unsigned int K_GPIO_HOLD_REFRESH_MS              = 10;
constexpr std::size_t K_GPIO_SAMPLE_COUNT                  = 5;
constexpr unsigned int K_GPIO_SAMPLE_INTERVAL_MS           = 100;

std::string hex_bytes(const uint8_t* data, std::size_t size) {
  std::ostringstream output;
  output << std::hex << std::uppercase;
  for (std::size_t i = 0; i < size; ++i) {
    if (i > 0) {
      output << ' ';
    }
    output.width(2);
    output.fill('0');
    output << static_cast<unsigned int>(data[i]);
  }
  return output.str();
}

std::string hex_bytes(const std::vector<uint8_t>& data) {
  return hex_bytes(data.data(), data.size());
}

uint16_t crc16_modbus(const uint8_t* data, std::size_t size) {
  uint16_t crc = 0xFFFF;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1U) != 0 ? static_cast<uint16_t>((crc >> 1U) ^ 0xA001U)
                            : static_cast<uint16_t>(crc >> 1U);
    }
  }
  return crc;
}

std::array<uint8_t, 16> make_uart_frame(uint8_t type,
                                        const std::array<uint8_t, 10>& payload,
                                        uint8_t payload_size) {
  std::array<uint8_t, 16> frame{};
  frame[0] = 0xA5;
  frame[1] = 0x5A;
  frame[2] = type;
  frame[3] = payload_size;
  std::copy(payload.begin(), payload.end(), frame.begin() + 4);
  const auto crc = crc16_modbus(frame.data(), 14);
  frame[14]      = static_cast<uint8_t>(crc & 0xFFU);
  frame[15]      = static_cast<uint8_t>((crc >> 8U) & 0xFFU);
  return frame;
}

bool validate_uart_frame(const std::array<uint8_t, 16>& frame,
                         uint8_t expected_type,
                         uint8_t expected_length,
                         const std::array<uint8_t, 10>& expected_payload,
                         std::string& error) {
  if (frame[0] != 0xA5 || frame[1] != 0x5A) {
    error = "invalid UART frame header";
    return false;
  }
  if (frame[2] != expected_type || frame[3] != expected_length) {
    error = "unexpected UART frame type or payload length";
    return false;
  }
  if (!std::equal(expected_payload.begin(), expected_payload.end(), frame.begin() + 4)) {
    error = "UART payload mismatch";
    return false;
  }
  const auto expected_crc = crc16_modbus(frame.data(), 14);
  const auto received_crc = static_cast<uint16_t>(frame[14]) |
                            static_cast<uint16_t>(static_cast<uint16_t>(frame[15]) << 8U);
  if (received_crc != expected_crc) {
    error = "UART CRC mismatch";
    return false;
  }
  return true;
}

bool read_uart_frame(platform::connectivity::UartDebugSession& uart,
                     std::array<uint8_t, 16>& frame,
                     std::chrono::milliseconds timeout,
                     const std::atomic_bool& cancel_requested,
                     std::string& error) {
  std::string received;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested.load()) {
      error = "UART receive canceled";
      return false;
    }
    auto chunk = uart.read_available(error);
    if (!error.empty()) {
      return false;
    }
    if (!chunk.empty()) {
      // Keep the raw stream visible, including fragmented/non-protocol bytes,
      // before the frame parser consumes it.
      LOG_VERBOSE("[CAP-FIXTURE][UART] RX buffer chunk len={} total={} data=[{}]",
               chunk.size(),
               received.size() + chunk.size(),
               hex_bytes(reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size()));
    }
    received.append(chunk);

    // The fixture can leave zero-filled bytes on RX while the channel is
    // switching. Synchronize on the protocol header instead of assuming the
    // first received byte is the start of a frame.
    const auto header = received.find("\xA5\x5A");
    if (header != std::string::npos && received.size() - header >= frame.size()) {
      std::copy_n(reinterpret_cast<const uint8_t*>(received.data() + header),
                  frame.size(),
                  frame.begin());
      if (header > 0) {
        LOG_VERBOSE("[CAP-FIXTURE][UART] discarded {} byte(s) before frame header", header);
      }
      if (received.size() - header > frame.size()) {
        LOG_VERBOSE("[CAP-FIXTURE][UART] received {} extra byte(s) after frame",
                 received.size() - header - frame.size());
      }
      return true;
    }

    if (header == std::string::npos && received.size() > 1) {
      // Retain a possible split 0xA5 header byte for the next read.
      const auto discarded = received.size() - 1;
      LOG_VERBOSE("[CAP-FIXTURE][UART] discarded {} byte(s) without frame header", discarded);
      received.erase(0, discarded);
    }
    if (received.empty() || header == std::string::npos ||
        received.size() - header < frame.size()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  error = "UART receive timed out waiting for 16-byte frame (buffered " +
          std::to_string(received.size()) + " byte(s))";
  return false;
}

}  // namespace

CapFixtureTestModel::CapFixtureTestModel() {
  snapshot_.items = {{
      {"Power", CapFixtureItemState::PENDING, "Waiting"},
      {"I2C", CapFixtureItemState::PENDING, "Waiting"},
      {"Grove I2C", CapFixtureItemState::PENDING, "Waiting"},
      {"UART", CapFixtureItemState::PENDING, "Waiting"},
      {"Grove UART", CapFixtureItemState::PENDING, "Waiting"},
      {"SPI", CapFixtureItemState::PENDING, "Waiting"},
      {"USB", CapFixtureItemState::PENDING, "Waiting"},
      {"GPIO", CapFixtureItemState::PENDING, "Waiting"},
  }};
}

CapFixtureTestModel::~CapFixtureTestModel() {
  cancel();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void CapFixtureTestModel::start() {
  if (worker_.joinable()) {
    return;
  }
  cancel_requested_.store(false);
  publish_(CapFixtureRunState::RUNNING, "Resetting CAP fixture");
  LOG_DEBUG(
      "[CAP-FIXTURE][RUN] starting Zero fixture sequence: EXT 5V -> Software Reset -> Power -> "
      "I2C -> Grove I2C -> UART -> Grove UART -> SPI -> USB -> GPIO");
  worker_ = std::thread(&CapFixtureTestModel::run_, this);
}

void CapFixtureTestModel::cancel() {
  const auto current = snapshot();
  if (current.state != CapFixtureRunState::RUNNING) {
    return;
  }
  if (!cancel_requested_.exchange(true)) {
    LOG_WARN("[CAP-FIXTURE][RUN] cancellation requested");
  }
}

CapFixtureSnapshot CapFixtureTestModel::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

void CapFixtureTestModel::run_() {
  std::string error;
  LOG_DEBUG("[CAP-FIXTURE][PREPARE] keeping I2C on HAT: grove_fun=0 before reset");
  if (!platform::gpio::set_external_bus_uart_mode(error)) {
    fail_("PREPARE", "failed to keep I2C on HAT: " + error);
    cleanup_();
    return;
  }
  host_external_uart_selected_ = true;
  LOG_DEBUG("[CAP-FIXTURE][PREPARE] ensuring EXT 5V output before reset: {}=1",
           platform::fixture::K_EXT_5V_PATH);
  if (!platform::fixture::set_ext_5v_enabled(true, error)) {
    fail_("PREPARE", "failed to enable EXT 5V before reset: " + error);
    cleanup_();
    return;
  }
  if (!delay_("PREPARE",
              K_FIXTURE_POWER_SETTLE_MS,
              "EXT 5V power-on and CAP I2C readiness before reset")) {
    cleanup_();
    return;
  }

  LOG_DEBUG("[CAP-FIXTURE][RESET] sending software reset command");
  std::string reset_error;
  bool reset_sent = false;
  for (unsigned int attempt = 1; attempt <= K_SOFTWARE_RESET_ATTEMPTS; ++attempt) {
    if (cancel_requested_.load()) {
      break;
    }
    LOG_VERBOSE("[CAP-FIXTURE][RESET] I2C TX attempt {}/{} addr=0x42 reg=0x01 data=[07]",
             attempt,
             K_SOFTWARE_RESET_ATTEMPTS);
    if (platform::fixture::write_register(0x01, 0x07, reset_error)) {
      reset_sent = true;
      LOG_DEBUG("[CAP-FIXTURE][RESET] software reset command accepted on attempt {}/{}",
               attempt,
               K_SOFTWARE_RESET_ATTEMPTS);
      break;
    }
    LOG_WARN("[CAP-FIXTURE][RESET] I2C software reset attempt {}/{} failed: {}",
             attempt,
             K_SOFTWARE_RESET_ATTEMPTS,
             reset_error);
    if (attempt < K_SOFTWARE_RESET_ATTEMPTS &&
        !delay_("RESET", K_SOFTWARE_RESET_RETRY_MS, "retry software reset I2C write")) {
      break;
    }
  }
  if (!reset_sent) {
    if (!cancel_requested_.load()) {
      fail_("RESET",
            "I2C software reset failed after " + std::to_string(K_SOFTWARE_RESET_ATTEMPTS) +
                " attempts: " + reset_error);
    }
    cleanup_();
    return;
  }
  LOG_DEBUG("[CAP-FIXTURE][RESET] fixed wait {} ms for CAP software restart; no I2C polling",
           K_SOFTWARE_RESET_DELAY_MS);
  if (!delay_("RESET", K_SOFTWARE_RESET_DELAY_MS, "CAP fixture software restart")) {
    cleanup_();
    return;
  }

  LOG_DEBUG(
      "[CAP-FIXTURE][PREPARE] RESET and Grove channel transitions use fixed waits; "
      "protocol-required UART readiness confirmation remains active");

  const bool passed =
      run_power_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "Power -> I2C guard") &&
      run_i2c_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "I2C -> Grove I2C guard") &&
      run_grove_i2c_() &&
      delay_("SEQUENCE", K_GROVE_I2C_TO_UART_DELAY_MS, "Grove I2C -> UART guard") &&
      run_uart_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "UART -> Grove UART guard") &&
      run_grove_uart_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "Grove UART -> SPI guard") &&
      run_spi_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "SPI -> USB guard") && run_usb_() &&
      delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "USB -> GPIO guard") && run_gpio_();
  cleanup_();
  if (cancel_requested_.load()) {
    publish_(CapFixtureRunState::CANCELED, "Fixture test canceled");
    LOG_WARN("[CAP-FIXTURE][RUN] sequence canceled after cleanup");
  } else if (passed) {
    publish_(CapFixtureRunState::PASSED, "All fixture tests passed");
    LOG_INFO("[CAP-FIXTURE][RUN] PASS: all eight Zero fixture tests completed");
  }
}

bool CapFixtureTestModel::run_i2c_() {
  constexpr const char* STAGE = "I2C";
  begin_item_(K_I2C_ITEM, "Register read/write");
  std::array<uint8_t, 2> original{};
  if (!read_reg_(STAGE, 0x02, original.data(), original.size())) {
    return false;
  }
  const std::array<uint8_t, 2> test_value{{0xA5, 0x5A}};
  if (!write_reg_(STAGE, 0x02, test_value.data(), test_value.size()) ||
      !delay_(STAGE, 20, "I2C test register write settle")) {
    return false;
  }
  std::array<uint8_t, 2> readback{};
  if (!read_reg_(STAGE, 0x02, readback.data(), readback.size())) {
    return false;
  }
  if (readback != test_value) {
    return fail_(STAGE, "register 0x02 mismatch: " + hex_bytes(readback.data(), readback.size()));
  }
  const std::array<uint8_t, 2> restore{{0x0F, 0xF0}};
  if (!write_reg_(STAGE, 0x02, restore.data(), restore.size()) || !write_reg_(STAGE, 0x01, 0x02) ||
      !delay_(STAGE, K_COMMAND_DELAY_MS, "CAP I2C command processing")) {
    return false;
  }
  uint8_t i2c_status = 0xFF;
  if (!read_reg_(STAGE, 0x03, &i2c_status, 1) || i2c_status != 0x02) {
    return fail_(STAGE, "CAP I2C command did not report PASS");
  }
  pass_item_(K_I2C_ITEM, "0xA55A verified");
  LOG_INFO("[CAP-FIXTURE][I2C] PASS: repeated-start read and register write verified");
  return true;
}

bool CapFixtureTestModel::run_grove_i2c_() {
  constexpr const char* STAGE = "GROVE-I2C";
  begin_item_(K_GROVE_I2C_ITEM, "Switch and verify Grove I2C");
  std::string error;
  LOG_DEBUG("[CAP-FIXTURE][{}] request CAP I2C channel toggle HAT -> Grove", STAGE);
  if (!write_reg_(STAGE, 0x01, 0x08)) {
    return false;
  }
  cap_grove_i2c_active_ = true;
  if (!delay_(STAGE, 200, "CAP Grove I2C channel reinitialization")) {
    return false;
  }
  if (!platform::gpio::set_external_bus_i2c_mode(true, error)) {
    return fail_(STAGE, "Grove I2C host mux switch failed: " + error);
  }
  host_external_uart_selected_ = false;
  if (!delay_(STAGE,
              K_GROVE_I2C_SWITCH_DELAY_MS,
              "Grove I2C host bus settle (fixed 500 ms; no status polling)")) {
    return false;
  }
  if (!write_reg_(STAGE, 0x01, 0x02) ||
      !delay_(STAGE, K_COMMAND_DELAY_MS, "Grove I2C command processing")) {
    return false;
  }
  uint8_t grove_status = 0xFF;
  if (!read_reg_(STAGE, 0x03, &grove_status, 1) || grove_status != 0x02) {
    return fail_(STAGE, "CAP Grove I2C command did not report PASS");
  }
  LOG_DEBUG("[CAP-FIXTURE][{}] Grove I2C command accepted; toggling CAP back to HAT", STAGE);
  if (!write_reg_(STAGE, 0x01, 0x08) ||
      !delay_(STAGE, 200, "CAP HAT I2C channel reinitialization") ||
      !delay_(STAGE,
              K_CHANNEL_SWITCH_DELAY_MS,
              "HAT I2C host bus settle (fixed; no status polling)")) {
    return false;
  }
  cap_grove_i2c_active_ = false;
  if (!platform::gpio::set_external_bus_uart_mode(error) ||
      !delay_(STAGE, K_SYSFS_SWITCH_DELAY_MS, "keep I2C on HAT after Grove I2C")) {
    return fail_(STAGE, "failed to restore grove_fun=0 after Grove I2C: " + error);
  }
  uint8_t hat_channel_status = 0xFF;
  const auto hat_channel_deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(K_CHANNEL_SWITCH_TIMEOUT_MS);
  while (std::chrono::steady_clock::now() < hat_channel_deadline) {
    if (!read_reg_(STAGE, 0x03, &hat_channel_status, 1)) {
      return false;
    }
    if (hat_channel_status == 0x00) {
      break;
    }
    if (!delay_(STAGE, 50, "waiting for CAP HAT I2C channel to return IDLE")) {
      return false;
    }
  }
  if (hat_channel_status != 0x00) {
    return fail_(STAGE, "CAP HAT I2C channel switch did not return IDLE");
  }
  LOG_DEBUG("[CAP-FIXTURE][{}] CAP I2C channel confirmed on HAT (status=IDLE)", STAGE);
  host_external_uart_selected_ = true;
  pass_item_(K_GROVE_I2C_ITEM, "Channel switch + command OK");
  LOG_INFO("[CAP-FIXTURE][{}] PASS", STAGE);
  return true;
}

bool CapFixtureTestModel::run_power_() {
  constexpr const char* STAGE = "POWER";
  begin_item_(K_POWER_ITEM, "5VIN / 5VOUT");
  std::string error;
  LOG_DEBUG("[CAP-FIXTURE][POWER] ensure EXT 5V output is enabled");
  if (!platform::fixture::set_ext_5v_enabled(true, error)) {
    return fail_(STAGE, "EXT 5V enable failed: " + error);
  }
  if (!delay_(STAGE, K_SYSFS_SWITCH_DELAY_MS, "EXT 5V rail settle") ||
      !write_reg_(STAGE, 0x01, 0x01) || !delay_(STAGE, 200, "CAP 5VIN enable settle")) {
    return false;
  }
  uint8_t charge_report = 0;
  std::string charge_detail;
  if (!platform::fixture::read_charge_report(charge_report, charge_detail, error)) {
    return fail_(STAGE, "power_supply read failed: " + error);
  }
  LOG_DEBUG("[CAP-FIXTURE][POWER] power_supply {} -> charge report 0x{:02X}",
           charge_detail,
           charge_report);
  if (!write_reg_(STAGE, 0x0F, charge_report) ||
      !delay_(STAGE, 100, "CAP charge report processing")) {
    return false;
  }
  uint8_t low  = 0;
  uint8_t high = 0;
  if (!read_reg_(STAGE, 0x08, &low, 1) || !read_reg_(STAGE, 0x09, &high, 1)) {
    return false;
  }
  const unsigned int millivolts =
      static_cast<unsigned int>(high) | (static_cast<unsigned int>(low) << 8U);
  LOG_VERBOSE(
      "[CAP-FIXTURE][POWER] locked Zero 5VOUT voltage={} mV (reg08=LSB, reg09=MSB) "
      "threshold=4500 mV",
      millivolts);
  if (millivolts <= 4500) {
    return fail_(STAGE, "5VOUT is below threshold: " + std::to_string(millivolts) + " mV");
  }
  pass_item_(K_POWER_ITEM, (std::to_string(millivolts) + " mV").c_str());
  LOG_INFO("[CAP-FIXTURE][POWER] PASS: charge report sent and 5VOUT is valid");
  return true;
}

bool CapFixtureTestModel::run_uart_() {
  constexpr const char* STAGE = "UART";
  begin_item_(K_UART_ITEM, "115200 8N1 handshake");
  std::string error;
  if (!prepare_uart_test_(STAGE)) {
    return false;
  }

  LOG_DEBUG("[CAP-FIXTURE][UART] reusing host UART session opened after software reset");
  LOG_DEBUG("[CAP-FIXTURE][UART] CAP UART command is RUNNING; sending HELLO");

  LOG_DEBUG("[CAP-FIXTURE][UART] CAP HAT UART uses a dedicated path; Grove mux GPIO4 is unchanged");
  const std::array<uint8_t, 10> zeros{};
  const auto hello = make_uart_frame(0x30, zeros, 0);
  const auto start = make_uart_frame(0x32, zeros, 0);
  std::array<uint8_t, 16> ready{};
  bool ready_received = false;
  for (int attempt = 1; attempt <= 3 && !cancel_requested_.load(); ++attempt) {
    LOG_VERBOSE("[CAP-FIXTURE][UART] HELLO attempt {}/3 TX={}",
             attempt,
             hex_bytes(hello.data(), hello.size()));
    if (!uart_session_->write_text(
            std::string(reinterpret_cast<const char*>(hello.data()), hello.size()), error)) {
      return fail_(STAGE, "HELLO write failed: " + error);
    }
    error.clear();
    if (!read_uart_frame(*uart_session_,
                         ready,
                         std::chrono::milliseconds(700),
                         cancel_requested_,
                         error)) {
      LOG_WARN("[CAP-FIXTURE][UART] HELLO attempt {} did not receive READY: {}", attempt, error);
      continue;
    }
    LOG_VERBOSE("[CAP-FIXTURE][UART] READY RX={}", hex_bytes(ready.data(), ready.size()));
    if (!validate_uart_frame(ready, 0x31, 0, zeros, error)) {
      return fail_(STAGE, "invalid READY frame: " + error);
    }
    ready_received = true;
    break;
  }
  if (!ready_received) {
    return fail_(STAGE, "READY not received after three HELLO attempts");
  }

  LOG_VERBOSE("[CAP-FIXTURE][UART] START TX={}", hex_bytes(start.data(), start.size()));
  if (!uart_session_->write_text(
          std::string(reinterpret_cast<const char*>(start.data()), start.size()), error)) {
    return fail_(STAGE, "START write failed: " + error);
  }
  const std::array<uint8_t, 10> tx_payload{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  const auto data = make_uart_frame(0x03, tx_payload, 10);
  LOG_VERBOSE("[CAP-FIXTURE][UART] DATA TX={}", hex_bytes(data.data(), data.size()));
  if (!uart_session_->write_text(
          std::string(reinterpret_cast<const char*>(data.data()), data.size()), error)) {
    return fail_(STAGE, "DATA write failed: " + error);
  }
  std::array<uint8_t, 16> reply{};
  if (!read_uart_frame(*uart_session_,
                       reply,
                       std::chrono::milliseconds(3500),
                       cancel_requested_,
                       error)) {
    return fail_(STAGE, error);
  }
  LOG_VERBOSE("[CAP-FIXTURE][UART] DATA RX={}", hex_bytes(reply.data(), reply.size()));
  const std::array<uint8_t, 10> rx_payload{{9, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  if (!validate_uart_frame(reply, 0x03, 10, rx_payload, error)) {
    return fail_(STAGE, error);
  }
  if (!delay_(STAGE, K_FEATURE_GUARD_DELAY_MS, "UART-to-I2C traffic guard")) return false;
  std::array<uint8_t, 10> cap_received{};
  std::array<uint8_t, 10> cap_replied{};
  if (!read_reg_(STAGE, 0x11, cap_received.data(), cap_received.size()) ||
      !read_reg_(STAGE, 0x1B, cap_replied.data(), cap_replied.size())) {
    return false;
  }
  pass_item_(K_UART_ITEM, "Handshake + payload OK");
  LOG_INFO("[CAP-FIXTURE][UART] PASS: handshake, CRC and reversed payload verified");
  return true;
}

bool CapFixtureTestModel::run_grove_uart_() {
  constexpr const char* STAGE = "GROVE-UART";
  begin_item_(K_GROVE_UART_ITEM, "Switch and verify Grove UART");
  std::string error;
  LOG_DEBUG("[CAP-FIXTURE][{}] request CAP UART channel toggle HAT -> Grove", STAGE);
  // Release the host Grove I2C controller before asking CAP to move the
  // physical Grove connector to UART. A connected I2C peripheral may hold
  // SDA/SCL low while the analog switch is changing; control I2C remains on
  // the dedicated HAT pins and is unaffected by this host-side mux change.
  if (!platform::gpio::set_external_bus_uart_mode(error)) {
    return fail_(STAGE, "Grove UART host mux pre-switch failed: " + error);
  }
  host_external_uart_selected_ = true;
  if (!delay_(STAGE, K_SYSFS_SWITCH_DELAY_MS, "release Grove I2C host bus")) {
    return false;
  }
  if (!write_reg_(STAGE, 0x01, 0x09)) {
    return false;
  }
  cap_grove_uart_active_ = true;
  LOG_DEBUG("[CAP-FIXTURE][{}] waiting for CAP status IDLE before Grove UART signal settle",
           STAGE);
  if (!delay_(STAGE, 200, "CAP Grove UART channel reinitialization")) {
    return false;
  }
  const auto channel_deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(K_CHANNEL_SWITCH_DELAY_MS);
  uint8_t channel_status = 0xFF;
  while (std::chrono::steady_clock::now() < channel_deadline) {
    if (!read_reg_(STAGE, 0x03, &channel_status, 1)) {
      return false;
    }
    if (channel_status == 0x00) {
      break;
    }
    if (!delay_(STAGE, 50, "waiting for CAP UART channel switch")) {
      return false;
    }
  }
  if (channel_status != 0x00) {
    return fail_(STAGE, "CAP UART channel switch did not return IDLE");
  }

  // Only the CAP-side UART channel moved; wait for the physical signal path to
  // stabilize before opening the host UART and starting protocol traffic.
  if (!delay_(STAGE,
              K_GROVE_UART_SIGNAL_STABLE_DELAY_MS,
              "Grove UART signal path stabilization before protocol traffic")) {
    return false;
  }
  if (!prepare_uart_test_(STAGE)) {
    return false;
  }
  LOG_DEBUG("[CAP-FIXTURE][{}] host UART session initialized after CAP status IDLE", STAGE);
  const std::array<uint8_t, 10> zeros{};
  const auto hello = make_uart_frame(0x30, zeros, 0);
  const auto start = make_uart_frame(0x32, zeros, 0);
  std::array<uint8_t, 16> frame{};
  bool ready = false;
  for (int attempt = 1; attempt <= 3 && !cancel_requested_.load(); ++attempt) {
    LOG_DEBUG("[CAP-FIXTURE][{}] HELLO attempt {}/3", STAGE, attempt);
    if (!uart_session_->write_text(
            std::string(reinterpret_cast<const char*>(hello.data()), hello.size()), error) ||
        !read_uart_frame(*uart_session_,
                         frame,
                         std::chrono::milliseconds(700),
                         cancel_requested_,
                         error)) {
      continue;
    }
    if (!validate_uart_frame(frame, 0x31, 0, zeros, error)) {
      return fail_(STAGE, error);
    }
    ready = true;
    break;
  }
  if (!ready ||
      !uart_session_->write_text(
          std::string(reinterpret_cast<const char*>(start.data()), start.size()), error)) {
    return fail_(
        STAGE,
        ready ? "Grove UART START write failed: " + error : "Grove UART READY not received");
  }
  const std::array<uint8_t, 10> tx{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  const auto data = make_uart_frame(0x03, tx, 10);
  if (!uart_session_->write_text(
          std::string(reinterpret_cast<const char*>(data.data()), data.size()), error) ||
      !read_uart_frame(*uart_session_,
                       frame,
                       std::chrono::milliseconds(3500),
                       cancel_requested_,
                       error)) {
    return fail_(STAGE, error.empty() ? "Grove UART data exchange failed" : error);
  }
  const std::array<uint8_t, 10> rx{{9, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  if (!validate_uart_frame(frame, 0x03, 10, rx, error)) {
    return fail_(STAGE, error);
  }
  LOG_DEBUG("[CAP-FIXTURE][{}] Grove UART payload verified; restoring HAT UART", STAGE);
  LOG_DEBUG("[CAP-FIXTURE][{}] CAP I2C remains on HAT and grove_fun stays LOW", STAGE);
  if (!write_reg_(STAGE, 0x01, 0x09) ||
      !delay_(STAGE, 200, "CAP HAT UART channel reinitialization") ||
      !delay_(STAGE,
              K_CHANNEL_SWITCH_DELAY_MS,
              "HAT UART host bus settle (fixed; no status polling)")) {
    return false;
  }
  cap_grove_uart_active_ = false;
  pass_item_(K_GROVE_UART_ITEM, "Handshake + payload OK");
  LOG_INFO("[CAP-FIXTURE][{}] PASS", STAGE);
  return true;
}

bool CapFixtureTestModel::prepare_uart_test_(const char* stage) {
  // Match the ADV host: restart the host UART for every UART test so its
  // peripheral configuration and RX buffer are independent of prior traffic.
  if (uart_session_) {
    LOG_DEBUG("[CAP-FIXTURE][{}] closing previous host UART session before reinitialization",
             stage);
    uart_session_.reset();
  }
  LOG_DEBUG("[CAP-FIXTURE][{}] opening host UART for test: path={} baud=115200 8N1",
           stage,
           platform::fixture::K_UART_PATH);
  platform::connectivity::UartOpenResult uart_open_result;
  uart_session_ = platform::connectivity::UartDebugSession::open(
      platform::fixture::K_UART_PATH, 115200, uart_open_result);
  if (!uart_session_) {
    return fail_(stage, "open UART failed: " + uart_open_result.message);
  }

  std::string error;
  const auto discarded = uart_session_->read_available(error);
  if (!error.empty()) {
    return fail_(stage, "failed to clear UART RX before test command: " + error);
  }
  if (!discarded.empty()) {
    LOG_VERBOSE("[CAP-FIXTURE][{}] discarded {} buffered UART byte(s) before test command",
             stage,
             discarded.size());
  }

  if (!write_reg_(stage, 0x01, 0x03)) {
    return false;
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(K_UART_COMMAND_READY_TIMEOUT_MS);
  uint8_t command = 0xFF;
  uint8_t status  = 0xFF;
  while (std::chrono::steady_clock::now() < deadline) {
    if (!read_reg_(stage, 0x01, &command, 1) || !read_reg_(stage, 0x03, &status, 1)) {
      return false;
    }
    if (command == 0x03 && status == 0x01) {
      LOG_DEBUG("[CAP-FIXTURE][{}] UART test command ready: reg01=0x03 reg03=RUNNING",
               stage);
      error.clear();
      const auto post_command_bytes = uart_session_->read_available(error);
      if (!error.empty()) {
        return fail_(stage, "failed to clear UART RX before HELLO: " + error);
      }
      if (!post_command_bytes.empty()) {
        LOG_VERBOSE("[CAP-FIXTURE][{}] discarded {} UART byte(s) after test command before HELLO",
                 stage,
                 post_command_bytes.size());
      }
      return true;
    }
    if (!delay_(stage,
                K_UART_COMMAND_READY_POLL_DELAY_MS,
                "waiting for CAP UART test command readiness")) {
      return false;
    }
  }

  return fail_(stage,
               "CAP UART test command did not become ready: reg01=0x" +
                   hex_bytes(&command, 1) + " reg03=0x" + hex_bytes(&status, 1));
}

bool CapFixtureTestModel::run_spi_() {
  constexpr const char* STAGE = "SPI";
  begin_item_(K_SPI_ITEM, "/dev/spidev0.1 @ 1 MHz");
  std::string error;
  // grove_fun is deliberately untouched here. The normal sequence can reach
  // SPI only after run_grove_i2c_() has restored HAT and driven grove_fun LOW.
  // Do not issue 0x08 from SPI: it is a toggle and would migrate control I2C.
  if (!write_reg_(STAGE, 0x01, 0x05) ||
      !delay_(STAGE, K_COMMAND_DELAY_MS, "CAP SPI slave initialization")) {
    return false;
  }
  const std::vector<uint8_t> tx{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
  const std::vector<uint8_t> expected{0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66};
  std::vector<uint8_t> rx;
  LOG_VERBOSE("[CAP-FIXTURE][SPI] transfer path={} mode=0 speed=1000000 TX={}",
           platform::fixture::K_SPI_PATH,
           hex_bytes(tx));
  if (!platform::fixture::spi_transfer(tx, rx, 1000000, error)) {
    return fail_(STAGE, error);
  }
  LOG_VERBOSE("[CAP-FIXTURE][SPI] RX={}", hex_bytes(rx));
  if (rx != expected) {
    return fail_(STAGE, "MISO payload mismatch: " + hex_bytes(rx));
  }
  if (!delay_(STAGE, 50, "CAP SPI result commit")) {
    return false;
  }
  std::array<uint8_t, 10> cap_received{};
  std::array<uint8_t, 10> cap_replied{};
  if (!read_reg_(STAGE, 0x25, cap_received.data(), cap_received.size()) ||
      !read_reg_(STAGE, 0x2F, cap_replied.data(), cap_replied.size())) {
    return false;
  }
  pass_item_(K_SPI_ITEM, "10-byte transfer OK");
  LOG_INFO("[CAP-FIXTURE][SPI] PASS: full-duplex payload verified");
  return true;
}

bool CapFixtureTestModel::run_usb_() {
  constexpr const char* STAGE = "USB";
  begin_item_(K_USB_ITEM, "Waiting for fixture USB");
  std::string error;
  LOG_DEBUG("[CAP-FIXTURE][USB] select host USB function: {}=1",
           platform::fixture::K_USB_GPIO_FUNCTION_PATH);
  if (!select_host_pin_function_(true, error)) {
    return fail_(STAGE, "USB function switch failed: " + error);
  }
  if (!delay_(STAGE,
              K_USB_FUNCTION_SETTLE_DELAY_MS,
              "host USB D+/D- function settle")) {
    return false;
  }
  LOG_DEBUG("[CAP-FIXTURE][USB] report fixture connection and request CDC+HID enumeration");
  if (!write_reg_(STAGE, 0x10, 0x01) || !write_reg_(STAGE, 0x01, 0x04) ||
      !delay_(STAGE, 500, "CAP USB switch and composite enumeration start")) {
    return false;
  }

  std::string usb_path;
  LOG_DEBUG("[CAP-FIXTURE][USB] wait up to 6 s for CDC+HID enumeration {:04X}:{:04X}",
           K_USB_VENDOR_ID,
           K_USB_PRODUCT_ID);
  if (!platform::fixture::wait_for_usb_device(K_USB_VENDOR_ID,
                                              K_USB_PRODUCT_ID,
                                              std::chrono::milliseconds(
                                                  K_USB_INITIAL_ENUM_TIMEOUT_MS),
                                              cancel_requested_,
                                              usb_path,
                                              error)) {
    if (cancel_requested_.load()) {
      return fail_(STAGE, error);
    }

    LOG_WARN("[CAP-FIXTURE][USB] no device after 6 s; resetting USB hub 05e3:0610 before retry");
    std::string reset_error;
    if (!platform::fixture::reset_usb_hub(reset_error)) {
      return fail_(STAGE, "USB hub reset failed: " + reset_error);
    }
    if (!delay_(STAGE,
                K_USB_HUB_RESET_SETTLE_DELAY_MS,
                "USB hub reinitialization after recovery reset")) {
      return false;
    }
    LOG_DEBUG("[CAP-FIXTURE][USB] retry enumeration for up to 6 s after USB hub reset");
    if (!platform::fixture::wait_for_usb_device(K_USB_VENDOR_ID,
                                                K_USB_PRODUCT_ID,
                                                std::chrono::milliseconds(
                                                    K_USB_RECOVERY_ENUM_TIMEOUT_MS),
                                                cancel_requested_,
                                                usb_path,
                                                error)) {
      return fail_(STAGE, error);
    }
  }
  LOG_DEBUG("[CAP-FIXTURE][USB] composite DUT enumerated at {}; opening HID input before flags=0x03",
           usb_path);

  std::string received;
  LOG_DEBUG("[CAP-FIXTURE][USB] wait up to 4 s for exact HID payload 'HDR-14P'");
  if (!platform::fixture::receive_usb_hid_text(
          K_USB_VENDOR_ID,
          K_USB_PRODUCT_ID,
          "HDR-14P",
          std::chrono::seconds(4),
          cancel_requested_,
          [](std::string& ready_error) {
            LOG_VERBOSE("[CAP-FIXTURE][USB] HID listener ready; I2C TX reg=0x10 data=[03]");
            return platform::fixture::write_register(0x10, 0x03, ready_error);
          },
          received,
          error)) {
    return fail_(STAGE, error);
  }
  LOG_VERBOSE("[CAP-FIXTURE][USB] HID payload RX='{}'; report flags=0x07", received);
  if (!write_reg_(STAGE, 0x10, 0x07) || !delay_(STAGE, 200, "CAP original USB channel restore")) {
    return false;
  }
  LOG_DEBUG("[CAP-FIXTURE][USB] wait up to 3 s for DUT disconnect");
  if (!platform::fixture::wait_for_usb_disconnect(K_USB_VENDOR_ID,
                                                  K_USB_PRODUCT_ID,
                                                  std::chrono::seconds(3),
                                                  cancel_requested_,
                                                  error)) {
    return fail_(STAGE, error);
  }
  if (!write_reg_(STAGE, 0x10, 0x0F) || !delay_(STAGE, 300, "CAP USB completion processing")) {
    return false;
  }
  pass_item_(K_USB_ITEM, "HID HDR-14P received");
  LOG_INFO("[CAP-FIXTURE][USB] PASS: enumerate, HID receive and disconnect verified");
  return true;
}

bool CapFixtureTestModel::run_gpio_() {
  constexpr const char* STAGE = "GPIO";
  begin_item_(K_GPIO_ITEM, "GPIO26/23/22 high");
  std::string error;
  LOG_DEBUG("[CAP-FIXTURE][GPIO] select host GPIO function: {}=0",
           platform::fixture::K_USB_GPIO_FUNCTION_PATH);
  if (!select_host_pin_function_(false, error)) {
    return fail_(STAGE, "GPIO function switch failed: " + error);
  }
  if (!delay_(STAGE, K_INTER_TEST_DELAY_MS, "USB-to-GPIO pin function settle")) {
    return false;
  }
  LOG_DEBUG("[CAP-FIXTURE][GPIO] drive gpiochip0 lines 26,23,22 HIGH");
  if (!platform::fixture::set_gpio_test_outputs(true, error)) {
    return fail_(STAGE, "GPIO output setup failed: " + error);
  }
  if (!hold_gpio_outputs_high_(STAGE, 500, "host GPIO HIGH output settle") ||
      !write_reg_(STAGE, 0x01, 0x06) ||
      !hold_gpio_outputs_high_(STAGE, 250, "CAP input preparation and sampling")) {
    return false;
  }
  uint8_t host_levels = 0;
  if (!platform::fixture::read_gpio_test_output_levels(host_levels, error)) {
    return fail_(STAGE, "host GPIO output readback failed: " + error);
  }
  LOG_VERBOSE("[CAP-FIXTURE][GPIO] host gpiod output readback=0x{:02X} expected mask=0x07",
           host_levels);
  if ((host_levels & 0x07U) != 0x07U) {
    return fail_(STAGE, "host GPIO output bitmap mismatch: 0x" + hex_bytes(&host_levels, 1));
  }
  std::array<uint8_t, K_GPIO_SAMPLE_COUNT> samples{};
  std::size_t completed_samples = 0;
  uint8_t levels                = 0;
  bool read_ok                  = true;
  for (std::size_t i = 0; i < samples.size(); ++i) {
    if (!platform::fixture::set_gpio_test_outputs(true, error)) {
      fail_(STAGE, "GPIO HIGH reassert before sample failed: " + error);
      read_ok = false;
      break;
    }
    if (!read_reg_(STAGE, 0x07, &samples[i], 1)) {
      read_ok = false;
      break;
    }
    levels = samples[i];
    ++completed_samples;
    LOG_VERBOSE("[CAP-FIXTURE][GPIO] sample {}/{} register 0x07=0x{:02X} expected mask=0x07",
             i + 1,
             samples.size(),
             levels);
    if (i + 1 < samples.size() && !hold_gpio_outputs_high_(STAGE,
                                                           K_GPIO_SAMPLE_INTERVAL_MS,
                                                           "interval between CAP GPIO samples")) {
      read_ok = false;
      break;
    }
  }
  LOG_VERBOSE("[CAP-FIXTURE][GPIO] sample summary=[{}] completed={}/{} final=0x{:02X}",
           hex_bytes(samples.data(), completed_samples),
           completed_samples,
           samples.size(),
           levels);
  std::string stop_error;
  const bool stop_ok = platform::fixture::write_register(0x01, 0x00, stop_error);
    LOG_VERBOSE("[CAP-FIXTURE][GPIO] mandatory STOP command write {}{}",
           stop_ok ? "succeeded" : "failed",
           stop_ok ? "" : ": " + stop_error);
  std::string input_error;
  const bool input_ok       = platform::fixture::set_gpio_test_inputs(input_error);
  gpio_test_lines_released_ = input_ok;
  LOG_DEBUG("[CAP-FIXTURE][GPIO] host GPIO26/23/22 final direction={}{}",
           input_ok ? "INPUT/HIGH-Z" : "INPUT request failed",
           input_ok ? "" : ": " + input_error);
  if (!read_ok || completed_samples != samples.size()) {
    return false;
  }
  if ((levels & 0x07U) != 0x07U || (levels & 0xF8U) != 0) {
    return fail_(STAGE, "GPIO bitmap mismatch: 0x" + hex_bytes(&levels, 1));
  }
  if (!stop_ok) {
    return fail_(STAGE, "GPIO STOP command failed: " + stop_error);
  }
  if (!input_ok) {
    return fail_(STAGE, "GPIO final input/high-Z setup failed: " + input_error);
  }
  pass_item_(K_GPIO_ITEM, "Bitmap 0x07");
  LOG_INFO(
      "[CAP-FIXTURE][GPIO] PASS: all outputs observed high; final mode remains GPIO input/high-Z");
  return true;
}

bool CapFixtureTestModel::select_host_pin_function_(bool usb_enabled, std::string& error_message) {
  if (host_pin_function_known_ && host_usb_function_selected_ == usb_enabled) {
    error_message.clear();
    LOG_DEBUG("[CAP-FIXTURE] host pin function already {}; skipping duplicate switch",
              usb_enabled ? "USB" : "GPIO");
    return true;
  }

  const bool selected = usb_enabled ? platform::fixture::select_usb_function(error_message)
                                    : platform::fixture::select_gpio_function(error_message);
  if (selected) {
    host_pin_function_known_    = true;
    host_usb_function_selected_ = usb_enabled;
  }
  return selected;
}

bool CapFixtureTestModel::delay_(const char* stage, unsigned int milliseconds, const char* reason) {
  LOG_VERBOSE("[CAP-FIXTURE][{}] delay {} ms: {}", stage, milliseconds, reason ? reason : "settle");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested_.load()) {
      LOG_WARN("[CAP-FIXTURE][{}] delay interrupted by cancellation", stage);
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return true;
}

bool CapFixtureTestModel::hold_gpio_outputs_high_(const char* stage,
                                                  unsigned int milliseconds,
                                                  const char* reason) {
  LOG_VERBOSE("[CAP-FIXTURE][{}] hold GPIO26/23/22 HIGH for {} ms: {} (refresh={} ms)",
           stage,
           milliseconds,
           reason ? reason : "hold",
           K_GPIO_HOLD_REFRESH_MS);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  unsigned int refresh_count = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested_.load()) {
      LOG_WARN("[CAP-FIXTURE][{}] GPIO HIGH hold interrupted by cancellation", stage);
      return false;
    }
    std::string error;
    if (!platform::fixture::set_gpio_test_outputs(true, error)) {
      return fail_(stage, "GPIO HIGH hold failed: " + error);
    }
    ++refresh_count;
    std::this_thread::sleep_for(std::chrono::milliseconds(K_GPIO_HOLD_REFRESH_MS));
  }
  std::string error;
  if (!platform::fixture::set_gpio_test_outputs(true, error)) {
    return fail_(stage, "final GPIO HIGH hold write failed: " + error);
  }
  LOG_VERBOSE("[CAP-FIXTURE][{}] GPIO26/23/22 HIGH hold complete: {} refresh write(s)",
           stage,
           refresh_count + 1);
  return true;
}

bool CapFixtureTestModel::write_reg_(const char* stage,
                                     unsigned int reg,
                                     const unsigned char* data,
                                     std::size_t size) {
  if (cancel_requested_.load()) {
    return false;
  }
  std::string error;
  LOG_VERBOSE("[CAP-FIXTURE][{}] I2C TX addr=0x42 reg=0x{:02X} data=[{}]",
           stage,
           reg,
           hex_bytes(data, size));
  if (!platform::fixture::write_register(static_cast<uint8_t>(reg), data, size, error)) {
    return fail_(stage, "I2C register write failed: " + error);
  }
  return true;
}

bool CapFixtureTestModel::write_reg_(const char* stage, unsigned int reg, unsigned int value) {
  const uint8_t byte = static_cast<uint8_t>(value);
  return write_reg_(stage, reg, &byte, 1);
}

bool CapFixtureTestModel::read_reg_(const char* stage,
                                    unsigned int reg,
                                    unsigned char* data,
                                    std::size_t size) {
  if (cancel_requested_.load()) {
    return false;
  }
  std::string error;
  LOG_VERBOSE("[CAP-FIXTURE][{}] I2C RX request addr=0x42 reg=0x{:02X} len={} repeated-start",
           stage,
           reg,
           size);
  if (!platform::fixture::read_register(static_cast<uint8_t>(reg), data, size, error)) {
    return fail_(stage, "I2C register read failed: " + error);
  }
  LOG_VERBOSE("[CAP-FIXTURE][{}] I2C RX reg=0x{:02X} data=[{}]", stage, reg, hex_bytes(data, size));
  return true;
}

bool CapFixtureTestModel::fail_(const char* stage, const std::string& message) {
  LOG_ERROR("[CAP-FIXTURE][{}] FAIL: {}", stage ? stage : "RUN", message);
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.state         = CapFixtureRunState::FAILED;
  snapshot_.headline      = std::string(stage ? stage : "Fixture") + " failed";
  snapshot_.error_message = message;
  if (active_item_ < snapshot_.items.size()) {
    snapshot_.items[active_item_].state  = CapFixtureItemState::FAILED;
    snapshot_.items[active_item_].detail = message;
  }
  ++snapshot_.revision;
  return false;
}

void CapFixtureTestModel::begin_item_(std::size_t index, const char* detail) {
  active_item_ = index;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.active_index        = index;
    snapshot_.items[index].state  = CapFixtureItemState::RUNNING;
    snapshot_.items[index].detail = detail ? detail : "Running";
    snapshot_.headline            = std::string("Running ") + snapshot_.items[index].name;
    ++snapshot_.revision;
  }
  LOG_INFO("[CAP-FIXTURE][{}] BEGIN item {}/8: {}",
           snapshot_.items[index].name,
           index + 1,
           detail ? detail : "Running");
}

void CapFixtureTestModel::pass_item_(std::size_t index, const char* detail) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.items[index].state  = CapFixtureItemState::PASSED;
  snapshot_.items[index].detail = detail ? detail : "Passed";
  ++snapshot_.revision;
}

void CapFixtureTestModel::cleanup_() {
  LOG_DEBUG("[CAP-FIXTURE][CLEANUP] begin best-effort Zero hardware restore");
  std::string error;
  // A live Grove I2C CAP channel must be reached through the Grove host bus
  // before toggling it back. UART/SPI recovery, however, must keep grove_fun=0
  // so the CAP control I2C stays on HAT.
  if (cap_grove_i2c_active_) {
    if (!platform::gpio::set_external_bus_i2c_mode(true, error)) {
      LOG_WARN("[CAP-FIXTURE][CLEANUP] failed to select Grove I2C for CAP recovery: {}",
               error);
    } else {
      LOG_DEBUG("[CAP-FIXTURE][CLEANUP] host bus temporarily set to Grove I2C for CAP recovery");
      delay_("CLEANUP",
             K_GROVE_I2C_SWITCH_DELAY_MS,
             "Grove I2C recovery host bus settle (fixed 3000 ms; no status polling)");
    }
  }
  if (cap_grove_i2c_active_) {
    LOG_WARN(
        "[CAP-FIXTURE][CLEANUP] Grove I2C was active; retrying CAP 0x08 toggle before restore");
    if (platform::fixture::write_register(0x01, 0x08, error)) {
      delay_("CLEANUP", 200, "CAP I2C channel recovery");
      delay_("CLEANUP",
             K_CHANNEL_SWITCH_DELAY_MS,
             "CAP I2C recovery settle (fixed; no status polling)");
    } else {
      LOG_WARN("[CAP-FIXTURE][CLEANUP] Grove I2C toggle retry failed: {}", error);
    }
    cap_grove_i2c_active_ = false;
  }
  if (!platform::gpio::set_external_bus_uart_mode(error)) {
    LOG_WARN("[CAP-FIXTURE][CLEANUP] failed to keep grove_fun=0 for HAT I2C: {}", error);
  } else {
    LOG_DEBUG("[CAP-FIXTURE][CLEANUP] grove_fun kept LOW; CAP control I2C remains on HAT");
    host_external_uart_selected_ = true;
  }
  if (cap_grove_uart_active_) {
    LOG_WARN("[CAP-FIXTURE][CLEANUP] Grove UART was active; retrying CAP 0x09 toggle");
    if (platform::fixture::write_register(0x01, 0x09, error)) {
      delay_("CLEANUP", 200, "CAP UART channel recovery");
      delay_("CLEANUP",
             K_CHANNEL_SWITCH_DELAY_MS,
             "CAP UART recovery settle (fixed; no status polling)");
    } else {
      LOG_WARN("[CAP-FIXTURE][CLEANUP] Grove UART toggle retry failed: {}", error);
    }
    cap_grove_uart_active_ = false;
  }
  error.clear();
  if (!platform::fixture::write_register(0x01, 0x00, error)) {
    LOG_WARN("[CAP-FIXTURE][CLEANUP] CAP STOP command failed: {}", error);
  } else {
    LOG_DEBUG("[CAP-FIXTURE][CLEANUP] CAP STOP command sent");
  }
  error.clear();
  if (!select_host_pin_function_(false, error)) {
    LOG_ERROR("[CAP-FIXTURE][CLEANUP] failed to keep host GPIO function: {}", error);
  } else {
    LOG_DEBUG("[CAP-FIXTURE][CLEANUP] host pin function kept as GPIO: brightness=0");
    std::this_thread::sleep_for(std::chrono::milliseconds(K_SYSFS_SWITCH_DELAY_MS));
  }
  error.clear();
  if (gpio_test_lines_released_) {
    LOG_DEBUG("[CAP-FIXTURE][CLEANUP] GPIO26/23/22 already released; skipping duplicate setup");
  } else if (!platform::fixture::set_gpio_test_inputs(error)) {
    LOG_ERROR("[CAP-FIXTURE][CLEANUP] failed to release GPIO26/23/22 in input/high-Z state: {}",
              error);
  } else {
    gpio_test_lines_released_ = true;
    LOG_DEBUG("[CAP-FIXTURE][CLEANUP] GPIO26/23/22 released in input/high-Z state");
  }
  error.clear();
  if (!platform::fixture::set_ext_5v_enabled(true, error)) {
    LOG_ERROR("[CAP-FIXTURE][CLEANUP] failed to restore EXT 5V output: {}", error);
  } else {
    LOG_DEBUG("[CAP-FIXTURE][CLEANUP] EXT 5V output restored: brightness=1");
  }
  uart_session_.reset();
  LOG_DEBUG("[CAP-FIXTURE][CLEANUP] host UART session closed");
  LOG_DEBUG("[CAP-FIXTURE][CLEANUP] complete");
}

void CapFixtureTestModel::publish_(CapFixtureRunState state, const std::string& headline) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.state    = state;
  snapshot_.headline = headline;
  ++snapshot_.revision;
}

const char* cap_fixture_item_state_text(CapFixtureItemState state) {
  switch (state) {
    case CapFixtureItemState::RUNNING:
      return "RUN";
    case CapFixtureItemState::PASSED:
      return "PASS";
    case CapFixtureItemState::FAILED:
      return "FAIL";
    case CapFixtureItemState::PENDING:
    default:
      return "WAIT";
  }
}

}  // namespace model
