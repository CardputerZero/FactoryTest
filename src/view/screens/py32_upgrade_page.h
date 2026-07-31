/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "base_screen.h"
#include "dialog.h"

namespace screen {

class Py32UpgradePage : public BaseScreen {
 public:
  Py32UpgradePage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);
  ~Py32UpgradePage() override;

 protected:
  void build_content(lv_obj_t* content) override;

 private:
  struct JobState {
    std::atomic_bool running{false};
    std::atomic_bool done{false};
    std::mutex mutex{};
    int percent{0};
    std::string status{};
    std::string error_status{};
    bool success{false};
  };

  void apply_progress_theme_(bool dark_mode);
  void start_upgrade_();
  void refresh_();
  void restore_nav_actions_();
  void show_reboot_dialog_();
  void close_reboot_dialog_();
  void request_reboot_();
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void theme_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void refresh_timer_cb(lv_timer_t* timer);

  lv_obj_t* upload_icon_{nullptr};
  lv_obj_t* progress_bar_{nullptr};
  lv_obj_t* version_label_{nullptr};
  lv_obj_t* status_label_{nullptr};
  lv_timer_t* refresh_timer_{nullptr};
  std::filesystem::path archive_path_{};
  JobState job_state_{};
  std::thread worker_{};
  std::unique_ptr<view::widgets::Dialog> reboot_dialog_{};
  bool completion_handled_{false};
};

}  // namespace screen
