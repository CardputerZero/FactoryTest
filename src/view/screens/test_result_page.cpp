/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "test_result_page.h"

#include <string>
#include <utility>
#include <vector>

#include "asset_manager.h"
#include "audio_service.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_LIST_WIDTH      = 300;
constexpr int32_t K_LIST_HEIGHT     = 106;
constexpr uint32_t K_UPLOAD_POLL_MS = 100;

const char* icon_for_test_name(const std::string& name) {
  if (name == "Input Test") {
    return view::ICON_KEYBOARD;
  }
  if (name == "Display Test" || name == "HDMI") {
    return view::ICON_MONITOR;
  }
  if (name == "Audio Test") {
    return view::ICON_MICROPHONE;
  }
  if (name == "Camera Test") {
    return view::ICON_CAMERA;
  }
  if (name == "IR Sender") {
    return view::ICON_PAPER_PLANE;
  }
  if (name == "IR Receiver") {
    return view::ICON_ENVELOPE_OPEN;
  }
  if (name == "IR Fixture Test") {
    return view::ICON_FLASK;
  }
  if (name.rfind("IR Fixture - ", 0) == 0) {
    return view::ICON_BROADCAST;
  }
  if (name == "Wi-Fi") {
    return view::ICON_WIFI;
  }
  if (name == "Bluetooth") {
    return view::ICON_BLUETOOTH;
  }
  if (name == "Ethernet") {
    return view::ICON_ETHERNET;
  }
  if (name == "USB") {
    return view::ICON_USB;
  }
  if (name == "I2C") {
    return view::ICON_SCAN;
  }
  if (name == "SPI") {
    return view::ICON_LINK_SIMPLE_HOR;
  }
  if (name == "UART") {
    return view::ICON_BROADCAST;
  }
  if (name == "EXT.IO") {
    return view::ICON_PLUGS_CONNECTED;
  }
  if (name == "CAP Fixture Test") {
    return view::ICON_FLASK;
  }
  if (name.rfind("CAP Fixture - ", 0) == 0) {
    return view::ICON_PLUGS_CONNECTED;
  }
  if (name == "CAP LoRa-1262" || name == "CAP-CC1101") {
    return view::ICON_BROADCAST;
  }
  if (name == "Link Test") {
    return view::ICON_GLOBE;
  }
  if (name == "Device Information") {
    return view::ICON_INFO;
  }
  if (name == "Power Information") {
    return view::ICON_LIGHTNING;
  }
  if (name == "IMU Test") {
    return view::ICON_VECTOR_THREE;
  }
  if (name == "CPU Benchmark") {
    return view::ICON_CPU;
  }
  if (name == "Mem Stress Test") {
    return view::ICON_MEMORY;
  }
  if (name == "SD Card Test") {
    return view::ICON_HARDDRIVE;
  }
  return view::ICON_INFO;
}

view::widgets::IconList::Status status_for_result(model::TestResult result) {
  return result == model::TestResult::PASS ? view::widgets::IconList::Status::PASS
                                           : view::widgets::IconList::Status::FAIL;
}

std::vector<view::widgets::IconList::Item> load_result_items(
    viewmodel::AppViewModel& app_view_model,
    std::vector<std::string>& titles) {
  std::vector<view::widgets::IconList::Item> items;
  titles.clear();
  const auto& records = app_view_model.test_records();
  if (records.empty()) {
    titles.push_back("No test results found");
    items.push_back(
        {view::ICON_INFO, titles.back().c_str(), false, view::widgets::IconList::Status::WARN});
    return items;
  }

  std::size_t item_count = records.size();
  for (const auto& record : records) {
    item_count += record.details.size();
  }
  items.reserve(item_count);
  titles.reserve(item_count);
  for (const auto& record : records) {
    std::string title = app_view_model.tr(record.name.c_str());
    if (!record.has_evidence()) {
      title += " - " + app_view_model.tr("No evidence");
    }
    titles.push_back(std::move(title));
    items.push_back({icon_for_test_name(record.name),
                     titles.back().c_str(),
                     false,
                     record.completed ? status_for_result(record.result)
                                      : view::widgets::IconList::Status::FAIL});
    for (const auto& detail : record.details) {
      titles.push_back(app_view_model.tr(detail.test_name.c_str()));
      items.push_back({icon_for_test_name(detail.test_name),
                       titles.back().c_str(),
                       false,
                       status_for_result(detail.result)});
    }
  }
  return items;
}

}  // namespace

TestResultPage::TestResultPage(viewmodel::AppViewModel& app_view_model, app::AssetManager& assets)
    : BaseScreen(app_view_model, assets) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() {
    app_view_model_ref_().show_start_page();
  });
  set_nav_action_('8', view::ICON_UPLOAD_SIMPLE, [this]() { start_upload_(); });
  init();
  platform::set_key_listener(key_listener, this);
  last_upload_revision_ = app_view_model_ref_().test_upload_snapshot().revision;
  upload_timer_         = lv_timer_create(upload_timer_cb, K_UPLOAD_POLL_MS, this);
}

