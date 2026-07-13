/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "usb_service.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "logger.h"

#if APP_USE_LIBUDEV
#include <libudev.h>
#endif
#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace platform::connectivity {
namespace {

bool same_usb_device(const UsbDeviceInfo& left, const UsbDeviceInfo& right) {
  return left.name == right.name && left.detail == right.detail && left.sys_path == right.sys_path;
}

bool same_usb_result(const std::vector<UsbDeviceInfo>& left,
                     const std::vector<UsbDeviceInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_usb_device);
}

struct UsbLogSnapshot {
  bool initialized{false};
  std::vector<UsbDeviceInfo> devices{};
  std::string error_message{};
};

bool update_usb_log_snapshot(UsbLogSnapshot& snapshot,
                             const std::vector<UsbDeviceInfo>& devices,
                             const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_usb_result(snapshot.devices, devices)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.devices       = devices;
  snapshot.error_message = error_message;
  return true;
}

void log_usb_result(const std::vector<UsbDeviceInfo>& devices,
                    const std::string& error_message,
                    bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity usb result unchanged: {} device(s)", devices.size());
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity usb result message: {}", error_message);
  }

  LOG_INFO("connectivity usb result: {} device(s)", devices.size());
  for (const auto& device : devices) {
    LOG_INFO("connectivity usb device: name='{}' detail='{}' syspath='{}'",
             device.name,
             device.detail,
             device.sys_path);
  }
}

#if APP_USE_LIBUDEV
struct UdevOwner {
  udev* context{nullptr};

  ~UdevOwner() {
    if (context) {
      udev_unref(context);
    }
  }
};

struct UdevEnumerateOwner {
  udev_enumerate* enumerate{nullptr};

  ~UdevEnumerateOwner() {
    if (enumerate) {
      udev_enumerate_unref(enumerate);
    }
  }
};

struct UdevDeviceOwner {
  udev_device* device{nullptr};

  ~UdevDeviceOwner() {
    if (device) {
      udev_device_unref(device);
    }
  }
};

std::string sysattr(udev_device* device, const char* name) {
  const char* value = device && name ? udev_device_get_sysattr_value(device, name) : nullptr;
  return value ? value : "";
}

std::string devtype(udev_device* device) {
  const char* value = device ? udev_device_get_devtype(device) : nullptr;
  return value ? value : "";
}

bool is_usb_bus_device(udev_device* device) { return devtype(device) == "usb_device"; }

std::string usb_host_controller_name(const std::string& manufacturer, const std::string& product) {
  const bool looks_like_linux_hcd =
      manufacturer.rfind("Linux ", 0) == 0 && manufacturer.find("hcd") != std::string::npos;
  if (looks_like_linux_hcd && !product.empty()) {
    return product;
  }

  const auto combined = manufacturer.empty() ? product : manufacturer + " " + product;
  const auto hcd_pos  = combined.find("hcd");
  if (combined.rfind("Linux ", 0) == 0 && hcd_pos != std::string::npos) {
    const auto name_start = combined.find_first_not_of(" _-", hcd_pos + 3);
    if (name_start != std::string::npos) {
      return combined.substr(name_start);
    }
  }

  return "";
}

std::string usb_device_name(udev_device* device) {
  auto product               = sysattr(device, "product");
  const auto vendor          = sysattr(device, "manufacturer");
  const auto host_controller = usb_host_controller_name(vendor, product);
  if (!host_controller.empty()) {
    return host_controller;
  }
  if (!vendor.empty() && !product.empty()) {
    return vendor + " " + product;
  }
  if (!product.empty()) {
    return product;
  }
  if (!vendor.empty()) {
    return vendor;
  }

  const auto vid = sysattr(device, "idVendor");
  const auto pid = sysattr(device, "idProduct");
  if (!vid.empty() && !pid.empty()) {
    return vid + ":" + pid;
  }
  return "USB Device";
}

std::string usb_device_detail(udev_device* device) {
  std::ostringstream detail;
  const auto vid = sysattr(device, "idVendor");
  const auto pid = sysattr(device, "idProduct");

  if (!vid.empty() || !pid.empty()) {
    detail << (vid.empty() ? "????" : vid) << ":" << (pid.empty() ? "????" : pid);
  }
  return detail.str();
}
#endif

}  // namespace

std::vector<UsbDeviceInfo> list_usb_devices(std::string& error_message) {
  static UsbLogSnapshot log_snapshot;
  error_message.clear();
#if APP_USE_LIBUDEV
  LOG_TRACE("connectivity USB enumeration requested");
  LOG_TRACE("connectivity initializing libudev context");
  UdevOwner udev_context{udev_new()};
  if (!udev_context.context) {
    error_message = "libudev is not available";
    const std::vector<UsbDeviceInfo> devices;
    const bool changed = update_usb_log_snapshot(log_snapshot, devices, error_message);
    log_usb_result(devices, error_message, changed);
    return {};
  }

  UdevEnumerateOwner enumerate{udev_enumerate_new(udev_context.context)};
  if (!enumerate.enumerate) {
    error_message = "failed to enumerate USB devices";
    const std::vector<UsbDeviceInfo> devices;
    const bool changed = update_usb_log_snapshot(log_snapshot, devices, error_message);
    log_usb_result(devices, error_message, changed);
    return {};
  }

  udev_enumerate_add_match_subsystem(enumerate.enumerate, "usb");
  if (udev_enumerate_scan_devices(enumerate.enumerate) < 0) {
    error_message = "failed to scan USB bus";
    const std::vector<UsbDeviceInfo> devices;
    const bool changed = update_usb_log_snapshot(log_snapshot, devices, error_message);
    log_usb_result(devices, error_message, changed);
    return {};
  }

  std::vector<UsbDeviceInfo> devices;
  auto* entries                        = udev_enumerate_get_list_entry(enumerate.enumerate);
  udev_list_entry* entry               = nullptr;
  std::size_t skipped_non_device_count = 0;
  udev_list_entry_foreach(entry, entries) {
    const char* path = udev_list_entry_get_name(entry);
    UdevDeviceOwner device{udev_device_new_from_syspath(udev_context.context, path)};
    if (!device.device) {
      continue;
    }

    const auto type = devtype(device.device);
    if (!is_usb_bus_device(device.device)) {
      ++skipped_non_device_count;
      LOG_TRACE("connectivity usb skipped non-device entry: devtype='{}' syspath='{}'",
                type,
                path ? path : "");
      continue;
    }

    UsbDeviceInfo info;
    info.name     = usb_device_name(device.device);
    info.detail   = usb_device_detail(device.device);
    info.sys_path = path ? path : "";
    devices.push_back(std::move(info));
  }

  std::sort(devices.begin(), devices.end(), [](const auto& left, const auto& right) {
    return left.sys_path < right.sys_path;
  });
  if (devices.empty()) {
    error_message = "No USB devices found";
  }
  LOG_TRACE("connectivity usb enumeration skipped {} non usb_device entrie(s)",
            skipped_non_device_count);
  const bool changed = update_usb_log_snapshot(log_snapshot, devices, error_message);
  log_usb_result(devices, error_message, changed);
  return devices;
#else
  LOG_TRACE("connectivity USB enumeration requested but libudev backend is disabled");
  error_message = "libudev backend is not enabled";
  const std::vector<UsbDeviceInfo> devices;
  const bool changed = update_usb_log_snapshot(log_snapshot, devices, error_message);
  log_usb_result(devices, error_message, changed);
  return {};
#endif
}
}  // namespace platform::connectivity
