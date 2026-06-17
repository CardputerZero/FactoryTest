/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_service.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <utility>

#include "logger.h"

#if APP_USE_LIBNM
#include <NetworkManager.h>
#endif
#if APP_USE_BLUEZ
#include <gio/gio.h>
#endif
#if APP_USE_LIBUDEV
#include <libudev.h>
#endif
#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
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

bool same_ethernet_info(const EthernetInfo& left, const EthernetInfo& right) {
  return left.interface_name == right.interface_name && left.state == right.state &&
         left.hw_address == right.hw_address && left.ip4_address == right.ip4_address &&
         left.connection_name == right.connection_name && left.connected == right.connected;
}

bool same_usb_device(const UsbDeviceInfo& left, const UsbDeviceInfo& right) {
  return left.name == right.name && left.detail == right.detail && left.sys_path == right.sys_path;
}

bool same_usb_result(const std::vector<UsbDeviceInfo>& left,
                     const std::vector<UsbDeviceInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_usb_device);
}

bool same_i2c_address(const I2cAddressInfo& left, const I2cAddressInfo& right) {
  return left.address == right.address && left.state == right.state;
}

bool same_i2c_result(const std::vector<I2cAddressInfo>& left,
                     const std::vector<I2cAddressInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_i2c_address);
}

bool same_spi_device(const SpiDeviceInfo& left, const SpiDeviceInfo& right) {
  return left.name == right.name && left.path == right.path;
}

bool same_spi_result(const std::vector<SpiDeviceInfo>& left,
                     const std::vector<SpiDeviceInfo>& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), same_spi_device);
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

struct EthernetLogSnapshot {
  bool initialized{false};
  EthernetInfo info{};
  std::string error_message{};
};

struct UsbLogSnapshot {
  bool initialized{false};
  std::vector<UsbDeviceInfo> devices{};
  std::string error_message{};
};

struct I2cLogSnapshot {
  bool initialized{false};
  std::vector<I2cAddressInfo> addresses{};
  std::string error_message{};
};

