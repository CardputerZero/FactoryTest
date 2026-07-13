/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "bluetooth_service.h"

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

#if APP_USE_BLUEZ
#include <gio/gio.h>
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

bool same_wireless_item(const WirelessScanItem& left, const WirelessScanItem& right) {
  return left.name == right.name && left.detail == right.detail &&
         left.strength_percent == right.strength_percent;
}

bool same_wireless_result(const std::vector<WirelessScanItem>& left,
                          const std::vector<WirelessScanItem>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_wireless_item);
}

bool wireless_result_sort_less(const WirelessScanItem& left, const WirelessScanItem& right) {
  if (left.strength_percent != right.strength_percent) {
    return left.strength_percent > right.strength_percent;
  }
  if (left.name != right.name) {
    return left.name < right.name;
  }
  return left.detail < right.detail;
}


struct WirelessLogSnapshot {
  bool initialized{false};
  std::vector<WirelessScanItem> items{};
  std::string error_message{};
};

bool update_wireless_log_snapshot(WirelessLogSnapshot& snapshot,
                                  const std::vector<WirelessScanItem>& items,
                                  const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_wireless_result(snapshot.items, items)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.items         = items;
  snapshot.error_message = error_message;
  return true;
}

void log_wireless_result(const char* source,
                         const std::vector<WirelessScanItem>& items,
                         const std::string& error_message,
                         bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity {} result unchanged: {} item(s)", source, items.size());
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity {} result message: {}", source, error_message);
  }

  LOG_INFO("connectivity {} result: {} item(s)", source, items.size());
  for (const auto& item : items) {
    LOG_INFO("connectivity {} item: name='{}' strength={} detail='{}'",
             source,
             item.name,
             item.strength_percent,
             item.detail);
  }
}

#if APP_USE_BLUEZ
struct GErrorOwner {
  GError* error{nullptr};

  ~GErrorOwner() {
    if (error) {
      g_error_free(error);
    }
  }
};

std::string take_error(GErrorOwner& owner, const char* fallback) {
  if (owner.error && owner.error->message) {
    return owner.error->message;
  }
  return fallback ? fallback : "NetworkManager error";
}

struct DBusConnectionOwner {
  GDBusConnection* connection{nullptr};

  ~DBusConnectionOwner() {
    if (connection) {
      g_object_unref(connection);
    }
  }
};

struct GVariantOwner {
  GVariant* value{nullptr};

  ~GVariantOwner() {
    if (value) {
      g_variant_unref(value);
    }
  }
};

DBusConnectionOwner make_system_bus(std::string& error_message) {
  LOG_TRACE("connectivity initializing BlueZ system D-Bus connection");
  GErrorOwner error;
  DBusConnectionOwner owner{g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error.error)};
  if (!owner.connection) {
    error_message = take_error(error, "D-Bus system bus is not available");
    LOG_TRACE("connectivity BlueZ system D-Bus initialization failed: {}", error_message);
  } else {
    LOG_TRACE("connectivity BlueZ system D-Bus connection initialized");
  }
  return owner;
}

GVariantOwner bluez_managed_objects(GDBusConnection* connection, std::string& error_message) {
  GErrorOwner error;
  GVariantOwner reply{g_dbus_connection_call_sync(connection,
                                                  "org.bluez",
                                                  "/",
                                                  "org.freedesktop.DBus.ObjectManager",
                                                  "GetManagedObjects",
                                                  nullptr,
                                                  G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                                                  G_DBUS_CALL_FLAGS_NONE,
                                                  1500,
                                                  nullptr,
                                                  &error.error)};
  if (!reply.value) {
    error_message = "Bluetooth is not enabled";
    LOG_TRACE("connectivity BlueZ ObjectManager query failed: {}", error_message);
  } else {
    LOG_TRACE("connectivity BlueZ ObjectManager query completed");
  }
  return reply;
}

