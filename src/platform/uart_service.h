/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <string>

struct sp_port;

namespace platform::connectivity {

enum class UartOpenStatus {
  OK,
  OCCUPIED_BY_CONSOLE,
  OPEN_FAILED,
  UNSUPPORTED,
};

struct UartOpenResult {
  UartOpenStatus status{UartOpenStatus::UNSUPPORTED};
  std::string message;
};

class UartDebugSession {
 public:
  ~UartDebugSession();

  UartDebugSession(const UartDebugSession&)            = delete;
  UartDebugSession& operator=(const UartDebugSession&) = delete;

  static std::unique_ptr<UartDebugSession> open(const std::string& path,
                                                int baud_rate,
                                                UartOpenResult& result);
  int baud_rate() const;
  bool set_baud_rate(int baud_rate, std::string& error_message);
  std::string read_available(std::string& error_message);
  bool write_text(const std::string& text, std::string& error_message);

 private:
  UartDebugSession(sp_port* port, std::string path, int baud_rate);

  sp_port* port_{nullptr};
  std::string path_;
  int baud_rate_{9600};
};

}  // namespace platform::connectivity
