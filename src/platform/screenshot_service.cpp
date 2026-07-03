/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "screenshot_service.h"

#include <png.h>

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "logger.h"
#include "lvgl.h"
#include "popup.h"

namespace platform::screenshot {
namespace {

constexpr uint32_t K_OVERLAY_HIDE_MS = 2600;

constexpr std::filesystem::perms K_SCREENSHOT_DIR_PERMS =
    std::filesystem::perms::owner_all |
    std::filesystem::perms::group_read |
    std::filesystem::perms::group_exec |
    std::filesystem::perms::others_read |
    std::filesystem::perms::others_exec;

std::filesystem::path pictures_directory() {
  if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
    return std::filesystem::path(home) / "Pictures" / "factory_test";
  }
#if !defined(_WIN32)
  if (const passwd* user = getpwuid(getuid()); user && user->pw_dir && user->pw_dir[0] != '\0') {
    return std::filesystem::path(user->pw_dir) / "Pictures" / "factory_test";
  }
#endif
  return std::filesystem::path("/tmp") / "factory_test";
}

std::string timestamp_name() {
  const auto now    = std::chrono::system_clock::now();
  const auto time_t = std::chrono::system_clock::to_time_t(now);
  const auto ms     = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &time_t);
#else
  localtime_r(&time_t, &local_time);
#endif

  char buffer[40]{};
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d%02d%02d_%02d%02d%02d_%03d.png",
                local_time.tm_year + 1900,
                local_time.tm_mon + 1,
                local_time.tm_mday,
                local_time.tm_hour,
                local_time.tm_min,
                local_time.tm_sec,
                static_cast<int>(ms.count()));
  return buffer;
}

bool ensure_screenshot_directory(const std::filesystem::path& dir, std::string& error_message) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    error_message = "failed to create screenshot directory: " + ec.message();
    return false;
  }

#if !defined(_WIN32)
  std::filesystem::permissions(dir, K_SCREENSHOT_DIR_PERMS, std::filesystem::perm_options::replace, ec);
  if (ec) {
    error_message = "failed to set screenshot directory permissions: " + ec.message();
    return false;
  }
#endif

  return true;
}

bool write_png_rgba(const std::filesystem::path& path,
                    const lv_draw_buf_t* snapshot,
                    std::string& error_message) {
  if (!snapshot || !snapshot->data || snapshot->header.cf != LV_COLOR_FORMAT_ARGB8888) {
    error_message = "invalid snapshot buffer";
    return false;
  }

  FILE* file = std::fopen(path.string().c_str(), "wb");
  if (!file) {
    error_message = "failed to open PNG for writing";
    return false;
  }

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    std::fclose(file);
    error_message = "failed to create PNG writer";
    return false;
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, nullptr);
    std::fclose(file);
    error_message = "failed to create PNG info";
    return false;
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    error_message = "failed to encode PNG";
    return false;
  }

  const auto width  = static_cast<png_uint_32>(snapshot->header.w);
  const auto height = static_cast<png_uint_32>(snapshot->header.h);
  png_init_io(png, file);
  png_set_IHDR(png,
               info,
               width,
               height,
               8,
               PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  std::vector<uint8_t> row(static_cast<std::size_t>(width) * 4U);
  const auto* src = static_cast<const uint8_t*>(snapshot->data);
  for (png_uint_32 y = 0; y < height; ++y) {
    const uint8_t* src_row = src + static_cast<std::size_t>(y) * snapshot->header.stride;
    for (png_uint_32 x = 0; x < width; ++x) {
      const uint8_t* bgra = src_row + static_cast<std::size_t>(x) * 4U;
      auto* rgba          = row.data() + static_cast<std::size_t>(x) * 4U;
      rgba[0]             = bgra[2];
      rgba[1]             = bgra[1];
      rgba[2]             = bgra[0];
      rgba[3]             = bgra[3];
    }
    png_write_row(png, row.data());
  }

  png_write_end(png, nullptr);
  png_destroy_write_struct(&png, &info);
  std::fclose(file);
  return true;
}

bool capture_active_screen(std::string& output_path, std::string& error_message) {
#if LV_USE_SNAPSHOT
  auto* screen = lv_screen_active();
  if (!screen) {
    error_message = "no active screen";
    return false;
  }

  const auto dir = pictures_directory();
  if (!ensure_screenshot_directory(dir, error_message)) {
    return false;
  }

  const auto path = dir / timestamp_name();
  lv_draw_buf_t* snapshot = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
  if (!snapshot) {
    error_message = "lv_snapshot_take failed";
    return false;
  }

  const bool saved = write_png_rgba(path, snapshot, error_message);
  lv_draw_buf_destroy(snapshot);
  if (!saved) {
    return false;
  }

  output_path = path.string();
  return true;
#else
  error_message = "LV_USE_SNAPSHOT is disabled";
  return false;
#endif
}

void show_overlay(const std::string& message, bool success) {
  auto* screen = lv_screen_active();
  if (!screen) {
    return;
  }

  view::widgets::PopupConfig config;
  config.width       = 292;
  config.height      = 56;
  config.label_width = 268;
  config.shadow_width = 14;
  config.shadow_opa  = LV_OPA_30;
  config.long_mode   = LV_LABEL_LONG_SCROLL_CIRCULAR;
  config.font        = &lv_font_montserrat_12;
  config.tone        = success ? view::widgets::PopupTone::SUCCESS : view::widgets::PopupTone::ERROR;
  config.message     = message;
  view::widgets::Popup::show_transient(screen, std::move(config), K_OVERLAY_HIDE_MS);
}

}  // namespace

void capture_active_screen_with_overlay() {
  std::string path;
  std::string error;
  const bool ok = capture_active_screen(path, error);
  if (ok) {
    LOG_INFO("screenshot saved: {}", path);
    show_overlay("Screenshot saved: " + path, true);
  } else {
    LOG_WARN("screenshot failed: {}", error);
    show_overlay("Screenshot failed: " + error, false);
  }
}

}  // namespace platform::screenshot
