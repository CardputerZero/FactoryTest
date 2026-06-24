/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "gpio_service.h"

#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

#include "logger.h"

#if APP_USE_LIBGPIOD
#include <gpiod.h>
#endif

namespace platform::gpio {
namespace {

constexpr const char* K_EXTERNAL_BUS_GPIO_CHIP     = "/dev/gpiochip0";
constexpr unsigned int K_EXTERNAL_BUS_GPIO_LINE    = 4;
constexpr const char* K_EXTERNAL_BUS_GPIO_CONSUMER = "factory-test-ext-bus";

std::string errno_message(const char* prefix) {
  return std::string(prefix) + ": " + std::strerror(errno);
}

}  // namespace

struct OutputLine::Impl {
  explicit Impl(OutputLineConfig line_config) : config(std::move(line_config)) {}

  bool set_value(bool active, std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex);
    error_message.clear();

#if APP_USE_LIBGPIOD
    const auto value = active ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    if (!request && !request_line_locked(value, error_message)) {
      return false;
    }

    if (gpiod_line_request_set_value(request, config.line_offset, value) == 0) {
      return true;
    }

    // If the request became invalid, release and try once more with the requested value.
    LOG_WARN("GPIO{} set failed, retrying request: {}", config.line_offset, std::strerror(errno));
    release_locked();
    if (!request_line_locked(value, error_message)) {
      return false;
    }
    if (gpiod_line_request_set_value(request, config.line_offset, value) == 0) {
      return true;
    }

    error_message = errno_message("Failed to set GPIO value");
    LOG_WARN("GPIO{} set failed: {}", config.line_offset, error_message);
    release_locked();
    return false;
#else
    (void)active;
    error_message = "libgpiod headers/library not found; cannot control GPIO";
    LOG_WARN("GPIO{} unavailable: {}", config.line_offset, error_message);
    return false;
#endif
  }

  void release() {
    std::lock_guard<std::mutex> lock(mutex);
    release_locked();
  }

#if APP_USE_LIBGPIOD
  bool request_line_locked(gpiod_line_value initial_value, std::string& error_message) {
    gpiod_chip* chip = gpiod_chip_open(config.chip_path.c_str());
    if (!chip) {
      error_message = errno_message((std::string("Failed to open ") + config.chip_path).c_str());
      LOG_WARN("GPIO{} request failed: {}", config.line_offset, error_message);
      return false;
    }

    gpiod_line_settings* settings         = gpiod_line_settings_new();
    gpiod_line_config* line_config        = gpiod_line_config_new();
    gpiod_request_config* request_config  = gpiod_request_config_new();
    auto cleanup = [&]() {
      if (settings) {
        gpiod_line_settings_free(settings);
      }
      if (line_config) {
        gpiod_line_config_free(line_config);
      }
      if (request_config) {
        gpiod_request_config_free(request_config);
      }
      gpiod_chip_close(chip);
    };

    if (!settings || !line_config || !request_config) {
      error_message = errno_message("Failed to allocate GPIO request");
      cleanup();
      LOG_WARN("GPIO{} request failed: {}", config.line_offset, error_message);
      return false;
    }

    int result = gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    if (result == 0) {
      result = gpiod_line_settings_set_output_value(settings, initial_value);
    }
    if (result == 0) {
      result = gpiod_line_config_add_line_settings(line_config, &config.line_offset, 1, settings);
    }
    gpiod_request_config_set_consumer(request_config, config.consumer.c_str());

    if (result == 0) {
      request = gpiod_chip_request_lines(chip, request_config, line_config);
    }

    if (result < 0 || !request) {
      error_message = errno_message("Failed to request GPIO line");
      cleanup();
      LOG_WARN("GPIO{} request failed: {}", config.line_offset, error_message);
      return false;
    }

    cleanup();
    return true;
  }
#endif

  void release_locked() {
#if APP_USE_LIBGPIOD
    if (request) {
      gpiod_line_request_release(request);
      request = nullptr;
    }
#endif
  }

  OutputLineConfig config;
  std::mutex mutex;
#if APP_USE_LIBGPIOD
  gpiod_line_request* request{nullptr};
#endif
};

OutputLine::OutputLine(OutputLineConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
OutputLine::~OutputLine() = default;

bool OutputLine::set_value(bool active, std::string& error_message) {
  return impl_->set_value(active, error_message);
}

void OutputLine::release() { impl_->release(); }

bool set_output_value(const OutputLineConfig& config, bool active, std::string& error_message) {
  static std::mutex lines_mutex;
  static std::map<std::string, std::unique_ptr<OutputLine>> lines;

  const auto key = config.chip_path + ":" + std::to_string(config.line_offset) + ":" +
                   config.consumer;
  std::lock_guard<std::mutex> lock(lines_mutex);
  auto& line = lines[key];
  if (!line) {
    line = std::make_unique<OutputLine>(config);
  }
  return line->set_value(active, error_message);
}

bool set_external_bus_i2c_mode(bool enabled, std::string& error_message) {
  static OutputLine external_bus_line({K_EXTERNAL_BUS_GPIO_CHIP,
                                       K_EXTERNAL_BUS_GPIO_LINE,
                                       K_EXTERNAL_BUS_GPIO_CONSUMER});
  const bool ok = external_bus_line.set_value(enabled, error_message);
  if (ok) {
    LOG_INFO("external bus switched to {} via GPIO4={}", enabled ? "I2C" : "UART", enabled ? 1 : 0);
  } else {
    LOG_WARN("external bus switch failed: {}", error_message);
  }
  return ok;
}

}  // namespace platform::gpio