std::string variant_string_property(GVariant* props, const char* name) {
  GVariantOwner value{g_variant_lookup_value(props, name, nullptr)};
  if (!value.value) {
    return "";
  }

  if (g_variant_is_of_type(value.value, G_VARIANT_TYPE_STRING) ||
      g_variant_is_of_type(value.value, G_VARIANT_TYPE_OBJECT_PATH)) {
    const char* text = g_variant_get_string(value.value, nullptr);
    return text ? text : "";
  }
  return "";
}

int16_t variant_int16_property(GVariant* props, const char* name, bool& found) {
  found = false;
  GVariantOwner value{g_variant_lookup_value(props, name, G_VARIANT_TYPE_INT16)};
  if (!value.value) {
    return 0;
  }
  found = true;
  return g_variant_get_int16(value.value);
}

bool variant_bool_property(GVariant* props, const char* name) {
  GVariantOwner value{g_variant_lookup_value(props, name, G_VARIANT_TYPE_BOOLEAN)};
  return value.value && g_variant_get_boolean(value.value);
}

std::string first_adapter_path(GVariant* managed_reply) {
  GVariant* objects_raw = nullptr;
  g_variant_get(managed_reply, "(@a{oa{sa{sv}}})", &objects_raw);
  GVariantOwner objects{objects_raw};

  GVariantIter iter;
  g_variant_iter_init(&iter, objects.value);

  const char* path = nullptr;
  GVariant* ifaces = nullptr;
  while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &path, &ifaces)) {
    GVariantOwner ifaces_owner{ifaces};
    GVariantOwner adapter_props{
        g_variant_lookup_value(ifaces_owner.value, "org.bluez.Adapter1", G_VARIANT_TYPE("a{sv}"))};
    if (adapter_props.value) {
      return path ? path : "";
    }
  }
  return "";
}

bool set_bluez_adapter_powered(GDBusConnection* connection,
                               const std::string& adapter_path,
                               bool powered,
                               std::string& error_message) {
  if (!connection || adapter_path.empty()) {
    return false;
  }

  GErrorOwner error;
  GVariantOwner reply{g_dbus_connection_call_sync(
      connection,
      "org.bluez",
      adapter_path.c_str(),
      "org.freedesktop.DBus.Properties",
      "Set",
      g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(powered)),
      nullptr,
      G_DBUS_CALL_FLAGS_NONE,
      1500,
      nullptr,
      &error.error)};
  if (!reply.value) {
    error_message = take_error(error, "Failed to power on Bluetooth adapter");
    LOG_TRACE("connectivity BlueZ adapter power request failed: {}", error_message);
    return false;
  }

  LOG_TRACE("connectivity BlueZ adapter '{}' powered={}", adapter_path, powered);
  return true;
}

void start_bluez_discovery(GDBusConnection* connection,
                           const std::string& adapter_path,
                           std::string& error_message) {
  if (adapter_path.empty()) {
    error_message = "No Bluetooth adapter found";
    LOG_TRACE("connectivity BlueZ discovery skipped: {}", error_message);
    return;
  }

  LOG_TRACE("connectivity BlueZ starting discovery on adapter '{}'", adapter_path);
  GErrorOwner error;
  GVariantOwner reply{g_dbus_connection_call_sync(connection,
                                                  "org.bluez",
                                                  adapter_path.c_str(),
                                                  "org.bluez.Adapter1",
                                                  "StartDiscovery",
                                                  nullptr,
                                                  nullptr,
                                                  G_DBUS_CALL_FLAGS_NONE,
                                                  1200,
                                                  nullptr,
                                                  &error.error)};
  if (!reply.value && error_message.empty()) {
    error_message = "Bluetooth is not enabled";
    LOG_TRACE("connectivity BlueZ discovery failed: {}", error_message);
  } else if (reply.value) {
    LOG_TRACE("connectivity BlueZ discovery started on adapter '{}'", adapter_path);
  }
}

void wait_for_bluez_discovery_results() {
  // Give bluetoothd a short window to populate org.bluez.Device1 objects after StartDiscovery.
  g_usleep(1200000);
}

