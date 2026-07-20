/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "gpio_service.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

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

std::string line_key(const OutputLineConfig& config) {
  return config.chip_path + ":" + std::to_string(config.line_offset) + ":" + config.consumer;
}

std::mutex& output_lines_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::map<std::string, std::unique_ptr<OutputLine>>& output_lines() {
  static std::map<std::string, std::unique_ptr<OutputLine>> lines;
  return lines;
}

#if APP_USE_LIBGPIOD
struct LineRequestOwner {
  gpiod_line_request* request{nullptr};

  ~LineRequestOwner() {
    if (request) {
      gpiod_line_request_release(request);
    }
  }

  LineRequestOwner() = default;
  explicit LineRequestOwner(gpiod_line_request* line_request)
      : request(line_request) {}

  LineRequestOwner(const LineRequestOwner&)            = delete;
  LineRequestOwner& operator=(const LineRequestOwner&) = delete;

  gpiod_line_request* release() {
    auto* result = request;
    request      = nullptr;
    return result;
  }
};

bool request_line(const OutputLineConfig& config,
                  gpiod_line_direction direction,
                  gpiod_line_value initial_value,
                  LineRequestOwner& owner,
                  std::string& error_message) {
  error_message.clear();

  gpiod_chip* chip = gpiod_chip_open(config.chip_path.c_str());
  if (!chip) {
    error_message = errno_message((std::string("Failed to open ") + config.chip_path).c_str());
    return false;
  }

  gpiod_line_settings* settings        = gpiod_line_settings_new();
  gpiod_line_config* line_config       = gpiod_line_config_new();
  gpiod_request_config* request_config = gpiod_request_config_new();
  auto cleanup                         = [&]() {
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
    return false;
  }

  int result = gpiod_line_settings_set_direction(settings, direction);
  if (result == 0 && direction == GPIOD_LINE_DIRECTION_OUTPUT) {
    result = gpiod_line_settings_set_drive(settings, GPIOD_LINE_DRIVE_PUSH_PULL);
  }
  if (result == 0) {
    gpiod_line_settings_set_active_low(settings, false);
  }
  if (result == 0 && direction == GPIOD_LINE_DIRECTION_OUTPUT) {
    result = gpiod_line_settings_set_output_value(settings, initial_value);
  }
  if (result == 0) {
    result = gpiod_line_config_add_line_settings(line_config, &config.line_offset, 1, settings);
  }
  gpiod_request_config_set_consumer(request_config, config.consumer.c_str());

  if (result == 0) {
    owner.request = gpiod_chip_request_lines(chip, request_config, line_config);
  }
  if (result < 0 || !owner.request) {
    error_message = errno_message("Failed to request GPIO line");
    cleanup();
    return false;
  }

  cleanup();
  return true;
}

bool read_requested_value(const OutputLineConfig& config,
                          gpiod_line_request* request,
                          bool& active,
                          std::string& error_message) {
  const auto value = gpiod_line_request_get_value(request, config.line_offset);
  if (value == GPIOD_LINE_VALUE_ERROR) {
    error_message = errno_message("Failed to read GPIO value");
    LOG_WARN("GPIO{} read failed: {}", config.line_offset, error_message);
    return false;
  }

  active = value == GPIOD_LINE_VALUE_ACTIVE;
  return true;
}

bool read_line_value_preserving_direction(const OutputLineConfig& config,
                                          bool& active,
                                          gpiod_line_request*& retained_request,
                                          std::string& error_message) {
  LineRequestOwner owner;
  if (!request_line(config,
                    GPIOD_LINE_DIRECTION_AS_IS,
                    GPIOD_LINE_VALUE_INACTIVE,
                    owner,
                    error_message)) {
    LOG_WARN("GPIO{} read failed: {}", config.line_offset, error_message);
    return false;
  }

  if (!read_requested_value(config, owner.request, active, error_message)) {
    return false;
  }

  retained_request = owner.release();
  return true;
}
#endif

}  // namespace

