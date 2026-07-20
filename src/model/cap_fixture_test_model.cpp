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
#include "logger.h"
#include "uart_service.h"

namespace model {
namespace {

constexpr std::size_t K_I2C_ITEM   = 0;
constexpr std::size_t K_POWER_ITEM = 1;
constexpr std::size_t K_UART_ITEM  = 2;
constexpr std::size_t K_SPI_ITEM   = 3;
constexpr std::size_t K_USB_ITEM   = 4;
constexpr std::size_t K_GPIO_ITEM  = 5;

constexpr unsigned int K_FEATURE_GUARD_DELAY_MS  = 100;
constexpr unsigned int K_COMMAND_DELAY_MS        = 100;
constexpr unsigned int K_SYSFS_SWITCH_DELAY_MS   = 150;
constexpr unsigned int K_INTER_TEST_DELAY_MS     = 1000;
constexpr unsigned int K_GPIO_HOLD_REFRESH_MS    = 10;
constexpr std::size_t K_GPIO_SAMPLE_COUNT        = 5;
constexpr unsigned int K_GPIO_SAMPLE_INTERVAL_MS = 100;

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
  while (received.size() < frame.size() && std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested.load()) {
      error = "UART receive canceled";
      return false;
    }
    auto chunk = uart.read_available(error);
    if (!error.empty()) {
      return false;
    }
    received.append(chunk);
    if (received.size() < frame.size()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  if (received.size() < frame.size()) {
    error = "UART receive timed out: " + std::to_string(received.size()) + "/16 byte(s)";
    return false;
  }
  std::copy_n(reinterpret_cast<const uint8_t*>(received.data()), frame.size(), frame.begin());
  if (received.size() > frame.size()) {
    LOG_WARN("[CAP-FIXTURE][UART] received {} extra byte(s) after frame",
             received.size() - frame.size());
  }
  return true;
}

}  // namespace

CapFixtureTestModel::CapFixtureTestModel() {
  snapshot_.items = {{
      {"I2C", CapFixtureItemState::PENDING, "Waiting"},
      {"Power", CapFixtureItemState::PENDING, "Waiting"},
      {"UART", CapFixtureItemState::PENDING, "Waiting"},
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
  publish_(CapFixtureRunState::RUNNING, "Preparing Zero interfaces");
  LOG_INFO(
      "[CAP-FIXTURE][RUN] starting Zero fixture sequence: I2C -> Power -> UART -> SPI -> USB -> "
      "GPIO");
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
  LOG_INFO(
      "[CAP-FIXTURE][PREPARE] no status/step registers will be polled; fixed delays and data "
      "checks are active");

  std::string error;
  LOG_INFO("[CAP-FIXTURE][PREPARE] restoring EXT 5V output: {}=1",
           platform::fixture::K_EXT_5V_PATH);
  if (!platform::fixture::set_ext_5v_enabled(true, error)) {
    fail_("PREPARE", "failed to enable EXT 5V: " + error);
    cleanup_();
    return;
  }
  if (!delay_("PREPARE", K_SYSFS_SWITCH_DELAY_MS, "EXT 5V output settle")) {
    cleanup_();
    return;
  }
  LOG_INFO("[CAP-FIXTURE][PREPARE] restoring host USB function: {}=1",
           platform::fixture::K_USB_GPIO_FUNCTION_PATH);
  if (!platform::fixture::set_usb_gpio_function(true, error)) {
    fail_("PREPARE", "failed to select USB function: " + error);
    cleanup_();
    return;
  }
  if (!delay_("PREPARE", K_SYSFS_SWITCH_DELAY_MS, "USB pin function settle") ||
      !delay_("PREPARE", K_FEATURE_GUARD_DELAY_MS, "CAP HAT I2C startup guard")) {
    cleanup_();
    return;
  }

  const bool passed =
      run_i2c_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "I2C -> Power guard") &&
      run_power_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "Power -> UART guard") &&
      run_uart_() && delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "UART -> SPI guard") && run_spi_() &&
      delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "SPI -> USB guard") && run_usb_() &&
      delay_("SEQUENCE", K_INTER_TEST_DELAY_MS, "USB -> GPIO guard") && run_gpio_();
  cleanup_();
  if (cancel_requested_.load()) {
    publish_(CapFixtureRunState::CANCELED, "Fixture test canceled");
    LOG_WARN("[CAP-FIXTURE][RUN] sequence canceled after cleanup");
  } else if (passed) {
    publish_(CapFixtureRunState::PASSED, "All fixture tests passed");
    LOG_INFO("[CAP-FIXTURE][RUN] PASS: all six Zero fixture tests completed");
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
  pass_item_(K_I2C_ITEM, "0xA55A verified");
  LOG_INFO("[CAP-FIXTURE][I2C] PASS: repeated-start read and register write verified");
  return true;
}