struct SpiLogSnapshot {
  bool initialized{false};
  std::vector<SpiDeviceInfo> devices{};
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

bool update_ethernet_log_snapshot(EthernetLogSnapshot& snapshot,
                                  const EthernetInfo& info,
                                  const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_ethernet_info(snapshot.info, info)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.info          = info;
  snapshot.error_message = error_message;
  return true;
}

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

bool update_i2c_log_snapshot(I2cLogSnapshot& snapshot,
                             const std::vector<I2cAddressInfo>& addresses,
                             const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_i2c_result(snapshot.addresses, addresses)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.addresses     = addresses;
  snapshot.error_message = error_message;
  return true;
}

bool update_spi_log_snapshot(SpiLogSnapshot& snapshot,
                             const std::vector<SpiDeviceInfo>& devices,
                             const std::string& error_message) {
  if (snapshot.initialized && snapshot.error_message == error_message &&
      same_spi_result(snapshot.devices, devices)) {
    return false;
  }

  snapshot.initialized   = true;
  snapshot.devices       = devices;
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

void log_ethernet_result(const EthernetInfo& info, const std::string& error_message, bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity ethernet result unchanged");
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity ethernet result message: {}", error_message);
  }

  LOG_INFO(
      "connectivity ethernet result: interface='{}' state='{}' connected={} connection='{}' "
      "ipv4='{}' mac='{}'",
      info.interface_name,
      info.state,
      info.connected,
      info.connection_name,
      info.ip4_address,
      info.hw_address);
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

void log_i2c_result(int bus_number,
                    const std::vector<I2cAddressInfo>& addresses,
                    const std::string& error_message,
                    bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity i2c{} result unchanged: {} address(es)", bus_number, addresses.size());
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity i2c{} result message: {}", bus_number, error_message);
  }

  std::size_t present_count = 0;
  std::size_t busy_count    = 0;
  for (const auto& address : addresses) {
    if (address.state == I2cAddressState::PRESENT) {
      ++present_count;
    } else if (address.state == I2cAddressState::KERNEL_DRIVER) {
      ++busy_count;
    }
  }
  LOG_INFO("connectivity i2c{} result: {} present, {} kernel-owned",
           bus_number,
           present_count,
           busy_count);
}

void log_spi_result(const std::vector<SpiDeviceInfo>& devices,
                    const std::string& error_message,
                    bool changed) {
  if (!changed) {
    LOG_TRACE("connectivity spi result unchanged: {} device(s)", devices.size());
    return;
  }

  if (!error_message.empty()) {
    LOG_WARN("connectivity spi result message: {}", error_message);
  }

  LOG_INFO("connectivity spi result: {} device(s)", devices.size());
  for (const auto& device : devices) {
    LOG_INFO("connectivity spi device: name='{}' path='{}'", device.name, device.path);
  }
}

#if APP_USE_LIBNM || APP_USE_BLUEZ
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

#if APP_USE_BLUEZ
bool is_gio_timeout(const GErrorOwner& owner) {
  return owner.error && g_error_matches(owner.error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
}
#endif
#endif

#if APP_USE_LIBNM
constexpr gint64 K_WIFI_SCAN_WAIT_TIMEOUT_US  = 12000000;
constexpr gint64 K_WIFI_SCAN_POLL_INTERVAL_US = 100000;
constexpr gint64 K_WIFI_SCAN_SETTLE_TIME_US   = 500000;
constexpr gint64 K_WIFI_SCAN_SETTLE_POLL_US   = 25000;

struct GMainContextScope {
  GMainContext* context{nullptr};

  GMainContextScope()
      : context(g_main_context_new()) {
    g_main_context_push_thread_default(context);
  }

  ~GMainContextScope() {
    if (context) {
      g_main_context_pop_thread_default(context);
      g_main_context_unref(context);
    }
  }
};

struct NMClientOwner {
  NMClient* client{nullptr};

  ~NMClientOwner() {
    if (client) {
      g_object_unref(client);
    }
  }
};

NMClientOwner make_client(std::string& error_message) {
  LOG_TRACE("connectivity initializing NetworkManager/libnm client");
  GErrorOwner error;
  NMClientOwner owner{nm_client_new(nullptr, &error.error)};
  if (!owner.client) {
    error_message = take_error(error, "NetworkManager is not available");
    LOG_TRACE("connectivity NetworkManager/libnm initialization failed: {}", error_message);
  } else {
    LOG_TRACE("connectivity NetworkManager/libnm client initialized");
  }
  return owner;
}

std::string object_string_property(GObject* object, const char* property_name) {
  if (!object || !property_name ||
      !g_object_class_find_property(G_OBJECT_GET_CLASS(object), property_name)) {
    return "";
  }

  char* value = nullptr;
  g_object_get(object, property_name, &value, nullptr);
  std::string result = value ? value : "";
  g_free(value);
  return result;
}

std::string device_state_text(NMDevice* device) {
  if (!device) {
    return "unknown";
  }

  switch (nm_device_get_state(device)) {
    case NM_DEVICE_STATE_UNKNOWN:
      return "unknown";
    case NM_DEVICE_STATE_UNMANAGED:
      return "unmanaged";
    case NM_DEVICE_STATE_UNAVAILABLE:
      return "unavailable";
    case NM_DEVICE_STATE_DISCONNECTED:
      return "disconnected";
    case NM_DEVICE_STATE_PREPARE:
      return "prepare";
    case NM_DEVICE_STATE_CONFIG:
      return "config";
    case NM_DEVICE_STATE_NEED_AUTH:
      return "need-auth";
    case NM_DEVICE_STATE_IP_CONFIG:
      return "ip-config";
    case NM_DEVICE_STATE_IP_CHECK:
      return "ip-check";
    case NM_DEVICE_STATE_SECONDARIES:
      return "secondaries";
    case NM_DEVICE_STATE_ACTIVATED:
      return "activated";
    case NM_DEVICE_STATE_DEACTIVATING:
      return "deactivating";
    case NM_DEVICE_STATE_FAILED:
      return "failed";
    default:
      return "unknown";
  }
}

std::string active_connection_name(NMDevice* device) {
  auto* connection = device ? nm_device_get_active_connection(device) : nullptr;
  const char* id   = connection ? nm_active_connection_get_id(connection) : nullptr;
  return id ? id : "";
}

std::string first_ip4_address(NMDevice* device) {
  auto* config = device ? nm_device_get_ip4_config(device) : nullptr;
  if (!config) {
    return "";
  }

  const auto* addresses = nm_ip_config_get_addresses(config);
  if (!addresses || addresses->len == 0) {
    return "";
  }

  auto* address    = static_cast<NMIPAddress*>(g_ptr_array_index(addresses, 0));
  const char* text = address ? nm_ip_address_get_address(address) : nullptr;
  if (!text) {
    return "";
  }

  std::ostringstream out;
  out << text << "/" << static_cast<int>(nm_ip_address_get_prefix(address));
  return out.str();
}

std::string ssid_to_text(GBytes* ssid) {
  if (!ssid) {
    return "<hidden>";
  }

  gsize size       = 0;
  const auto* raw  = static_cast<const guint8*>(g_bytes_get_data(ssid, &size));
  char* utf8       = nm_utils_ssid_to_utf8(raw, size);
  std::string text = utf8 && utf8[0] != '\0' ? utf8 : "<hidden>";
  g_free(utf8);
  return text;
}

GMainContext* active_glib_context() {
  auto* context = g_main_context_get_thread_default();
  return context ? context : g_main_context_default();
}

void drain_glib_context() {
  auto* context = active_glib_context();
  while (g_main_context_pending(context)) {
    g_main_context_iteration(context, FALSE);
  }
}

void settle_glib_context() {
  const gint64 deadline = g_get_monotonic_time() + K_WIFI_SCAN_SETTLE_TIME_US;
  auto* context         = active_glib_context();
  while (g_get_monotonic_time() < deadline) {
    if (!g_main_context_iteration(context, FALSE)) {
      g_usleep(K_WIFI_SCAN_SETTLE_POLL_US);
    }
  }
  drain_glib_context();
}

struct WifiScanRequestState {
  NMDeviceWifi* device{nullptr};
  gint64 previous_last_scan{-1};
  bool completed{false};
  bool success{false};
  bool scan_completed{false};
  std::string error_message{};
};

void update_wifi_scan_completed(WifiScanRequestState& state) {
  if (!state.device) {
    return;
  }

  const gint64 last_scan = nm_device_wifi_get_last_scan(state.device);
  if (last_scan > 0 && last_scan != state.previous_last_scan) {
    state.scan_completed = true;
  }
}

void wifi_last_scan_notify_cb(GObject* source, GParamSpec* pspec, gpointer user_data) {
  (void)pspec;

  auto* state  = static_cast<WifiScanRequestState*>(user_data);
  auto* device = NM_DEVICE_WIFI(source);
  if (!state || !device) {
    return;
  }

  state->device = device;
  update_wifi_scan_completed(*state);
}

void wifi_scan_request_cb(GObject* source, GAsyncResult* result, gpointer user_data) {
  auto* state  = static_cast<WifiScanRequestState*>(user_data);
  auto* device = NM_DEVICE_WIFI(source);
  if (!state || !device) {
    return;
  }

  GErrorOwner error;
  state->success = nm_device_wifi_request_scan_finish(device, result, &error.error);
  if (!state->success) {
    state->error_message = take_error(error, "Failed to request Wi-Fi scan");
  }
  state->completed = true;
  update_wifi_scan_completed(*state);
}

bool wait_for_wifi_scan(WifiScanRequestState& state) {
  const gint64 deadline = g_get_monotonic_time() + K_WIFI_SCAN_WAIT_TIMEOUT_US;
  auto* context         = active_glib_context();
  while (g_get_monotonic_time() < deadline) {
    update_wifi_scan_completed(state);
    if (state.completed && !state.success) {
      return false;
    }
    if (state.scan_completed) {
      return true;
    }
    if (!g_main_context_iteration(context, FALSE)) {
      g_usleep(K_WIFI_SCAN_POLL_INTERVAL_US);
    }
  }

  drain_glib_context();
  update_wifi_scan_completed(state);
  return state.scan_completed;
}

bool request_wifi_scan(NMDeviceWifi* device, std::string& scan_error_message) {
  if (!device) {
    return false;
  }

  const gint64 previous_last_scan = nm_device_wifi_get_last_scan(device);
  WifiScanRequestState state;
  state.device             = device;
  state.previous_last_scan = previous_last_scan;
  const gulong notify_id   = g_signal_connect(device,
                                              "notify::" NM_DEVICE_WIFI_LAST_SCAN,
                                              G_CALLBACK(wifi_last_scan_notify_cb),
                                              &state);
  nm_device_wifi_request_scan_async(device, nullptr, wifi_scan_request_cb, &state);
  const bool scan_completed = wait_for_wifi_scan(state);
  g_signal_handler_disconnect(device, notify_id);

  if (state.completed && !state.success) {
    scan_error_message = std::move(state.error_message);
    LOG_WARN("connectivity Wi-Fi scan request failed: {}", scan_error_message);
    return false;
  }

  if (!scan_completed) {
    scan_error_message = "Wi-Fi scan timed out";
    LOG_WARN("connectivity Wi-Fi scan result did not arrive before timeout");
    return false;
  }

  settle_glib_context();
  return true;
}

WirelessScanItem wifi_access_point_item(NMAccessPoint* ap) {
  WirelessScanItem item;
  item.name             = ssid_to_text(nm_access_point_get_ssid(ap));
  item.strength_percent = nm_access_point_get_strength(ap);

  std::ostringstream detail;
  const char* bssid = nm_access_point_get_bssid(ap);
  if (bssid && bssid[0] != '\0') {
    detail << bssid << " ";
  }
  detail << nm_access_point_get_frequency(ap) << "MHz";
  item.detail = detail.str();
  return item;
}

std::vector<WirelessScanItem> read_wifi_access_points(NMClient* client,
                                                      std::string& error_message) {
  std::vector<WirelessScanItem> items;
  const auto* devices = nm_client_get_devices(client);
  if (!devices) {
    error_message = "NetworkManager did not report any devices";
    return items;
  }

  std::vector<NMDeviceWifi*> wifi_devices;
  for (guint i = 0; i < devices->len; ++i) {
    auto* device = static_cast<NMDevice*>(g_ptr_array_index(devices, i));
    if (!device || nm_device_get_device_type(device) != NM_DEVICE_TYPE_WIFI) {
      continue;
    }

    wifi_devices.push_back(NM_DEVICE_WIFI(device));
  }

  std::string scan_error_message;
  for (auto* device : wifi_devices) {
    std::string device_scan_error;
    if (!request_wifi_scan(device, device_scan_error) && scan_error_message.empty()) {
      scan_error_message = std::move(device_scan_error);
    }
  }

  for (auto* device : wifi_devices) {
    const auto* aps = nm_device_wifi_get_access_points(device);
    if (!aps) {
      continue;
    }

    for (guint ap_index = 0; ap_index < aps->len; ++ap_index) {
      auto* ap = static_cast<NMAccessPoint*>(g_ptr_array_index(aps, ap_index));
      if (!ap) {
        continue;
      }

      items.push_back(wifi_access_point_item(ap));
    }
  }

  std::sort(items.begin(), items.end(), wireless_result_sort_less);

  if (items.empty() && error_message.empty()) {
    if (wifi_devices.empty()) {
      error_message = "No Wi-Fi device found";
    } else if (!scan_error_message.empty()) {
      error_message = std::move(scan_error_message);
    } else {
      error_message = "No Wi-Fi access points found";
    }
  }
  return items;
}

#endif

#if APP_USE_BLUEZ
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

#if defined(__linux__)
struct FileDescriptorOwner {
  int fd{-1};

  ~FileDescriptorOwner() {
    if (fd >= 0) {
      close(fd);
    }
  }
};

std::string i2c_bus_device_path(int bus_number) {
  char buffer[32]{};
  std::snprintf(buffer, sizeof(buffer), "/dev/i2c-%d", bus_number);
  return buffer;
}

int i2c_smbus_access(int fd,
                     char read_write,
                     uint8_t command,
                     int size,
                     union i2c_smbus_data* data) {
  i2c_smbus_ioctl_data args{};
  args.read_write = read_write;
  args.command    = command;
  args.size       = size;
  args.data       = data;
  return ioctl(fd, I2C_SMBUS, &args);
}

bool i2c_smbus_read_byte(int fd) {
  union i2c_smbus_data data{};
  return i2c_smbus_access(fd, static_cast<char>(I2C_SMBUS_READ), 0, I2C_SMBUS_BYTE, &data) >= 0;
}

I2cAddressState probe_i2c_address(int fd, uint8_t address) {
  if (ioctl(fd, I2C_SLAVE, address) < 0) {
    return errno == EBUSY ? I2cAddressState::KERNEL_DRIVER : I2cAddressState::ABSENT;
  }
  return i2c_smbus_read_byte(fd) ? I2cAddressState::PRESENT : I2cAddressState::ABSENT;
}
#endif

bool is_spidev_name(const std::string& name) {
  constexpr const char* PREFIX = "spidev";
  return name.rfind(PREFIX, 0) == 0 && name.size() > std::strlen(PREFIX);
}

}  // namespace

std::vector<WirelessScanItem> scan_wifi(std::string& error_message) {
  static WirelessLogSnapshot log_snapshot;
  error_message.clear();
#if APP_USE_LIBNM
  LOG_TRACE("connectivity Wi-Fi scan requested");
  GMainContextScope context_scope;
  auto client = make_client(error_message);
  if (!client.client) {
    const std::vector<WirelessScanItem> items;
    const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
    log_wireless_result("wifi", items, error_message, changed);
    return {};
  }
  auto items         = read_wifi_access_points(client.client, error_message);
  const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
  log_wireless_result("wifi", items, error_message, changed);
  return items;
#else
  LOG_TRACE("connectivity Wi-Fi scan requested but libnm backend is disabled");
  error_message = "libnm backend is not enabled";
  const std::vector<WirelessScanItem> items;
  const bool changed = update_wireless_log_snapshot(log_snapshot, items, error_message);
  log_wireless_result("wifi", items, error_message, changed);
  return {};
#endif
}

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