struct OutputLine::Impl {
  explicit Impl(OutputLineConfig line_config)
      : config(std::move(line_config)) {}

  ~Impl() { release_locked(); }

  bool set_value(bool active, std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex);
    error_message.clear();

#if APP_USE_LIBGPIOD
    const auto value = active ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    if (!request || !output_mode) {
      release_locked();
      if (!request_line_locked(GPIOD_LINE_DIRECTION_OUTPUT, value, error_message)) {
        return false;
      }
    }

    if (gpiod_line_request_set_value(request, config.line_offset, value) == 0) {
      bool readback = false;
      if (!read_requested_value(config, request, readback, error_message)) {
        return false;
      }
      if (readback == active) {
        return true;
      }
      error_message = "GPIO output readback mismatch";
      LOG_WARN("GPIO{} output readback mismatch: requested={} actual={}",
               config.line_offset,
               active ? 1 : 0,
               readback ? 1 : 0);
      return false;
    }

    // If the request became invalid, release and try once more with the requested value.
    LOG_WARN("GPIO{} set failed, retrying request: {}", config.line_offset, std::strerror(errno));
    release_locked();
    if (!request_line_locked(GPIOD_LINE_DIRECTION_OUTPUT, value, error_message)) {
      return false;
    }
    if (gpiod_line_request_set_value(request, config.line_offset, value) == 0) {
      bool readback = false;
      if (!read_requested_value(config, request, readback, error_message)) {
        return false;
      }
      if (readback == active) {
        return true;
      }
      error_message = "GPIO output readback mismatch after retry";
      LOG_WARN("GPIO{} output readback mismatch after retry: requested={} actual={}",
               config.line_offset,
               active ? 1 : 0,
               readback ? 1 : 0);
      return false;
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

  bool set_input(std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex);
    error_message.clear();

#if APP_USE_LIBGPIOD
    release_locked();
    return request_line_locked(GPIOD_LINE_DIRECTION_INPUT,
                               GPIOD_LINE_VALUE_INACTIVE,
                               error_message);
#else
    error_message = "libgpiod headers/library not found; cannot configure GPIO input";
    LOG_WARN("GPIO{} unavailable: {}", config.line_offset, error_message);
    return false;
#endif
  }

  void release() {
    std::lock_guard<std::mutex> lock(mutex);
    release_locked();
  }

  bool get_value(bool& active, std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex);
    error_message.clear();

#if APP_USE_LIBGPIOD
    if (!request) {
      return read_line_value_preserving_direction(config, active, request, error_message);
    }

    return read_requested_value(config, request, active, error_message);
#else
    (void)active;
    error_message = "libgpiod headers/library not found; cannot read GPIO";
    LOG_WARN("GPIO{} unavailable: {}", config.line_offset, error_message);
    return false;
#endif
  }

#if APP_USE_LIBGPIOD
  bool request_line_locked(gpiod_line_direction direction,
                           gpiod_line_value initial_value,
                           std::string& error_message) {
    LineRequestOwner owner;
    if (!request_line(config, direction, initial_value, owner, error_message)) {
      LOG_WARN("GPIO{} request failed: {}", config.line_offset, error_message);
      return false;
    }

    request     = owner.release();
    output_mode = direction == GPIOD_LINE_DIRECTION_OUTPUT;
    return true;
  }
#endif

  void release_locked() {
#if APP_USE_LIBGPIOD
    if (request) {
      gpiod_line_request_release(request);
      request = nullptr;
    }
    output_mode = false;
#endif
  }

  OutputLineConfig config;
  std::mutex mutex;
#if APP_USE_LIBGPIOD
  gpiod_line_request* request{nullptr};
  bool output_mode{false};
#endif
};

