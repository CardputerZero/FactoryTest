/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace platform::fixture {

constexpr int K_I2C_BUS                        = 1;
constexpr uint8_t K_I2C_ADDRESS                = 0x42;
constexpr const char* K_UART_PATH              = "/dev/ttyS0";
constexpr const char* K_SPI_PATH               = "/dev/spidev0.1";
constexpr const char* K_EXT_5V_PATH            = "/sys/class/leds/ext_5v_out/brightness";
constexpr const char* K_USB_GPIO_FUNCTION_PATH = "/sys/class/leds/ext_usb_gpio_fun/brightness";

bool write_register(uint8_t reg, const uint8_t* data, std::size_t size, std::string& error_message);
bool write_register(uint8_t reg, uint8_t value, std::string& error_message);
bool read_register(uint8_t reg, uint8_t* data, std::size_t size, std::string& error_message);

bool set_ext_5v_enabled(bool enabled, std::string& error_message);
bool set_usb_gpio_function(bool usb_enabled, std::string& error_message);
bool read_charge_report(uint8_t& report, std::string& detail, std::string& error_message);

bool spi_transfer(const std::vector<uint8_t>& tx,
                  std::vector<uint8_t>& rx,
                  uint32_t speed_hz,
                  std::string& error_message);

bool set_gpio_test_outputs(bool active, std::string& error_message);
bool read_gpio_test_output_levels(uint8_t& levels, std::string& error_message);
bool set_gpio_test_inputs(std::string& error_message);

bool wait_for_usb_device(uint16_t vendor_id,
                         uint16_t product_id,
                         std::chrono::milliseconds timeout,
                         const std::atomic_bool& cancel_requested,
                         std::string& sys_path,
                         std::string& error_message);
bool wait_for_usb_disconnect(uint16_t vendor_id,
                             uint16_t product_id,
                             std::chrono::milliseconds timeout,
                             const std::atomic_bool& cancel_requested,
                             std::string& error_message);
bool receive_usb_hid_text(uint16_t vendor_id,
                          uint16_t product_id,
                          const std::string& expected,
                          std::chrono::milliseconds timeout,
                          const std::atomic_bool& cancel_requested,
                          const std::function<bool(std::string&)>& ready_action,
                          std::string& received,
                          std::string& error_message);

}  // namespace platform::fixture
