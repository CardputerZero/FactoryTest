/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cap_fixture_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

#include "gpio_service.h"
#include "logger.h"
#include "power_service.h"
#include "usb_service.h"

#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/spi/spidev.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace platform::fixture {
namespace {

constexpr auto K_POLL_INTERVAL = std::chrono::milliseconds(50);
const std::array<platform::gpio::OutputLineConfig, 3> K_GPIO_TEST_LINES = {{
    {"/dev/gpiochip0", 26, "factory-test-cap-gpio26"},
    {"/dev/gpiochip0", 23, "factory-test-cap-gpio23"},
    {"/dev/gpiochip0", 22, "factory-test-cap-gpio22"},
}};

std::string errno_error(const char* operation) {
  return std::string(operation ? operation : "operation") + ": " + std::strerror(errno);
}

bool write_switch_file(const char* path, bool enabled, std::string& error_message) {
  error_message.clear();
  errno = 0;
  std::ofstream output(path);
  if (!output) {
    error_message = errno_error((std::string("open ") + path).c_str());
    return false;
  }
  output << (enabled ? 1 : 0) << '\n';
  if (!output) {
    error_message = errno_error((std::string("write ") + path).c_str());
    return false;
  }
  output.close();

  int actual = -1;
  for (int attempt = 1; attempt <= 6; ++attempt) {
    std::ifstream input(path);
    input >> actual;
    if (input && ((actual != 0) == enabled)) {
      return true;
    }
    if (attempt < 6) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  LOG_WARN("[CAP-FIXTURE][SYSFS] write accepted but readback stayed at {}: path={} requested={}",
           actual,
           path,
           enabled ? 1 : 0);
  error_message.clear();
  return true;
}

bool target_usb_present(uint16_t vendor_id,
                        uint16_t product_id,
                        std::string* sys_path,
                        std::string& error_message) {
  auto devices = platform::connectivity::list_usb_devices(error_message);
  char expected[10]{};
  std::snprintf(expected, sizeof(expected), "%04x:%04x", vendor_id, product_id);
  std::string expected_text(expected);
  std::transform(expected_text.begin(), expected_text.end(), expected_text.begin(), [](char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  });

  for (const auto& device : devices) {
    auto detail = device.detail;
    std::transform(detail.begin(), detail.end(), detail.begin(), [](char ch) {
      return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    });
    if (detail == expected_text) {
      if (sys_path) {
        *sys_path = device.sys_path;
      }
      error_message.clear();
      return true;
    }
  }
  error_message.clear();
  return false;
}

bool wait_cancelable(std::chrono::milliseconds duration, const std::atomic_bool& cancel_requested) {
  const auto deadline = std::chrono::steady_clock::now() + duration;
  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested.load()) {
      return false;
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    std::this_thread::sleep_for(std::min(K_POLL_INTERVAL, remaining));
  }
  return !cancel_requested.load();
}

#if defined(__linux__)
struct FileDescriptor {
  int value{-1};
  explicit FileDescriptor(int fd = -1)
      : value(fd) {}
  FileDescriptor(const FileDescriptor&)            = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  FileDescriptor(FileDescriptor&& other) noexcept
      : value(other.value) {
    other.value = -1;
  }
  FileDescriptor& operator=(FileDescriptor&& other) noexcept {
    if (this != &other) {
      if (value >= 0) {
        close(value);
      }
      value       = other.value;
      other.value = -1;
    }
    return *this;
  }
  ~FileDescriptor() {
    if (value >= 0) {
      close(value);
    }
  }
};

std::vector<std::string> find_target_input_events(const std::string& usb_sys_path) {
  std::vector<std::string> events;
  std::error_code ec;
  const std::filesystem::path input_root("/sys/class/input");
  for (const auto& entry : std::filesystem::directory_iterator(input_root, ec)) {
    if (ec) {
      break;
    }
    const auto name = entry.path().filename().string();
    if (name.rfind("event", 0) != 0) {
      continue;
    }
    const auto canonical = std::filesystem::weakly_canonical(entry.path(), ec).string();
    if (!ec && !usb_sys_path.empty() && canonical.rfind(usb_sys_path, 0) == 0) {
      events.push_back("/dev/input/" + name);
    }
    ec.clear();
  }
  return events;
}

char keycode_to_ascii(uint16_t code, bool shift) {
  char letter = '\0';
  switch (code) {
    case KEY_A:
      letter = 'a';
      break;
    case KEY_B:
      letter = 'b';
      break;
    case KEY_C:
      letter = 'c';
      break;
    case KEY_D:
      letter = 'd';
      break;
    case KEY_E:
      letter = 'e';
      break;
    case KEY_F:
      letter = 'f';
      break;
    case KEY_G:
      letter = 'g';
      break;
    case KEY_H:
      letter = 'h';
      break;
    case KEY_I:
      letter = 'i';
      break;
    case KEY_J:
      letter = 'j';
      break;
    case KEY_K:
      letter = 'k';
      break;
    case KEY_L:
      letter = 'l';
      break;
    case KEY_M:
      letter = 'm';
      break;
    case KEY_N:
      letter = 'n';
      break;
    case KEY_O:
      letter = 'o';
      break;
    case KEY_P:
      letter = 'p';
      break;
    case KEY_Q:
      letter = 'q';
      break;
    case KEY_R:
      letter = 'r';
      break;
    case KEY_S:
      letter = 's';
      break;
    case KEY_T:
      letter = 't';
      break;
    case KEY_U:
      letter = 'u';
      break;
    case KEY_V:
      letter = 'v';
      break;
    case KEY_W:
      letter = 'w';
      break;
    case KEY_X:
      letter = 'x';
      break;
    case KEY_Y:
      letter = 'y';
      break;
    case KEY_Z:
      letter = 'z';
      break;
    default:
      break;
  }
  if (letter != '\0') {
    return shift ? static_cast<char>(std::toupper(static_cast<unsigned char>(letter))) : letter;
  }
  if (code >= KEY_1 && code <= KEY_9) {
    return static_cast<char>('1' + (code - KEY_1));
  }
  if (code == KEY_0) {
    return '0';
  }
  if (code == KEY_MINUS) {
    return shift ? '_' : '-';
  }
  return '\0';
}
#endif

}  // namespace

