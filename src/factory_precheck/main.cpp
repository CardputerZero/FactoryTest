/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <fcntl.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "asset_manager.h"
#include "linux_input.h"
#include "logger.h"
#include "precheck_model.h"
#include "precheck_view.h"
#include "precheck_viewmodel.h"
#include "theme.h"
#include "usb_keyboard_service.h"

#if USE_DESKTOP
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#else
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "src/drivers/display/drm/lv_linux_drm.h"
#endif

#ifndef PRECHECK_DRM_DEVICE
#define PRECHECK_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef PRECHECK_DRM_CONNECTOR_ID
#define PRECHECK_DRM_CONNECTOR_ID (-1)
#endif

#ifndef PRECHECK_ASSET_APP_NAME
#define PRECHECK_ASSET_APP_NAME "factory_test"
#endif

#ifndef PRECHECK_DESKTOP_WIDTH
#define PRECHECK_DESKTOP_WIDTH 1024
#endif

#ifndef PRECHECK_DESKTOP_HEIGHT
#define PRECHECK_DESKTOP_HEIGHT 600
#endif

namespace {

#if USE_DESKTOP
void desktop_key_listener(uint32_t key, const char* key_name, void* user_data) {
  LV_UNUSED(key_name);
  precheck::PrecheckView::key_listener(key, true, user_data);
}
#endif

#if !USE_DESKTOP
struct ConnectorProbe {
  bool inspected{false};
  std::optional<int64_t> connector_id{};
};

bool has_connected_hdmi_connector() {
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm", error)) {
    const auto name = entry.path().filename().string();
    const auto dash = name.find('-');
    if (dash == std::string::npos || name.rfind("card", 0) != 0 ||
        name.find("HDMI", dash + 1) == std::string::npos) {
      continue;
    }
    std::ifstream status(entry.path() / "status");
    std::string value;
    std::getline(status, value);
    if (value == "connected") return true;
  }
  return false;
}

bool is_hdmi_connector(const drmModeConnector* connector) {
  if (!connector) return false;
  return connector->connector_type == DRM_MODE_CONNECTOR_HDMIA ||
         connector->connector_type == DRM_MODE_CONNECTOR_HDMIB;
}

ConnectorProbe find_connected_hdmi_connector(const std::string& device_path) {
  const int fd = open(device_path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    LOG_DEBUG("unable to inspect DRM device {}: {}", device_path, std::strerror(errno));
    return {};
  }

  auto* resources = drmModeGetResources(fd);
  if (!resources) {
    LOG_DEBUG("unable to enumerate DRM connectors on {}", device_path);
    close(fd);
    return {};
  }

  ConnectorProbe probe;
  probe.inspected = true;
  for (int index = 0; index < resources->count_connectors && !probe.connector_id; ++index) {
    auto* connector = drmModeGetConnector(fd, resources->connectors[index]);
    if (!connector) continue;
    if (is_hdmi_connector(connector) && connector->connection == DRM_MODE_CONNECTED &&
        connector->count_modes > 0) {
      probe.connector_id = static_cast<int64_t>(connector->connector_id);
    }
    drmModeFreeConnector(connector);
  }

  drmModeFreeResources(resources);
  close(fd);
  return probe;
}

void add_unique_drm_candidate(std::vector<std::string>& candidates, std::string path) {
  if (path.empty() || std::find(candidates.begin(), candidates.end(), path) != candidates.end()) {
    return;
  }
  candidates.push_back(std::move(path));
}

std::vector<std::string> drm_candidates(const char* configured_path) {
  std::vector<std::string> candidates;
  if (configured_path && configured_path[0] != '\0') {
    add_unique_drm_candidate(candidates, configured_path);
  }

  // Prefer the DRM card that owns a connected HDMI connector.  Card numbers can
  // change when the kernel probes the internal panel and HDMI bridge in a
  // different order, so the service must not depend on card0 being HDMI.
  std::error_code error;
  std::vector<std::string> discovered;
  for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm", error)) {
    const auto name = entry.path().filename().string();
    const auto dash = name.find('-');
    if (dash == std::string::npos || name.rfind("card", 0) != 0 ||
        name.find("HDMI", dash + 1) == std::string::npos) {
      continue;
    }
    std::ifstream status(entry.path() / "status");
    std::string value;
    std::getline(status, value);
    if (value != "connected") continue;
    discovered.push_back("/dev/dri/" + name.substr(0, dash));
  }
  std::sort(discovered.begin(), discovered.end());
  for (auto& path : discovered) {
    add_unique_drm_candidate(candidates, std::move(path));
  }

  return candidates;
}

std::optional<int64_t> configured_connector_id() {
  const char* value = std::getenv("FACTORY_PRECHECK_DRM_CONNECTOR_ID");
  if (!value || value[0] == '\0') {
    return PRECHECK_DRM_CONNECTOR_ID >= 0 ? std::optional<int64_t>(PRECHECK_DRM_CONNECTOR_ID)
                                          : std::nullopt;
  }

  char* end         = nullptr;
  errno             = 0;
  const auto parsed = std::strtoll(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed < 0) {
    LOG_WARN("ignoring invalid FACTORY_PRECHECK_DRM_CONNECTOR_ID='{}'", value);
    return std::nullopt;
  }
  return parsed;
}

