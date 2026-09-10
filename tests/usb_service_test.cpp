/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "usb_service.h"

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

void test_usb_physical_port_identity() {
  using platform::connectivity::UsbPortDeviceInfo;
  using platform::connectivity::is_usb_device_at_port;

  UsbPortDeviceInfo device;
  device.bus       = 1;
  device.device    = 5;
  device.port_path = "1.3";

  CHECK(is_usb_device_at_port(device, 1, "1.3"));
  CHECK(!is_usb_device_at_port(device, 1, "1.2"));
  CHECK(!is_usb_device_at_port(device, 2, "1.3"));

  device.device = 37;
  CHECK(is_usb_device_at_port(device, 1, "1.3"));

  device.device    = 4;
  device.port_path = "1.2";
  CHECK(is_usb_device_at_port(device, 1, "1.2"));
  CHECK(!is_usb_device_at_port(device, 1, "1.3"));
  device.device = 41;
  CHECK(is_usb_device_at_port(device, 1, "1.2"));
}

}  // namespace

int main() {
  test_usb_physical_port_identity();
  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "usb service tests passed\n";
  return 0;
}