bool CapFixtureTestModel::run_power_() {
  constexpr const char* STAGE = "POWER";
  begin_item_(K_POWER_ITEM, "5VIN / 5VOUT");
  std::string error;
  LOG_INFO("[CAP-FIXTURE][POWER] ensure EXT 5V output is enabled");
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
  LOG_INFO("[CAP-FIXTURE][POWER] power_supply {} -> charge report 0x{:02X}",
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
      static_cast<unsigned int>(low) | (static_cast<unsigned int>(high) << 8U);
  LOG_INFO(
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
  if (!write_reg_(STAGE, 0x01, 0x03) ||
      !delay_(STAGE, K_COMMAND_DELAY_MS, "CAP UART initialization") ||
      !delay_(STAGE, K_FEATURE_GUARD_DELAY_MS, "I2C command-to-UART traffic guard")) {
    return false;
  }

  std::string error;
  LOG_INFO("[CAP-FIXTURE][UART] CAP HAT UART uses a dedicated path; Grove mux GPIO4 is unchanged");
  platform::connectivity::UartOpenResult open_result;
  auto uart = platform::connectivity::UartDebugSession::open(platform::fixture::K_UART_PATH,
                                                             115200,
                                                             open_result);
  if (!uart) {
    return fail_(STAGE, "open UART failed: " + open_result.message);
  }

  const std::array<uint8_t, 10> zeros{};
  const auto hello = make_uart_frame(0x30, zeros, 0);
  const auto start = make_uart_frame(0x32, zeros, 0);
  std::array<uint8_t, 16> ready{};
  bool ready_received = false;
  for (int attempt = 1; attempt <= 3 && !cancel_requested_.load(); ++attempt) {
    LOG_INFO("[CAP-FIXTURE][UART] HELLO attempt {}/3 TX={}",
             attempt,
             hex_bytes(hello.data(), hello.size()));
    if (!uart->write_text(std::string(reinterpret_cast<const char*>(hello.data()), hello.size()),
                          error)) {
      return fail_(STAGE, "HELLO write failed: " + error);
    }
    error.clear();
    if (!read_uart_frame(*uart, ready, std::chrono::milliseconds(700), cancel_requested_, error)) {
      LOG_WARN("[CAP-FIXTURE][UART] HELLO attempt {} did not receive READY: {}", attempt, error);
      continue;
    }
    LOG_INFO("[CAP-FIXTURE][UART] READY RX={}", hex_bytes(ready.data(), ready.size()));
    if (!validate_uart_frame(ready, 0x31, 0, zeros, error)) {
      return fail_(STAGE, "invalid READY frame: " + error);
    }
    ready_received = true;
    break;
  }
  if (!ready_received) {
    return fail_(STAGE, "READY not received after three HELLO attempts");
  }

  LOG_INFO("[CAP-FIXTURE][UART] START TX={}", hex_bytes(start.data(), start.size()));
  if (!uart->write_text(std::string(reinterpret_cast<const char*>(start.data()), start.size()),
                        error)) {
    return fail_(STAGE, "START write failed: " + error);
  }
  const std::array<uint8_t, 10> tx_payload{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  const auto data = make_uart_frame(0x03, tx_payload, 10);
  LOG_INFO("[CAP-FIXTURE][UART] DATA TX={}", hex_bytes(data.data(), data.size()));
  if (!uart->write_text(std::string(reinterpret_cast<const char*>(data.data()), data.size()),
                        error)) {
    return fail_(STAGE, "DATA write failed: " + error);
  }
  std::array<uint8_t, 16> reply{};
  if (!read_uart_frame(*uart, reply, std::chrono::milliseconds(3500), cancel_requested_, error)) {
    return fail_(STAGE, error);
  }
  LOG_INFO("[CAP-FIXTURE][UART] DATA RX={}", hex_bytes(reply.data(), reply.size()));
  const std::array<uint8_t, 10> rx_payload{{9, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  if (!validate_uart_frame(reply, 0x03, 10, rx_payload, error)) {
    return fail_(STAGE, error);
  }
  uart.reset();
  if (!delay_(STAGE, K_FEATURE_GUARD_DELAY_MS, "UART close-to-I2C traffic guard")) {
    return false;
  }
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

bool CapFixtureTestModel::run_spi_() {
  constexpr const char* STAGE = "SPI";
  begin_item_(K_SPI_ITEM, "/dev/spidev0.1 @ 1 MHz");
  if (!write_reg_(STAGE, 0x01, 0x05) ||
      !delay_(STAGE, K_COMMAND_DELAY_MS, "CAP SPI slave initialization")) {
    return false;
  }
  const std::vector<uint8_t> tx{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
  const std::vector<uint8_t> expected{0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66};
  std::vector<uint8_t> rx;
  std::string error;
  LOG_INFO("[CAP-FIXTURE][SPI] transfer path={} mode=0 speed=1000000 TX={}",
           platform::fixture::K_SPI_PATH,
           hex_bytes(tx));
  if (!platform::fixture::spi_transfer(tx, rx, 1000000, error)) {
    return fail_(STAGE, error);
  }
  LOG_INFO("[CAP-FIXTURE][SPI] RX={}", hex_bytes(rx));
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
  LOG_INFO("[CAP-FIXTURE][USB] select host USB function: {}=1",
           platform::fixture::K_USB_GPIO_FUNCTION_PATH);
  if (!platform::fixture::set_usb_gpio_function(true, error)) {
    return fail_(STAGE, "USB function switch failed: " + error);
  }
  if (!delay_(STAGE, 200, "host USB D+/D- function settle")) {
    return false;
  }
  LOG_INFO("[CAP-FIXTURE][USB] report fixture connection and request CDC+HID enumeration");
  if (!write_reg_(STAGE, 0x10, 0x01) || !write_reg_(STAGE, 0x01, 0x04) ||
      !delay_(STAGE, 500, "CAP USB switch and composite enumeration start")) {
    return false;
  }

  std::string usb_path;
  LOG_INFO("[CAP-FIXTURE][USB] wait up to 12 s for CDC+HID enumeration");
  if (!platform::fixture::wait_for_usb_device(0x303A,
                                              0x1001,
                                              std::chrono::seconds(12),
                                              cancel_requested_,
                                              usb_path,
                                              error)) {
    return fail_(STAGE, error);
  }
  LOG_INFO("[CAP-FIXTURE][USB] composite DUT enumerated at {}; opening HID input before flags=0x03",
           usb_path);

  std::string received;
  LOG_INFO("[CAP-FIXTURE][USB] wait up to 4 s for exact HID payload 'HDR-14P'");
  if (!platform::fixture::receive_usb_hid_text(
          0x303A,
          0x1001,
          "HDR-14P",
          std::chrono::seconds(4),
          cancel_requested_,
          [](std::string& ready_error) {
            LOG_INFO("[CAP-FIXTURE][USB] HID listener ready; I2C TX reg=0x10 data=[03]");
            return platform::fixture::write_register(0x10, 0x03, ready_error);
          },
          received,
          error)) {
    return fail_(STAGE, error);
  }
  LOG_INFO("[CAP-FIXTURE][USB] HID payload RX='{}'; report flags=0x07", received);
  if (!write_reg_(STAGE, 0x10, 0x07) || !delay_(STAGE, 200, "CAP original USB channel restore")) {
    return false;
  }
  LOG_INFO("[CAP-FIXTURE][USB] wait up to 3 s for DUT disconnect");
  if (!platform::fixture::wait_for_usb_disconnect(0x303A,
                                                  0x1001,
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
  LOG_INFO("[CAP-FIXTURE][GPIO] select host GPIO function: {}=0",
           platform::fixture::K_USB_GPIO_FUNCTION_PATH);
  if (!platform::fixture::set_usb_gpio_function(false, error)) {
    return fail_(STAGE, "GPIO function switch failed: " + error);
  }
  if (!delay_(STAGE, K_INTER_TEST_DELAY_MS, "USB-to-GPIO pin function settle")) {
    return false;
  }
  LOG_INFO("[CAP-FIXTURE][GPIO] drive gpiochip0 lines 26,23,22 HIGH");
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
  LOG_INFO("[CAP-FIXTURE][GPIO] host gpiod output readback=0x{:02X} expected mask=0x07",
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
    LOG_INFO("[CAP-FIXTURE][GPIO] sample {}/{} register 0x07=0x{:02X} expected mask=0x07",
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
  LOG_INFO("[CAP-FIXTURE][GPIO] sample summary=[{}] completed={}/{} final=0x{:02X}",
           hex_bytes(samples.data(), completed_samples),
           completed_samples,
           samples.size(),
           levels);
  std::string stop_error;
  const bool stop_ok = platform::fixture::write_register(0x01, 0x00, stop_error);
  LOG_INFO("[CAP-FIXTURE][GPIO] mandatory STOP command write {}{}",
           stop_ok ? "succeeded" : "failed",
           stop_ok ? "" : ": " + stop_error);
  std::string input_error;
  const bool input_ok = platform::fixture::set_gpio_test_inputs(input_error);
  LOG_INFO("[CAP-FIXTURE][GPIO] host GPIO26/23/22 final direction={}{}",
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

bool CapFixtureTestModel::delay_(const char* stage, unsigned int milliseconds, const char* reason) {
  LOG_INFO("[CAP-FIXTURE][{}] delay {} ms: {}", stage, milliseconds, reason ? reason : "settle");
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
  LOG_INFO("[CAP-FIXTURE][{}] hold GPIO26/23/22 HIGH for {} ms: {} (refresh={} ms)",
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
  LOG_INFO("[CAP-FIXTURE][{}] GPIO26/23/22 HIGH hold complete: {} refresh write(s)",
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
  LOG_INFO("[CAP-FIXTURE][{}] I2C TX addr=0x42 reg=0x{:02X} data=[{}]",
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
  LOG_INFO("[CAP-FIXTURE][{}] I2C RX request addr=0x42 reg=0x{:02X} len={} repeated-start",
           stage,
           reg,
           size);
  if (!platform::fixture::read_register(static_cast<uint8_t>(reg), data, size, error)) {
    return fail_(stage, "I2C register read failed: " + error);
  }
  LOG_INFO("[CAP-FIXTURE][{}] I2C RX reg=0x{:02X} data=[{}]", stage, reg, hex_bytes(data, size));
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
  LOG_INFO("[CAP-FIXTURE][{}] BEGIN item {}/6: {}",
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
  LOG_INFO("[CAP-FIXTURE][CLEANUP] begin best-effort Zero hardware restore");
  std::string error;
  if (!platform::fixture::write_register(0x01, 0x00, error)) {
    LOG_WARN("[CAP-FIXTURE][CLEANUP] CAP STOP command failed: {}", error);
  } else {
    LOG_INFO("[CAP-FIXTURE][CLEANUP] CAP STOP command sent");
  }
  error.clear();
  if (!platform::fixture::set_usb_gpio_function(false, error)) {
    LOG_ERROR("[CAP-FIXTURE][CLEANUP] failed to keep host GPIO function: {}", error);
  } else {
    LOG_INFO("[CAP-FIXTURE][CLEANUP] host pin function kept as GPIO: brightness=0");
    std::this_thread::sleep_for(std::chrono::milliseconds(K_SYSFS_SWITCH_DELAY_MS));
  }
  error.clear();
  if (!platform::fixture::set_gpio_test_inputs(error)) {
    LOG_ERROR("[CAP-FIXTURE][CLEANUP] failed to retain GPIO26/23/22 as input/high-Z: {}", error);
  } else {
    LOG_INFO("[CAP-FIXTURE][CLEANUP] GPIO26/23/22 retained as input/high-Z");
  }
  error.clear();
  if (!platform::fixture::set_ext_5v_enabled(true, error)) {
    LOG_ERROR("[CAP-FIXTURE][CLEANUP] failed to restore EXT 5V output: {}", error);
  } else {
    LOG_INFO("[CAP-FIXTURE][CLEANUP] EXT 5V output restored: brightness=1");
  }
  LOG_INFO("[CAP-FIXTURE][CLEANUP] complete");
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