OutputLine::OutputLine(OutputLineConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
OutputLine::~OutputLine() = default;

bool OutputLine::set_value(bool active, std::string& error_message) {
  return impl_->set_value(active, error_message);
}

bool OutputLine::set_input(std::string& error_message) { return impl_->set_input(error_message); }

bool OutputLine::get_value(bool& active, std::string& error_message) {
  return impl_->get_value(active, error_message);
}

void OutputLine::release() { impl_->release(); }

bool set_output_value(const OutputLineConfig& config, bool active, std::string& error_message) {
  std::lock_guard<std::mutex> lock(output_lines_mutex());
  auto& line = output_lines()[line_key(config)];
  if (!line) {
    line = std::make_unique<OutputLine>(config);
  }
  return line->set_value(active, error_message);
}

bool set_input_mode(const OutputLineConfig& config, std::string& error_message) {
  std::lock_guard<std::mutex> lock(output_lines_mutex());
  auto& line = output_lines()[line_key(config)];
  if (!line) {
    line = std::make_unique<OutputLine>(config);
  }
  return line->set_input(error_message);
}

bool get_output_value(const OutputLineConfig& config, bool& active, std::string& error_message) {
  std::lock_guard<std::mutex> lock(output_lines_mutex());
  const auto it = output_lines().find(line_key(config));
  if (it != output_lines().end() && it->second) {
    return it->second->get_value(active, error_message);
  }

#if APP_USE_LIBGPIOD
  gpiod_line_request* retained_request = nullptr;
  const bool ok =
      read_line_value_preserving_direction(config, active, retained_request, error_message);
  if (retained_request) {
    gpiod_line_request_release(retained_request);
  }
  return ok;
#else
  (void)active;
  error_message = "libgpiod headers/library not found; cannot read GPIO";
  LOG_WARN("GPIO{} unavailable: {}", config.line_offset, error_message);
  return false;
#endif
}

void release_output_value(const OutputLineConfig& config) {
  std::lock_guard<std::mutex> lock(output_lines_mutex());
  const auto it = output_lines().find(line_key(config));
  if (it != output_lines().end()) {
    if (it->second) {
      it->second->release();
    }
    output_lines().erase(it);
  }
}

bool get_input_value(const OutputLineConfig& config, bool& active, std::string& error_message) {
  {
    std::lock_guard<std::mutex> lock(output_lines_mutex());
    const auto it = output_lines().find(line_key(config));
    if (it != output_lines().end()) {
      if (it->second) {
        it->second->release();
      }
      output_lines().erase(it);
    }
  }

#if APP_USE_LIBGPIOD
  LineRequestOwner owner;
  if (!request_line(config,
                    GPIOD_LINE_DIRECTION_INPUT,
                    GPIOD_LINE_VALUE_INACTIVE,
                    owner,
                    error_message)) {
    LOG_WARN("GPIO{} input request failed: {}", config.line_offset, error_message);
    return false;
  }

  return read_requested_value(config, owner.request, active, error_message);
#else
  (void)active;
  error_message = "libgpiod headers/library not found; cannot read GPIO input";
  LOG_WARN("GPIO{} unavailable: {}", config.line_offset, error_message);
  return false;
#endif
}

bool set_external_bus_i2c_mode(bool enabled, std::string& error_message) {
  static OutputLine external_bus_line(
      {K_EXTERNAL_BUS_GPIO_CHIP, K_EXTERNAL_BUS_GPIO_LINE, K_EXTERNAL_BUS_GPIO_CONSUMER});
  const bool ok = external_bus_line.set_value(enabled, error_message);
  if (ok) {
    LOG_INFO("external bus switched to {} via GPIO4={}", enabled ? "I2C" : "UART", enabled ? 1 : 0);
  } else {
    LOG_WARN("external bus switch failed: {}", error_message);
  }
  return ok;
}

bool set_external_bus_uart_mode(std::string& error_message) {
  return set_external_bus_i2c_mode(false, error_message);
}

}  // namespace platform::gpio
