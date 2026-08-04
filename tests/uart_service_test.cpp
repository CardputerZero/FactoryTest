/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "uart_service.h"

#include <iostream>

namespace {

int failures = 0;

#define CHECK(condition)                                                                    \
  do {                                                                                      \
    if (!(condition)) {                                                                     \
      std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " << #condition << '\n'; \
      ++failures;                                                                           \
    }                                                                                       \
  } while (false)

void test_usb_serial_instance_identity() {
  using platform::connectivity::same_usb_serial_instance;
  using platform::connectivity::UsbSerialPortInfo;

  CHECK(!same_usb_serial_instance({}, {}));
  CHECK(!same_usb_serial_instance({"/dev/ttyACM0", 1, 4}, {"/dev/ttyACM1", 1, 4}));
  CHECK(same_usb_serial_instance({"/dev/ttyACM0", 1, 4}, {"/dev/ttyACM0", 1, 4}));
  CHECK(!same_usb_serial_instance({"/dev/ttyACM0", 1, 4}, {"/dev/ttyACM0", 1, 5}));
  CHECK(same_usb_serial_instance({"/dev/ttyACM0", -1, -1}, {"/dev/ttyACM0", -1, -1}));
}

}  // namespace

int main() {
  test_usb_serial_instance_identity();
  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "uart service tests passed\n";
  return 0;
}