TestResultPage::~TestResultPage() {
  platform::clear_key_listener(key_listener, this);
  if (upload_timer_) {
    lv_timer_delete(upload_timer_);
    upload_timer_ = nullptr;
  }
  upload_popup_.reset();
  result_list_.reset();
}

void TestResultPage::build_content(lv_obj_t* content) {
  auto* viewport = lv_obj_create(content);
  lv_obj_remove_style_all(viewport);
  lv_obj_set_size(viewport, view::K_SCREEN_WIDTH, K_LIST_HEIGHT);
  lv_obj_align(viewport, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(viewport,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  auto items  = load_result_items(app_view_model_ref_(), result_titles_);
  item_count_ = items.size();
  auto* text_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 14);
  auto* icon_font = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  result_list_ =
      std::make_unique<view::widgets::IconList>(viewport,
                                                app_view_model_ref_(),
                                                items,
                                                text_font ? text_font : &lv_font_montserrat_14,
                                                icon_font ? icon_font : &lv_font_montserrat_14,
                                                K_LIST_WIDTH,
                                                K_LIST_HEIGHT,
                                                nullptr,
                                                nullptr);
  result_list_->build();
  result_list_->set_selected_index(selected_index_);
  result_list_->set_focused(true);
}

void TestResultPage::move_selection_(int32_t direction) {
  if (!result_list_ || item_count_ == 0 || direction == 0) {
    return;
  }

  const auto previous = selected_index_;
  if (direction < 0) {
    selected_index_ = selected_index_ == 0 ? 0 : selected_index_ - 1;
  } else if (selected_index_ + 1 < item_count_) {
    ++selected_index_;
  }
  result_list_->set_selected_index(selected_index_);
  if (selected_index_ != previous) {
    platform::audio::play_ui_sound(platform::audio::UiSound::SELECT);
  }
}

void TestResultPage::start_upload_() {
  std::string error_message;
  if (!app_view_model_ref_().upload_test_result(error_message)) {
    show_upload_popup_(error_message.empty() ? "Unable to upload test result" : error_message,
                       view::widgets::PopupTone::ERROR,
                       3500);
    return;
  }

  upload_requested_ = true;
  show_upload_popup_("Uploading test result", view::widgets::PopupTone::DEFAULT, 60000);
}

void TestResultPage::refresh_upload_() {
  const auto snapshot = app_view_model_ref_().test_upload_snapshot();
  if (snapshot.revision == last_upload_revision_) {
    return;
  }
  last_upload_revision_ = snapshot.revision;
  if (!upload_requested_) {
    return;
  }

  switch (snapshot.state) {
    case viewmodel::TestUploadState::SUCCEEDED:
      show_upload_popup_(snapshot.message.empty() ? "Test result uploaded" : snapshot.message,
                         view::widgets::PopupTone::SUCCESS,
                         3000);
      upload_requested_ = false;
      break;
    case viewmodel::TestUploadState::FAILED:
    case viewmodel::TestUploadState::UNAVAILABLE:
      show_upload_popup_(snapshot.message.empty() ? "Test result upload failed" : snapshot.message,
                         view::widgets::PopupTone::ERROR,
                         4500);
      upload_requested_ = false;
      break;
    case viewmodel::TestUploadState::SEARCHING:
    case viewmodel::TestUploadState::WAITING_HANDSHAKE:
    case viewmodel::TestUploadState::QUEUED:
    case viewmodel::TestUploadState::SENDING:
    case viewmodel::TestUploadState::WAITING_RESULT_ACK:
      show_upload_popup_(snapshot.message.empty() ? "Uploading test result" : snapshot.message,
                         view::widgets::PopupTone::DEFAULT,
                         60000);
      break;
    case viewmodel::TestUploadState::READY:
    default:
      break;
  }
}

void TestResultPage::show_upload_popup_(const std::string& message,
                                        view::widgets::PopupTone tone,
                                        uint32_t duration_ms) {
  upload_popup_.reset();
  if (!root()) {
    return;
  }

  view::widgets::PopupConfig config;
  config.width       = 286;
  config.height      = 46;
  config.label_width = 268;
  config.radius      = 6;
  config.tone        = tone;
  config.message     = app_view_model_ref_().tr(message.c_str());
  config.font = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 13);
  upload_popup_ =
      std::make_unique<view::widgets::Popup>(root(), app_view_model_ref_(), std::move(config));
  upload_popup_->build();
  upload_popup_->show_for(duration_ms);
}

void TestResultPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<TestResultPage*>(user_data);
  if (!page) {
    return;
  }

  switch (key) {
    case LV_KEY_UP:
    case 'f':
    case 'F':
      page->move_selection_(-1);
      break;
    case LV_KEY_DOWN:
    case 'x':
    case 'X':
      page->move_selection_(1);
      break;
    default:
      break;
  }
}

void TestResultPage::upload_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<TestResultPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->refresh_upload_();
  }
}

}  // namespace screen