std::vector<WirelessScanItem> bluez_devices_from_managed_objects(GVariant* managed_reply) {
  std::vector<WirelessScanItem> items;
  GVariant* objects_raw = nullptr;
  g_variant_get(managed_reply, "(@a{oa{sa{sv}}})", &objects_raw);
  GVariantOwner objects{objects_raw};

  GVariantIter iter;
  g_variant_iter_init(&iter, objects.value);

  const char* path = nullptr;
  GVariant* ifaces = nullptr;
  while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &path, &ifaces)) {
    GVariantOwner ifaces_owner{ifaces};
    GVariantOwner props{
        g_variant_lookup_value(ifaces_owner.value, "org.bluez.Device1", G_VARIANT_TYPE("a{sv}"))};
    if (!props.value) {
      continue;
    }

    WirelessScanItem item;
    const auto address   = variant_string_property(props.value, "Address");
    bool has_rssi        = false;
    const auto rssi      = variant_int16_property(props.value, "RSSI", has_rssi);
    const bool paired    = variant_bool_property(props.value, "Paired");
    const bool connected = variant_bool_property(props.value, "Connected");

    item.name = variant_string_property(props.value, "Alias");
    if (item.name.empty()) {
      item.name = variant_string_property(props.value, "Name");
    }
    if (item.name.empty()) {
      item.name = address.empty() ? "Bluetooth Device" : address;
    }

    std::ostringstream detail;
    if (!address.empty()) {
      detail << address;
    }
    if (has_rssi) {
      if (!detail.str().empty()) {
        detail << " ";
      }
      detail << rssi << "dBm";
    }
    if (connected || paired) {
      if (!detail.str().empty()) {
        detail << " ";
      }
      detail << (connected ? "connected" : "paired");
    }
    item.detail = detail.str();
    item.strength_percent =
        has_rssi ? std::clamp<int32_t>((static_cast<int32_t>(rssi) + 100) * 2, 0, 100) : -1;
    items.push_back(std::move(item));
  }

  std::sort(items.begin(), items.end(), wireless_result_sort_less);
  return items;
}
#endif


}  // namespace

std::vector<WirelessScanItem> scan_bluetooth(std::string& error_message) {
  static WirelessLogSnapshot log_snapshot;
  error_message.clear();
#if APP_USE_BLUEZ
  LOG_TRACE("connectivity Bluetooth scan requested");
  auto bus = make_system_bus(error_message);
  if (!bus.connection) {
    const std::vector<WirelessScanItem> items;
    const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
    log_wireless_result("bluetooth", items, error_message, changed);
    return {};
  }

  auto managed = bluez_managed_objects(bus.connection, error_message);
  if (!managed.value) {
    const std::vector<WirelessScanItem> items;
    const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
    log_wireless_result("bluetooth", items, error_message, changed);
    return {};
  }

  const auto adapter_path = first_adapter_path(managed.value);
  if (adapter_path.empty()) {
    error_message = "No Bluetooth adapter found";
    const std::vector<WirelessScanItem> items;
    const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
    log_wireless_result("bluetooth", items, error_message, changed);
    return {};
  }

  std::string power_error;
  set_bluez_adapter_powered(bus.connection, adapter_path, true, power_error);
  start_bluez_discovery(bus.connection, adapter_path, error_message);
  wait_for_bluez_discovery_results();

  auto refreshed = bluez_managed_objects(bus.connection, error_message);
  auto items =
      bluez_devices_from_managed_objects(refreshed.value ? refreshed.value : managed.value);
  if (!items.empty()) {
    error_message.clear();
  } else if (!power_error.empty()) {
    error_message = std::move(power_error);
  } else if (error_message.empty()) {
    error_message = "No Bluetooth devices found";
  }
  const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
  log_wireless_result("bluetooth", items, error_message, changed);
  return items;
#else
  LOG_TRACE("connectivity Bluetooth scan requested but BlueZ backend is disabled");
  error_message = "BlueZ backend is not enabled";
  const std::vector<WirelessScanItem> items;
  const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
  log_wireless_result("bluetooth", items, error_message, changed);
  return {};
#endif
}
}  // namespace platform::connectivity
