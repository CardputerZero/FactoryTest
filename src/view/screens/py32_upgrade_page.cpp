/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "py32_upgrade_page.h"

#include <algorithm>
#include <utility>

#include "asset_manager.h"
#include "bindings.h"
#include "device_info_service.h"
#include "linux_input.h"
#include "logger.h"
#include "power_service.h"
#include "py32_upgrade_service.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_CONTENT_WIDTH        = 280;
constexpr int32_t K_PROGRESS_WIDTH       = 220;
constexpr int32_t K_PROGRESS_HEIGHT      = 8;
constexpr int32_t K_UPLOAD_ICON_SIZE     = 38;
constexpr int32_t K_STATUS_FONT_SIZE     = 12;
constexpr uint32_t K_REFRESH_INTERVAL_MS = 100;

bool is_enter_key(uint32_t key) { return key == LV_KEY_ENTER || key == '\n' || key == '\r'; }

}  // namespace

Py32UpgradePage::Py32UpgradePage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  archive_path_ = assets.resolve("py32_firmware/cardputerzero_ioe1_upgrade-main.tar.gz");
  set_default_test_nav_(false);
  set_nav_action_('8', view::ICON_UPLOAD, [this]() { start_upgrade_(); });
  init();
  platform::set_key_listener(key_listener, this);
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_INTERVAL_MS, this);
}

Py32UpgradePage::~Py32UpgradePage() {
  platform::clear_key_listener(key_listener, this);
  if (refresh_timer_) {
    lv_timer_delete(refresh_timer_);
    refresh_timer_ = nullptr;
  }
  if (worker_.joinable()) {
    worker_.join();
  }
  reboot_dialog_.reset();
  platform::set_modal_key_capture(false);
}