int acquire_instance_lock() {
  const char* configured_path = std::getenv("FACTORY_PRECHECK_LOCK_FILE");
  const char* lock_path       = configured_path && configured_path[0] != '\0'
                                    ? configured_path
                                    : "/tmp/factory-precheck.lock";
  const int fd                = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC, 0666);
  if (fd < 0) {
    LOG_ERROR("failed to open precheck instance lock {}: {}", lock_path, std::strerror(errno));
    return -1;
  }
  // The systemd service runs as root while an operator may launch the same
  // binary as the factory user.  Keep the shared lock file writable by both.
  (void)fchmod(fd, 0666);
  if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
    return fd;
  }

  const int lock_error = errno;
  close(fd);
  if (lock_error == EWOULDBLOCK || lock_error == EAGAIN) {
    LOG_WARN("factory precheck is already running (lock {})", lock_path);
  } else {
    LOG_ERROR("failed to acquire precheck instance lock {}: {}",
              lock_path,
              std::strerror(lock_error));
  }
  return -1;
}
#endif

lv_display_t* create_display() {
#if USE_DESKTOP
  auto* display = lv_sdl_window_create(PRECHECK_DESKTOP_WIDTH, PRECHECK_DESKTOP_HEIGHT);
  if (display) {
    lv_sdl_window_set_title(display, "Factory Precheck");
    lv_sdl_window_set_resizeable(display, false);
    auto* mouse    = lv_sdl_mouse_create();
    auto* keyboard = lv_sdl_keyboard_create();
    platform::attach_key_router(keyboard);
    LV_UNUSED(mouse);
  }
  return display;
#else
  const char* device_path = std::getenv("FACTORY_PRECHECK_DRM_DEVICE");
  if (!device_path || device_path[0] == '\0') {
    device_path = PRECHECK_DRM_DEVICE;
  }

  const auto connector_override = configured_connector_id();
  for (const auto& candidate : drm_candidates(device_path)) {
    if (access(candidate.c_str(), R_OK | W_OK) != 0) {
      LOG_DEBUG("DRM display device is not accessible: {}", candidate);
      continue;
    }

    const auto probe = find_connected_hdmi_connector(candidate);
    if (!connector_override && probe.inspected && !probe.connector_id) {
      LOG_DEBUG("DRM device {} has no connected HDMI connector; skipping", candidate);
      continue;
    }
    const auto connector_id = connector_override.value_or(probe.connector_id.value_or(-1));
    LOG_INFO("trying precheck DRM display device: {} connector={}", candidate, connector_id);
    auto* display = lv_linux_drm_create();
    if (!display) {
      LOG_ERROR("failed to create DRM display object for {}", candidate);
      continue;
    }
    if (lv_linux_drm_set_file(display, candidate.c_str(), connector_id) == LV_RESULT_OK) {
      LOG_INFO("using precheck DRM display device: {} connector={} resolution={}x{}",
               candidate,
               connector_id,
               lv_display_get_horizontal_resolution(display),
               lv_display_get_vertical_resolution(display));
      return display;
    }
    lv_display_delete(display);
    LOG_WARN("precheck DRM display device failed: {} connector={}", candidate, connector_id);
  }
  return nullptr;
#endif
}

}  // namespace

int main() {
  logger::Logger::init();
  logger::Logger::set_tag("factory-precheck");
#if !USE_DESKTOP
  const int instance_lock_fd = acquire_instance_lock();
  if (instance_lock_fd < 0) {
    logger::Logger::shutdown();
    return 2;
  }
#endif
  lv_init();
  auto* display = create_display();
  if (!display) {
    LOG_ERROR("failed to initialize precheck display");
#if !USE_DESKTOP
    close(instance_lock_fd);
#endif
    logger::Logger::shutdown();
    return 1;
  }

  app::AssetManager assets(PRECHECK_ASSET_APP_NAME);
  precheck::PrecheckModel model;
  precheck::PrecheckViewModel view_model(model);
  view::apply_lvgl_theme(display, true);
  {
    // Destroy LVGL-owned view objects before deleting their display.
    precheck::PrecheckView view(view_model, assets);
    view.build(display);
    // Submit the initial frame immediately.  This makes a successful service
    // start observable even when no input or timer event has arrived yet.
    lv_refr_now(display);
    precheck::UsbKeyboardService keyboard;
    keyboard.start(display, precheck::PrecheckView::key_listener, &view);
#if USE_DESKTOP
    platform::set_key_listener(desktop_key_listener, &view);
#endif
    while (view.running() && lv_display_get_default()) {
#if !USE_DESKTOP
      if (!has_connected_hdmi_connector()) {
        LOG_INFO("HDMI disconnected; stopping precheck display");
        break;
      }
#endif
      lv_timer_handler();
      lv_delay_ms(5);
    }
    keyboard.stop();
#if USE_DESKTOP
    platform::clear_key_listener(desktop_key_listener, &view);
#endif
  }
  if (auto* active_display = lv_display_get_default()) {
    lv_display_delete(active_display);
  }
#if !USE_DESKTOP
  close(instance_lock_fd);
#endif
  logger::Logger::shutdown();
  return 0;
}