bool write_register(uint8_t reg,
                    const uint8_t* data,
                    std::size_t size,
                    std::string& error_message) {
  error_message.clear();
  if (size > 255 || (size > 0 && !data)) {
    error_message = "invalid I2C register write buffer";
    return false;
  }
#if defined(__linux__)
  std::vector<uint8_t> buffer(size + 1);
  buffer[0] = reg;
  if (size > 0) {
    std::copy(data, data + size, buffer.begin() + 1);
  }
  FileDescriptor bus{open("/dev/i2c-1", O_RDWR | O_CLOEXEC)};
  if (bus.value < 0) {
    error_message = errno_error("open /dev/i2c-1");
    return false;
  }
  i2c_msg message{};
  message.addr  = K_I2C_ADDRESS;
  message.flags = 0;
  message.len   = static_cast<__u16>(buffer.size());
  message.buf   = buffer.data();
  i2c_rdwr_ioctl_data transfer{&message, 1};
  if (ioctl(bus.value, I2C_RDWR, &transfer) < 0) {
    error_message = errno_error("I2C register write");
    return false;
  }
  return true;
#else
  (void)reg;
  (void)data;
  (void)size;
  error_message = "Linux I2C backend is not available";
  return false;
#endif
}

bool write_register(uint8_t reg, uint8_t value, std::string& error_message) {
  return write_register(reg, &value, 1, error_message);
}

bool read_register(uint8_t reg, uint8_t* data, std::size_t size, std::string& error_message) {
  error_message.clear();
  if (!data || size == 0 || size > 255) {
    error_message = "invalid I2C register read buffer";
    return false;
  }
#if defined(__linux__)
  FileDescriptor bus{open("/dev/i2c-1", O_RDWR | O_CLOEXEC)};
  if (bus.value < 0) {
    error_message = errno_error("open /dev/i2c-1");
    return false;
  }
  std::array<i2c_msg, 2> messages{};
  messages[0].addr  = K_I2C_ADDRESS;
  messages[0].flags = 0;
  messages[0].len   = 1;
  messages[0].buf   = &reg;
  messages[1].addr  = K_I2C_ADDRESS;
  messages[1].flags = I2C_M_RD;
  messages[1].len   = static_cast<__u16>(size);
  messages[1].buf   = data;
  i2c_rdwr_ioctl_data transfer{messages.data(), static_cast<__u32>(messages.size())};
  if (ioctl(bus.value, I2C_RDWR, &transfer) < 0) {
    error_message = errno_error("I2C repeated-start register read");
    return false;
  }
  return true;
#else
  (void)reg;
  (void)data;
  (void)size;
  error_message = "Linux I2C backend is not available";
  return false;
#endif
}

