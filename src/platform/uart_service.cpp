/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "uart_service.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

#include "logger.h"

#if APP_USE_LIBSERIALPORT
#include <libserialport.h>
#endif

namespace platform::connectivity {
namespace {

std::string lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

#if defined(__linux__)
std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  std::ostringstream stream;
  stream << file.rdbuf();
  return stream.str();
}

std::string basename_from_path(const std::string& path) {
  const auto pos = path.find_last_of('/');
  return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool kernel_console_references_uart(const std::string& path) {
  const auto tty_name = lower_copy(basename_from_path(path));
  const auto cmdline  = lower_copy(read_text_file("/proc/cmdline"));
  if (!cmdline.empty() && (cmdline.find("console=" + tty_name) != std::string::npos ||
                           cmdline.find("console=serial0") != std::string::npos)) {
    return true;
  }

  std::istringstream consoles(read_text_file("/proc/consoles"));
  std::string line;
  while (std::getline(consoles, line)) {
    if (lower_copy(line).find(tty_name) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool login_process_uses_uart(const std::string& path) {
  const auto tty_name = lower_copy(basename_from_path(path));
  std::error_code ec;
  const auto proc_root = std::filesystem::path{"/proc"};
  for (const auto& entry : std::filesystem::directory_iterator(proc_root, ec)) {
    if (ec || !entry.is_directory(ec)) {
      continue;
    }

    const auto pid = entry.path().filename().string();
    if (pid.empty() || !std::all_of(pid.begin(), pid.end(), [](unsigned char ch) {
          return std::isdigit(ch) != 0;
        })) {
      continue;
    }

    const auto cmdline = lower_copy(read_text_file(entry.path() / "cmdline"));
    const bool login_service =
        cmdline.find("getty") != std::string::npos || cmdline.find("login") != std::string::npos;
    if (login_service && cmdline.find(tty_name) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool uart_occupied_by_console(const std::string& path) {
  return kernel_console_references_uart(path) || login_process_uses_uart(path);
}
#else
bool uart_occupied_by_console(const std::string& path) {
  (void)path;
  return false;
}
#endif

#if APP_USE_LIBSERIALPORT
struct PortConfigOwner {
  sp_port_config* config{nullptr};

  ~PortConfigOwner() {
    if (config) {
      sp_free_config(config);
    }
  }
};

std::string serialport_error(const char* operation, sp_return result) {
  std::string message = operation ? operation : "libserialport";
  message += " failed";

  if (result == SP_ERR_FAIL) {
    char* detail = sp_last_error_message();
    if (detail) {
      message += ": ";
      message += detail;
      sp_free_error_message(detail);
    }
  } else if (result == SP_ERR_ARG) {
    message += ": invalid argument";
  } else if (result == SP_ERR_MEM) {
    message += ": out of memory";
  } else if (result == SP_ERR_SUPP) {
    message += ": unsupported operation";
  } else {
    message += ": code ";
    message += std::to_string(static_cast<int>(result));
  }
  return message;
}

bool check_sp(sp_return result, const char* operation, std::string& error_message) {
  if (result == SP_OK) {
    return true;
  }
  error_message = serialport_error(operation, result);
  return false;
}

bool configure_uart_port(sp_port* port, int baud_rate, std::string& error_message) {
  if (!port) {
    error_message = "UART is not open";
    return false;
  }

  sp_drain(port);
  sp_flush(port, SP_BUF_BOTH);

  PortConfigOwner owner;
  auto rc = sp_new_config(&owner.config);
  if (!check_sp(rc, "sp_new_config", error_message)) {
    return false;
  }

  if (!check_sp(sp_set_config_baudrate(owner.config, baud_rate),
                "sp_set_config_baudrate",
                error_message) ||
      !check_sp(sp_set_config_bits(owner.config, 8), "sp_set_config_bits", error_message) ||
      !check_sp(sp_set_config_parity(owner.config, SP_PARITY_NONE),
                "sp_set_config_parity",
                error_message) ||
      !check_sp(sp_set_config_stopbits(owner.config, 1),
                "sp_set_config_stopbits",
                error_message) ||
      // Avoid flowcontrol presets: they also overwrite RTS/DTR on four-wire UART.
      !check_sp(sp_set_config_xon_xoff(owner.config, SP_XONXOFF_DISABLED),
                "sp_set_config_xon_xoff",
                error_message) ||
      !check_sp(sp_set_config_cts(owner.config, SP_CTS_IGNORE),
                "sp_set_config_cts",
                error_message) ||
      !check_sp(sp_set_config_dsr(owner.config, SP_DSR_IGNORE),
                "sp_set_config_dsr",
                error_message)) {
    return false;
  }

  if (!check_sp(sp_set_config(port, owner.config), "sp_set_config", error_message)) {
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  sp_flush(port, SP_BUF_BOTH);
  return true;
}
#endif

}  // namespace

UartDebugSession::UartDebugSession(sp_port* port, std::string path, int baud_rate)
    : port_(port),
      path_(std::move(path)),
      baud_rate_(baud_rate) {}

UartDebugSession::~UartDebugSession() {
#if APP_USE_LIBSERIALPORT
  if (port_) {
    sp_drain(port_);
    sp_close(port_);
    sp_free_port(port_);
    port_ = nullptr;
  }
#endif
}

std::unique_ptr<UartDebugSession> UartDebugSession::open(const std::string& path,
                                                         int baud_rate,
                                                         UartOpenResult& result) {
  result = {};
#if APP_USE_LIBSERIALPORT
  if (uart_occupied_by_console(path)) {
    result.status  = UartOpenStatus::OCCUPIED_BY_CONSOLE;
    result.message = "UART is used by console. Disable serial login in raspi-config.";
    return nullptr;
  }

  sp_port* port = nullptr;
  auto rc       = sp_get_port_by_name(path.c_str(), &port);
  if (rc != SP_OK || !port) {
    result.status  = UartOpenStatus::OPEN_FAILED;
    result.message = serialport_error("sp_get_port_by_name", rc);
    return nullptr;
  }

  rc = sp_open(port, SP_MODE_READ_WRITE);
  if (rc != SP_OK) {
    result.status  = UartOpenStatus::OPEN_FAILED;
    result.message = serialport_error("sp_open", rc);
    sp_free_port(port);
    return nullptr;
  }

  std::string error;
  if (!configure_uart_port(port, baud_rate, error)) {
    sp_close(port);
    sp_free_port(port);
    result.status  = UartOpenStatus::OPEN_FAILED;
    result.message = error.empty() ? "Configure UART failed" : error;
    return nullptr;
  }

  result.status  = UartOpenStatus::OK;
  result.message = "OK";
  LOG_INFO("UART debug opened path={} baud={} mode=8N1 backend=libserialport", path, baud_rate);
  return std::unique_ptr<UartDebugSession>(new UartDebugSession(port, path, baud_rate));
#else
  (void)path;
  (void)baud_rate;
  result.status  = UartOpenStatus::UNSUPPORTED;
  result.message = "libserialport backend is not enabled";
  return nullptr;
#endif
}

int UartDebugSession::baud_rate() const { return baud_rate_; }

bool UartDebugSession::set_baud_rate(int baud_rate, std::string& error_message) {
  error_message.clear();
#if APP_USE_LIBSERIALPORT
  if (!port_) {
    error_message = "UART is not open";
    return false;
  }
  if (!configure_uart_port(port_, baud_rate, error_message)) {
    return false;
  }
  baud_rate_ = baud_rate;
  LOG_INFO("UART debug baud changed path={} baud={} mode=8N1 backend=libserialport",
           path_,
           baud_rate_);
  return true;
#else
  (void)baud_rate;
  error_message = "libserialport backend is not enabled";
  return false;
#endif
}

std::string UartDebugSession::read_available(std::string& error_message) {
  error_message.clear();
  std::string output;
#if APP_USE_LIBSERIALPORT
  if (!port_) {
    error_message = "UART is not open";
    return output;
  }

  std::array<char, 256> buffer{};
  while (true) {
    const auto count = sp_nonblocking_read(port_, buffer.data(), buffer.size());
    if (count > 0) {
      output.append(buffer.data(), static_cast<std::size_t>(count));
      continue;
    }
    if (count == 0) {
      break;
    }
    error_message = serialport_error("sp_nonblocking_read", count);
    break;
  }
#else
  error_message = "libserialport backend is not enabled";
#endif
  return output;
}

bool UartDebugSession::write_text(const std::string& text, std::string& error_message) {
  error_message.clear();
#if APP_USE_LIBSERIALPORT
  if (!port_) {
    error_message = "UART is not open";
    return false;
  }

  const char* data      = text.data();
  std::size_t remaining = text.size();
  while (remaining > 0) {
    const auto chunk = std::min<std::size_t>(remaining, 256);
    const auto count = sp_blocking_write(port_, data, chunk, 250);
    if (count > 0) {
      data += count;
      remaining -= static_cast<std::size_t>(count);
      continue;
    }
    if (count == 0) {
      error_message = "UART write timed out";
    } else {
      error_message = serialport_error("sp_blocking_write", count);
    }
    return false;
  }

  const auto rc = sp_drain(port_);
  if (rc != SP_OK) {
    error_message = serialport_error("sp_drain", rc);
    return false;
  }
  return true;
#else
  (void)text;
  error_message = "libserialport backend is not enabled";
  return false;
#endif
}

}  // namespace platform::connectivity