void Py32UpgradePage::build_content(lv_obj_t* content) {
  auto* layout = lv_obj_create(content);
  lv_obj_remove_style_all(layout);
  lv_obj_set_size(layout, K_CONTENT_WIDTH, LV_PCT(100));
  lv_obj_set_flex_flow(layout, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(layout, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(layout, 9, 0);
  lv_obj_clear_flag(layout, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(layout);

  upload_icon_ = lv_label_create(layout);
  lv_label_set_text(upload_icon_, view::ICON_UPLOAD);
  auto* icon_font = assets_ref_().load_font("Phosphor-Fill.ttf", K_UPLOAD_ICON_SIZE);
  lv_obj_set_style_text_font(upload_icon_, icon_font ? icon_font : &lv_font_montserrat_18, 0);
  reactive::bind_theme(upload_icon_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  progress_bar_ = lv_bar_create(layout);
  lv_obj_set_size(progress_bar_, K_PROGRESS_WIDTH, K_PROGRESS_HEIGHT);
  lv_bar_set_range(progress_bar_, 0, 100);
  lv_bar_set_value(progress_bar_, 0, LV_ANIM_OFF);
  lv_obj_set_style_radius(progress_bar_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(progress_bar_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(progress_bar_, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(progress_bar_, LV_OPA_COVER, LV_PART_INDICATOR);
  apply_progress_theme_(app_view_model_ref_().is_dark_mode());
  reactive::observe_obj(progress_bar_,
                        app_view_model_ref_().dark_mode_subject(),
                        theme_observer,
                        this);

  status_label_        = lv_label_create(layout);
  auto current_version = platform::device_info::read_py32_firmware_version(false);
  if (current_version.empty() || current_version == "--") {
    current_version = "0x--";
  }
  const auto current_status = app_view_model_ref_().tr("Current version") + " " + current_version;
  lv_label_set_text(status_label_, current_status.c_str());
  lv_obj_set_width(status_label_, K_CONTENT_WIDTH);
  lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  auto* status_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"),
                              K_STATUS_FONT_SIZE);
  lv_obj_set_style_text_font(status_label_, status_font ? status_font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(status_label_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
}

void Py32UpgradePage::apply_progress_theme_(bool dark_mode) {
  if (!progress_bar_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  const auto track  = lv_color_mix(colors.primary, colors.surface, 36);
  lv_obj_set_style_bg_color(progress_bar_, track, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress_bar_, colors.primary, LV_PART_INDICATOR);
}

void Py32UpgradePage::start_upgrade_() {
  if (job_state_.running.load() || !progress_bar_ || !status_label_) {
    return;
  }

  if (worker_.joinable()) {
    worker_.join();
  }

  {
    std::lock_guard<std::mutex> lock(job_state_.mutex);
    job_state_.percent = 0;
    job_state_.status  = "Preparing upgrade...";
    job_state_.error_status.clear();
    job_state_.success = false;
  }
  job_state_.done.store(false);
  job_state_.running.store(true);
  completion_handled_ = false;
  app_view_model_ref_().clear_nav_actions();

  const auto archive_path = archive_path_;
  worker_                 = std::thread([this, archive_path]() {
    auto result =
        platform::py32_upgrade::run(archive_path,
                                    [this](const platform::py32_upgrade::Progress& progress) {
                                      std::lock_guard<std::mutex> lock(job_state_.mutex);
                                      job_state_.percent = progress.percent;
                                      job_state_.status  = progress.status;
                                    });
    {
      std::lock_guard<std::mutex> lock(job_state_.mutex);
      job_state_.success      = result.success;
      job_state_.error_status = std::move(result.error_status);
      if (result.success) {
        job_state_.percent = 100;
        job_state_.status  = "Upgrade complete (0xF8). Please reboot.";
      }
    }
    job_state_.running.store(false);
    job_state_.done.store(true);
  });
}

void Py32UpgradePage::refresh_() {
  if (!progress_bar_ || !status_label_) {
    return;
  }

  int percent  = 0;
  bool success = false;
  std::string status;
  std::string error_status;
  {
    std::lock_guard<std::mutex> lock(job_state_.mutex);
    percent       = job_state_.percent;
    success       = job_state_.success;
    status        = job_state_.status;
    error_status  = job_state_.error_status;
  }

  if (!status.empty()) {
    lv_bar_set_value(progress_bar_, std::clamp(percent, 0, 100), LV_ANIM_ON);
    if (job_state_.done.load() && !success) {
      const auto translated = app_view_model_ref_().tr(
          error_status.empty() ? "Upgrade failed" : error_status.c_str());
      lv_label_set_text(status_label_, translated.c_str());
    } else {
      const auto translated = app_view_model_ref_().tr(status.c_str());
      lv_label_set_text(status_label_, translated.c_str());
    }
  }

  if (job_state_.done.load() && !completion_handled_) {
    completion_handled_ = true;
    restore_nav_actions_();
    if (success) {
      show_reboot_dialog_();
    }
  }
}

void Py32UpgradePage::restore_nav_actions_() {
  set_default_test_nav_(false);
  set_nav_action_('8', view::ICON_UPLOAD, [this]() { start_upgrade_(); });
}

void Py32UpgradePage::show_reboot_dialog_() {
  if (!root() || (reboot_dialog_ && reboot_dialog_->visible())) {
    return;
  }

  platform::set_modal_key_capture(true);
  view::widgets::DialogConfig config;
  config.width               = 270;
  config.height              = 136;
  config.title               = "Upgrade complete";
  config.shortcut_text       = "ESC: Cancel  Enter: OK";
  config.ok_button_label     = "Reboot";
  config.cancel_button_label = "Cancel";
  config.button_width        = 92;
  config.button_row_width    = 206;
  config.button_bottom_pad   = 4;
  config.body_font_size      = 13;
  config.ok_button_tone      = view::widgets::DialogButtonTone::WARNING;

  view::widgets::DialogCallbacks callbacks;
  callbacks.ok_action     = [this]() { request_reboot_(); };
  callbacks.cancel_action = [this]() { close_reboot_dialog_(); };
  reboot_dialog_ = std::make_unique<view::widgets::Dialog>(
      root(), app_view_model_ref_(), assets_ref_(), config, callbacks);
  reboot_dialog_->build();
  reboot_dialog_->add_label("Upgrade completed successfully. Reboot now?",
                            246,
                            LV_TEXT_ALIGN_CENTER);
}

void Py32UpgradePage::close_reboot_dialog_() {
  if (reboot_dialog_) {
    reboot_dialog_->close();
  }
  platform::set_modal_key_capture(false);
}

void Py32UpgradePage::request_reboot_() {
  close_reboot_dialog_();
  std::string error_message;
  if (!platform::power::safe_reboot(error_message)) {
    LOG_ERROR("reboot after PY32 upgrade failed: {}", error_message);
    std::lock_guard<std::mutex> lock(job_state_.mutex);
    job_state_.status = "Reboot failed";
  }
}

void Py32UpgradePage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<Py32UpgradePage*>(user_data);
  if (!page) {
    return;
  }
  if (page->reboot_dialog_ && page->reboot_dialog_->visible()) {
    page->reboot_dialog_->handle_key(key, key_name);
    return;
  }
  if (is_enter_key(key)) {
    page->start_upgrade_();
  }
}

void Py32UpgradePage::theme_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<Py32UpgradePage*>(lv_observer_get_user_data(observer));
  if (page) {
    page->apply_progress_theme_(lv_subject_get_int(subject) != 0);
  }
}

void Py32UpgradePage::refresh_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<Py32UpgradePage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->refresh_();
  }
}

}  // namespace screen