bool set_ext_5v_enabled(bool enabled, std::string& error_message) {
  return write_switch_file(K_EXT_5V_PATH, enabled, error_message);
}

bool set_usb_gpio_function(bool usb_enabled, std::string& error_message) {
  return write_switch_file(K_USB_GPIO_FUNCTION_PATH, usb_enabled, error_message);
}

bool read_charge_report(uint8_t& report, std::string& detail, std::string& error_message) {
  platform::power::PowerSupplyInfo info;
  if (!platform::power::read_battery_info(info, error_message)) {
    return false;
  }
  auto status = info.status;
  std::transform(status.begin(), status.end(), status.begin(), [](char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  });
  if (status == "charging") {
    report = 0x01;
  } else if (info.capacity_percent == 100) {
    report = 0x02;
  } else {
    report = 0x00;
  }
  detail = "status=" + info.status + " capacity=" + std::to_string(info.capacity_percent) + "%";
  error_message.clear();
  return true;
}

bool spi_transfer(const std::vector<uint8_t>& tx,
                  std::vector<uint8_t>& rx,
                  uint32_t speed_hz,
                  std::string& error_message) {
  error_message.clear();
  if (tx.empty()) {
    error_message = "SPI transfer buffer is empty";
    return false;
  }
#if defined(__linux__)
  FileDescriptor device{open(K_SPI_PATH, O_RDWR | O_CLOEXEC)};
  if (device.value < 0) {
    error_message = errno_error("open /dev/spidev0.1");
    return false;
  }
  uint8_t mode      = SPI_MODE_0;
  uint8_t bits      = 8;
  uint8_t lsb_first = 0;
  if (ioctl(device.value, SPI_IOC_WR_MODE, &mode) < 0 ||
      ioctl(device.value, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
      ioctl(device.value, SPI_IOC_WR_LSB_FIRST, &lsb_first) < 0 ||
      ioctl(device.value, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
    error_message = errno_error("configure /dev/spidev0.1");
    return false;
  }
  rx.assign(tx.size(), 0);
  spi_ioc_transfer transfer{};
  transfer.tx_buf        = reinterpret_cast<__u64>(tx.data());
  transfer.rx_buf        = reinterpret_cast<__u64>(rx.data());
  transfer.len           = static_cast<__u32>(tx.size());
  transfer.speed_hz      = speed_hz;
  transfer.bits_per_word = bits;
  transfer.cs_change     = 0;
  if (ioctl(device.value, SPI_IOC_MESSAGE(1), &transfer) < 0) {
    error_message = errno_error("SPI_IOC_MESSAGE");
    return false;
  }
  return true;
#else
  (void)rx;
  (void)speed_hz;
  error_message = "Linux SPI backend is not available";
  return false;
#endif
}

bool set_gpio_test_outputs(bool active, std::string& error_message) {
  for (std::size_t i = 0; i < K_GPIO_TEST_LINES.size(); ++i) {
    if (!platform::gpio::set_output_value(K_GPIO_TEST_LINES[i], active, error_message)) {
      for (std::size_t release = 0; release < i; ++release) {
        platform::gpio::release_output_value(K_GPIO_TEST_LINES[release]);
      }
      return false;
    }
  }
  return true;
}

bool read_gpio_test_output_levels(uint8_t& levels, std::string& error_message) {
  levels = 0;
  for (std::size_t i = 0; i < K_GPIO_TEST_LINES.size(); ++i) {
    bool active = false;
    if (!platform::gpio::get_output_value(K_GPIO_TEST_LINES[i], active, error_message)) {
      return false;
    }
    if (active) {
      levels |= static_cast<uint8_t>(1U << i);
    }
  }
  error_message.clear();
  return true;
}

bool set_gpio_test_inputs(std::string& error_message) {
  bool all_configured = true;
  std::string first_error;
  for (const auto& line : K_GPIO_TEST_LINES) {
    std::string line_error;
    if (!platform::gpio::set_input_mode(line, line_error)) {
      all_configured = false;
      if (first_error.empty()) {
        first_error = line_error;
      }
    }
  }
  error_message = first_error;
  return all_configured;
}

bool wait_for_usb_device(uint16_t vendor_id,
                         uint16_t product_id,
                         std::chrono::milliseconds timeout,
                         const std::atomic_bool& cancel_requested,
                         std::string& sys_path,
                         std::string& error_message) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested.load()) {
      error_message = "USB wait canceled";
      return false;
    }
    if (target_usb_present(vendor_id, product_id, &sys_path, error_message)) {
      return true;
    }
    if (!wait_cancelable(K_POLL_INTERVAL, cancel_requested)) {
      error_message = "USB wait canceled";
      return false;
    }
  }
  error_message = "timed out waiting for USB 303a:1001";
  return false;
}

