/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <string>

namespace platform::gpio {

struct OutputLineConfig {
  std::string chip_path{"/dev/gpiochip0"};
  unsigned int line_offset{0};
  std::string consumer{"factory-test"};
};

class OutputLine {
 public:
  explicit OutputLine(OutputLineConfig config);
  ~OutputLine();

  OutputLine(const OutputLine&)            = delete;
  OutputLine& operator=(const OutputLine&) = delete;

  bool set_value(bool active, std::string& error_message);
  void release();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

bool set_output_value(const OutputLineConfig& config, bool active, std::string& error_message);
bool set_external_bus_i2c_mode(bool enabled, std::string& error_message);
bool set_external_bus_uart_mode(std::string& error_message);

}  // namespace platform::gpio