  start_bluez_discovery(bus.connection, adapter_path, error_message);

  auto items = bluez_devices_from_managed_objects(managed.value);
  if (!items.empty()) {
    error_message.clear();
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

EthernetInfo read_ethernet_info(std::string& error_message) {
  static EthernetLogSnapshot log_snapshot;
  error_message.clear();
  EthernetInfo info;
#if APP_USE_LIBNM
  LOG_TRACE("connectivity Ethernet info requested");
  auto client = make_client(error_message);
  if (!client.client) {
    const bool changed = update_ethernet_log_snapshot(log_snapshot, info, error_message);
    log_ethernet_result(info, error_message, changed);
    return info;
  }

  const auto* devices = nm_client_get_devices(client.client);
  if (!devices) {
    error_message      = "No NetworkManager devices";
    const bool changed = update_ethernet_log_snapshot(log_snapshot, info, error_message);
    log_ethernet_result(info, error_message, changed);
    return info;
  }

  for (guint i = 0; i < devices->len; ++i) {
    auto* device = static_cast<NMDevice*>(g_ptr_array_index(devices, i));
    if (!device || nm_device_get_device_type(device) != NM_DEVICE_TYPE_ETHERNET) {
      continue;
    }

    const char* iface    = nm_device_get_iface(device);
    info.interface_name  = iface ? iface : "";
    info.state           = device_state_text(device);
    info.hw_address      = object_string_property(G_OBJECT(device), "hw-address");
    info.ip4_address     = first_ip4_address(device);
    info.connection_name = active_connection_name(device);
    info.connected       = nm_device_get_state(device) == NM_DEVICE_STATE_ACTIVATED;
    const bool changed   = update_ethernet_log_snapshot(log_snapshot, info, error_message);
    log_ethernet_result(info, error_message, changed);
    return info;
  }

  error_message = "No Ethernet device found";
#else
  LOG_TRACE("connectivity Ethernet info requested but libnm backend is disabled");
  error_message = "libnm backend is not enabled";
#endif
  const bool changed = update_ethernet_log_snapshot(log_snapshot, info, error_message);
  log_ethernet_result(info, error_message, changed);
  return info;
}

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

std::vector<I2cAddressInfo> scan_i2c_bus(int bus_number, std::string& error_message) {
  static I2cLogSnapshot log_snapshot;
  error_message.clear();

#if defined(__linux__)
  LOG_TRACE("connectivity I2C bus {} scan requested", bus_number);
  const auto path = i2c_bus_device_path(bus_number);
  FileDescriptorOwner bus{open(path.c_str(), O_RDWR)};
  if (bus.fd < 0) {
    error_message = std::string("failed to open ") + path + ": " + std::strerror(errno);
    const std::vector<I2cAddressInfo> addresses;
    const bool changed = update_i2c_log_snapshot(log_snapshot, addresses, error_message);
    log_i2c_result(bus_number, addresses, error_message, changed);
    return {};
  }

  std::vector<I2cAddressInfo> addresses;
  addresses.reserve(0x78 - 0x03);
  for (uint8_t address = 0x03; address <= 0x77; ++address) {
    addresses.push_back({address, probe_i2c_address(bus.fd, address)});
  }

  const bool changed = update_i2c_log_snapshot(log_snapshot, addresses, error_message);
  log_i2c_result(bus_number, addresses, error_message, changed);
  return addresses;
#else
  LOG_TRACE("connectivity I2C bus {} scan requested but Linux I2C backend is unavailable",
            bus_number);
  error_message = "Linux I2C backend is not available";
  const std::vector<I2cAddressInfo> addresses;
  const bool changed = update_i2c_log_snapshot(log_snapshot, addresses, error_message);
  log_i2c_result(bus_number, addresses, error_message, changed);
  return {};
#endif
}

std::vector<SpiDeviceInfo> list_spi_devices(std::string& error_message) {
  static SpiLogSnapshot log_snapshot;
  error_message.clear();

#if defined(__linux__)
  LOG_TRACE("connectivity SPI device enumeration requested");
  std::vector<SpiDeviceInfo> devices;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator("/dev", ec)) {
    const auto name = entry.path().filename().string();
    if (!is_spidev_name(name)) {
      continue;
    }

    devices.push_back({name, entry.path().string()});
  }

  if (ec) {
    error_message = std::string("failed to enumerate /dev: ") + ec.message();
  }
  std::sort(devices.begin(), devices.end(), [](const auto& left, const auto& right) {
    return left.name < right.name;
  });
  if (devices.empty() && error_message.empty()) {
    error_message = "No SPI devices found";
  }

  const bool changed = update_spi_log_snapshot(log_snapshot, devices, error_message);
  log_spi_result(devices, error_message, changed);
  return devices;
#else
  LOG_TRACE("connectivity SPI device enumeration requested but Linux backend is unavailable");
  error_message = "Linux SPI backend is not available";
  const std::vector<SpiDeviceInfo> devices;
  const bool changed = update_spi_log_snapshot(log_snapshot, devices, error_message);
  log_spi_result(devices, error_message, changed);
  return {};
#endif
}

}  // namespace platform::connectivity