bool wait_for_usb_disconnect(uint16_t vendor_id,
                             uint16_t product_id,
                             std::chrono::milliseconds timeout,
                             const std::atomic_bool& cancel_requested,
                             std::string& error_message) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested.load()) {
      error_message = "USB disconnect wait canceled";
      return false;
    }
    if (!target_usb_present(vendor_id, product_id, nullptr, error_message)) {
      error_message.clear();
      return true;
    }
    if (!wait_cancelable(K_POLL_INTERVAL, cancel_requested)) {
      error_message = "USB disconnect wait canceled";
      return false;
    }
  }
  error_message = "timed out waiting for USB disconnect";
  return false;
}

bool receive_usb_hid_text(uint16_t vendor_id,
                          uint16_t product_id,
                          const std::string& expected,
                          std::chrono::milliseconds timeout,
                          const std::atomic_bool& cancel_requested,
                          const std::function<bool(std::string&)>& ready_action,
                          std::string& received,
                          std::string& error_message) {
  received.clear();
  error_message.clear();
#if defined(__linux__)
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::vector<FileDescriptor> inputs;
  bool shift      = false;
  bool ready_sent = false;
  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_requested.load()) {
      error_message = "USB HID receive canceled";
      return false;
    }
    if (inputs.empty()) {
      std::string usb_path;
      std::string ignored;
      if (target_usb_present(vendor_id, product_id, &usb_path, ignored)) {
        for (const auto& event_path : find_target_input_events(usb_path)) {
          const int fd = open(event_path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
          if (fd >= 0) {
            LOG_INFO("CAP fixture USB HID input opened: {}", event_path);
            inputs.emplace_back(fd);
          }
        }
      }
      if (inputs.empty()) {
        wait_cancelable(K_POLL_INTERVAL, cancel_requested);
        continue;
      }
    }
    if (!ready_sent) {
      if (!ready_action || !ready_action(error_message)) {
        if (error_message.empty()) {
          error_message = "USB HID ready action failed";
        }
        return false;
      }
      ready_sent = true;
    }

    std::vector<pollfd> poll_fds;
    poll_fds.reserve(inputs.size());
    for (const auto& input : inputs) {
      poll_fds.push_back({input.value, POLLIN, 0});
    }
    const int poll_result = poll(poll_fds.data(), poll_fds.size(), 50);
    if (poll_result < 0 && errno != EINTR) {
      error_message = errno_error("poll USB HID input");
      return false;
    }
    for (std::size_t i = 0; i < poll_fds.size(); ++i) {
      if ((poll_fds[i].revents & POLLIN) == 0) {
        continue;
      }
      input_event event{};
      while (read(inputs[i].value, &event, sizeof(event)) == sizeof(event)) {
        if (event.type != EV_KEY) {
          continue;
        }
        if (event.code == KEY_LEFTSHIFT || event.code == KEY_RIGHTSHIFT) {
          shift = event.value != 0;
          continue;
        }
        if (event.value != 1) {
          continue;
        }
        const char ch = keycode_to_ascii(event.code, shift);
        if (ch == '\0') {
          continue;
        }
        received.push_back(ch);
        if (received == expected) {
          return true;
        }
        if (received.size() >= expected.size() ||
            expected.compare(0, received.size(), received) != 0) {
          error_message = "USB HID payload mismatch: " + received;
          return false;
        }
      }
    }
  }
  error_message = "timed out waiting for USB HID payload; received='" + received + "'";
  return false;
#else
  (void)vendor_id;
  (void)product_id;
  (void)expected;
  (void)timeout;
  (void)cancel_requested;
  (void)ready_action;
  error_message = "Linux USB HID backend is not available";
  return false;
#endif
}

}  // namespace platform::fixture
