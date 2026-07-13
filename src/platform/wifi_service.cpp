/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_service.h"

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

#if APP_USE_LIBNM
#include <NetworkManager.h>
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

#if APP_USE_LIBNM
constexpr gint64 K_WIFI_SCAN_WAIT_TIMEOUT_US  = 12000000;
constexpr gint64 K_WIFI_SCAN_POLL_INTERVAL_US = 100000;
constexpr gint64 K_WIFI_SCAN_SETTLE_TIME_US   = 500000;
constexpr gint64 K_WIFI_SCAN_SETTLE_POLL_US   = 25000;
constexpr gint64 K_NM_PROPERTY_SET_TIMEOUT_US = 2000000;

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

GMainContext* active_glib_context();
void drain_glib_context();
void settle_glib_context();

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

struct NMAsyncState {
  bool completed{false};
  bool success{false};
  std::string error_message{};
};

#if NM_CHECK_VERSION(1, 24, 0)
void nm_dbus_set_property_cb(GObject* source, GAsyncResult* result, gpointer user_data) {
  auto* state  = static_cast<NMAsyncState*>(user_data);
  auto* client = NM_CLIENT(source);
  if (!state || !client) {
    return;
  }

  GErrorOwner error;
  state->success = nm_client_dbus_set_property_finish(client, result, &error.error);
  if (!state->success) {
    state->error_message = take_error(error, "NetworkManager property update failed");
  }
  state->completed = true;
}
#endif

bool wait_for_nm_async(NMAsyncState& state, gint64 timeout_us) {
  const gint64 deadline = g_get_monotonic_time() + timeout_us;
  auto* context         = active_glib_context();
  while (!state.completed && g_get_monotonic_time() < deadline) {
    if (!g_main_context_iteration(context, FALSE)) {
      g_usleep(K_WIFI_SCAN_SETTLE_POLL_US);
    }
  }
  drain_glib_context();
  return state.completed;
}

bool set_wifi_enabled(NMClient* client, bool enabled, std::string& error_message) {
  if (!client) {
    return false;
  }

#if NM_CHECK_VERSION(1, 24, 0)
  NMAsyncState state;
  nm_client_dbus_set_property(client,
                              NM_DBUS_PATH,
                              NM_DBUS_INTERFACE,
                              "WirelessEnabled",
                              g_variant_new_boolean(enabled ? TRUE : FALSE),
                              static_cast<int>(K_NM_PROPERTY_SET_TIMEOUT_US / 1000),
                              nullptr,
                              nm_dbus_set_property_cb,
                              &state);
  if (!wait_for_nm_async(state, K_NM_PROPERTY_SET_TIMEOUT_US)) {
    error_message = "Wi-Fi enable request timed out";
    return false;
  }
  if (!state.success) {
    error_message = state.error_message.empty() ? "Wi-Fi enable request failed"
                                                : std::move(state.error_message);
    return false;
  }
  return true;
#else
  nm_client_wireless_set_enabled(client, enabled ? TRUE : FALSE);
  return true;
#endif
}

bool ensure_wifi_enabled(NMClient* client, std::string& error_message) {
  if (!client) {
    return false;
  }

  if (!nm_client_wireless_hardware_get_enabled(client)) {
    error_message = "Wi-Fi hardware switch is off";
    return false;
  }

  if (nm_client_wireless_get_enabled(client)) {
    return true;
  }

  LOG_INFO("connectivity Wi-Fi is disabled; requesting NetworkManager to enable it");
  if (!set_wifi_enabled(client, true, error_message)) {
    return false;
  }
  settle_glib_context();
  if (nm_client_wireless_get_enabled(client)) {
    return true;
  }

  error_message = "Wi-Fi enable request was denied or did not complete";
  return false;
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
  if (!ensure_wifi_enabled(client, error_message)) {
    return items;
  }

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
}  // namespace platform::connectivity
