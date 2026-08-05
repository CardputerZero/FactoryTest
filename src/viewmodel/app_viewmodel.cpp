/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_viewmodel.h"

#include <algorithm>
#include <array>
#include <utility>

#include "audio_service.h"
#include "device_info_service.h"
#include "factory_upload_service.h"
#include "logger.h"
#include "version.h"

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) { return static_cast<int>(page); }

struct TestSequenceItem {
  const char* id;
  const char* name;
  model::AppPage page;
};

constexpr std::array<TestSequenceItem, 19> K_TEST_SEQUENCE = {{
    {"input", "Input Test", model::AppPage::KEYBOARD_TEST},
    {"display", "Display Test", model::AppPage::LCD_TEST},
    {"audio", "Audio Test", model::AppPage::AUDIO_TEST},
    {"camera", "Camera Test", model::AppPage::CAMERA_TEST},
    {"ir_fixture", "IR Fixture Test", model::AppPage::IR_FIXTURE_TEST},
    {"wifi", "Wi-Fi", model::AppPage::WIFI_TEST},
    {"bluetooth", "Bluetooth", model::AppPage::BT_TEST},
    {"ethernet", "Ethernet", model::AppPage::ETH_TEST},
    {"link", "Link Test", model::AppPage::LINK_TEST},
    {"hdmi", "HDMI", model::AppPage::HDMI_TEST},
    {"cap_fixture", "CAP Fixture Test", model::AppPage::CAP_FIXTURE_TEST},
    {"cap_lora_1262", "CAP LoRa-1262", model::AppPage::CAP_LORA_1262_TEST},
    {"cap_cc1101", "CAP-CC1101", model::AppPage::CAP_CC1101_TEST},
    {"device_info", "Device Information", model::AppPage::DEVICE_INFO},
    {"power", "Power Information", model::AppPage::POWER_INFO},
    {"imu", "IMU Test", model::AppPage::IMU_TEST},
    {"cpu", "CPU Benchmark", model::AppPage::CPU_BENCHMARK},
    {"memory", "Mem Stress Test", model::AppPage::MEM_STRESS_TEST},
    {"sd_card", "SD Card Test", model::AppPage::SD_CARD_TEST},
}};

bool sequence_item_enabled(const TestSequenceItem& item) {
  if (platform::device_info::product_model() ==
      platform::device_info::ProductModel::CARDPUTER_ZERO) {
    return true;
  }
  return item.page != model::AppPage::CAMERA_TEST && item.page != model::AppPage::IMU_TEST;
}

const TestSequenceItem* sequence_item(std::size_t index) {
  std::size_t enabled_index = 0;
  for (const auto& item : K_TEST_SEQUENCE) {
    if (!sequence_item_enabled(item)) {
      continue;
    }
    if (enabled_index == index) {
      return &item;
    }
    ++enabled_index;
  }
  return nullptr;
}

std::size_t test_sequence_size() {
  return static_cast<std::size_t>(
      std::count_if(K_TEST_SEQUENCE.begin(), K_TEST_SEQUENCE.end(), sequence_item_enabled));
}

std::vector<model::TestDefinition> test_sequence_plan() {
  std::vector<model::TestDefinition> plan;
  plan.reserve(test_sequence_size());
  for (const auto& item : K_TEST_SEQUENCE) {
    if (sequence_item_enabled(item)) {
      plan.push_back({item.id, item.name});
    }
  }
  return plan;
}

model::SessionMetadata session_metadata(const std::string& station_id) {
  model::SessionMetadata metadata;
  const auto product_model = platform::device_info::product_model();
  metadata.sku             = platform::device_info::product_model_sku(product_model);
  metadata.serial_number   = platform::device_info::read_serial_number();
  metadata.station         = station_id;
  metadata.firmware        = factory_test::get_version_str();
#if defined(FACTORY_TEST_GIT_COMMIT)
  metadata.commit = FACTORY_TEST_GIT_COMMIT;
#else
  metadata.commit = "UNKNOWN";
#endif
  return metadata;
}

}  // namespace

AppViewModel::AppViewModel(model::TranslationService& translations,
                           model::AppConfigStore& config_store)
    : translations_(translations),
      config_store_(config_store),
      title_msgid_(model_.app_title()),
      title_subject_(translations_.translate(title_msgid_).c_str()),
      title_alignment_subject_(static_cast<int32_t>(LV_ALIGN_LEFT_MID)),
      title_x_offset_subject_(8),
      title_y_offset_subject_(0),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false) {
  model_.set_dark_mode(config_store_.config().ui.dark_mode);
  dark_mode_subject_.set(model_.dark_mode());
  if (!translations_.set_language(config_store_.config().ui.language)) {
    config_store_.config().ui.language = "en";
    translations_.set_language("en");
    save_config_();
  }
  title_subject_.set(translations_.translate(title_msgid_).c_str());

  platform::factory_upload::FactoryUploadConfig upload_config;
  upload_config.tester_firmware = factory_test::get_version_str();
  factory_upload_service_ =
      std::make_unique<platform::factory_upload::FactoryUploadService>(std::move(upload_config));
  factory_upload_service_->start();
}

AppViewModel::~AppViewModel() = default;

lv_subject_t* AppViewModel::title_subject() { return title_subject_.native(); }

lv_subject_t* AppViewModel::title_alignment_subject() { return title_alignment_subject_.native(); }

lv_subject_t* AppViewModel::title_x_offset_subject() { return title_x_offset_subject_.native(); }

lv_subject_t* AppViewModel::title_y_offset_subject() { return title_y_offset_subject_.native(); }

lv_subject_t* AppViewModel::nav_actions_subject() { return nav_actions_subject_.native(); }

lv_subject_t* AppViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }

lv_subject_t* AppViewModel::language_subject() { return language_subject_.native(); }

lv_subject_t* AppViewModel::current_page_subject() { return current_page_subject_.native(); }

lv_subject_t* AppViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }

bool AppViewModel::is_dark_mode() const { return model_.dark_mode(); }

bool AppViewModel::ui_sounds_enabled() const { return config_store_.config().ui.key_click_enabled; }

const std::string& AppViewModel::language() const { return translations_.language(); }

const std::array<NavAction, 5>& AppViewModel::nav_actions() const { return nav_actions_; }

void AppViewModel::set_dark_mode(bool enabled) {
  model_.set_dark_mode(enabled);
  dark_mode_subject_.set(model_.dark_mode());
  config_store_.config().ui.dark_mode = model_.dark_mode();
  save_config_();
}

void AppViewModel::toggle_dark_mode() {
  model_.toggle_dark_mode();
  dark_mode_subject_.set(model_.dark_mode());
  config_store_.config().ui.dark_mode = model_.dark_mode();
  save_config_();
}

bool AppViewModel::set_ui_sounds_enabled(bool enabled) {
  auto& preference = config_store_.config().ui.key_click_enabled;
  if (preference == enabled) {
    return true;
  }

  const bool previous = preference;
  preference          = enabled;
  if (!save_config_()) {
    preference = previous;
    return false;
  }

  platform::audio::set_ui_sounds_enabled(enabled);
  if (enabled) {
    if (!platform::audio::initialize_ui_sounds()) {
      LOG_WARN("failed to initialize UI SFX after enabling sound");
    }
    platform::audio::play_ui_sound(platform::audio::UiSound::TOGGLE_ON);
  }
  return true;
}

bool AppViewModel::set_language(const std::string& locale) {
  if (!translations_.set_language(locale)) {
    return false;
  }

  title_subject_.set(translations_.translate(title_msgid_).c_str());
  language_subject_.set(language_subject_.value() + 1);
  config_store_.config().ui.language = locale;
  save_config_();
  return true;
}

int AppViewModel::uart_baud_rate() const { return config_store_.config().uart.baud_rate; }

bool AppViewModel::set_uart_baud_rate(int baud_rate) {
  config_store_.config().uart.baud_rate = baud_rate;
  return save_config_();
}

bool AppViewModel::set_iperf_settings(const std::string& host, int port) {
  if (host.empty() || port <= 0 || port > 65535) {
    return false;
  }
  config_store_.config().network.iperf_host = host;
  config_store_.config().network.iperf_port = port;
  return save_config_();
}

std::string AppViewModel::tr(const char* msgid) const {
  return translations_.translate(msgid ? msgid : "");
}

const char* AppViewModel::ui_font_name(const char* latin_font_name) const {
  return translations_.uses_cjk_font() ? "alibaba-puhui-regular.ttf" : latin_font_name;
}

model::AppPage AppViewModel::current_page() const { return model_.current_page(); }

bool AppViewModel::is_test_sequence_active() const { return model_.test_sequence_active(); }

void AppViewModel::set_title_text(const char* title) {
  title_msgid_ = title ? title : "";
  title_subject_.set(translations_.translate(title_msgid_).c_str());
}

void AppViewModel::set_title_alignment(lv_align_t align, int32_t x_offset, int32_t y_offset) {
  title_alignment_subject_.set(static_cast<int32_t>(align));
  title_x_offset_subject_.set(x_offset);
  title_y_offset_subject_.set(y_offset);
}

void AppViewModel::set_nav_action(std::size_t index, NavAction action) {
  if (index >= nav_actions_.size()) {
    return;
  }
  nav_actions_[index] = std::move(action);
  nav_actions_subject_.set(++nav_actions_revision_);
}

void AppViewModel::set_keypad_nav_action(uint32_t keypad, NavAction action) {
  if (keypad < '4' || keypad > '8') {
    return;
  }
  set_nav_action(static_cast<std::size_t>(keypad - '4'), std::move(action));
}

void AppViewModel::set_nav_actions(std::array<NavAction, 5> actions) {
  nav_actions_ = std::move(actions);
  nav_actions_subject_.set(++nav_actions_revision_);
}

void AppViewModel::clear_nav_actions() {
  nav_actions_ = {};
  nav_actions_subject_.set(++nav_actions_revision_);
}

bool AppViewModel::trigger_nav_action(std::size_t index, lv_event_code_t event_code) {
  if (index >= nav_actions_.size()) {
    return false;
  }
  auto& action = nav_actions_[index];
  if (!action.action || action.event_code != event_code) {
    return false;
  }
  action.action();
  return true;
}

void AppViewModel::notify_nav_action_pressed(std::size_t index) {
  if (index >= nav_actions_.size() || !nav_actions_[index].press_action) {
    return;
  }
  nav_actions_[index].press_action();
}

void AppViewModel::notify_nav_action_released(std::size_t index) {
  if (index >= nav_actions_.size() || !nav_actions_[index].release_action) {
    return;
  }
  nav_actions_[index].release_action();
}

void AppViewModel::show_start_page() {
  model_.set_test_sequence_active(false);
  show_page_(model::AppPage::START);
}

void AppViewModel::show_keyboard_test_page() { show_page_(model::AppPage::KEYBOARD_TEST); }

void AppViewModel::show_lcd_test_page() { show_page_(model::AppPage::LCD_TEST); }

void AppViewModel::show_audio_test_page() { show_page_(model::AppPage::AUDIO_TEST); }

void AppViewModel::show_camera_test_page() { show_page_(model::AppPage::CAMERA_TEST); }

void AppViewModel::show_ir_send_test_page() { show_page_(model::AppPage::IR_SEND_TEST); }

void AppViewModel::show_ir_receive_test_page() { show_page_(model::AppPage::IR_RECEIVE_TEST); }

void AppViewModel::show_imu_test_page() { show_page_(model::AppPage::IMU_TEST); }

void AppViewModel::show_power_info_page() { show_page_(model::AppPage::POWER_INFO); }

void AppViewModel::show_device_info_page() { show_page_(model::AppPage::DEVICE_INFO); }

void AppViewModel::show_perf_test_page() { show_page_(model::AppPage::PERF_TEST); }

void AppViewModel::show_test_result_page() { show_page_(model::AppPage::TEST_RESULT); }

bool AppViewModel::load_recoverable_test_session() {
  return session_manager_.load_latest_recoverable(test_sequence_plan());
}

bool AppViewModel::start_full_test_sequence() {
  if (!session_manager_.start_new(test_sequence_plan(),
                                  session_metadata(config_store_.config().factory.station_id))) {
    LOG_ERROR("failed to start test session: {}", session_manager_.last_error());
    platform::audio::play_ui_sound(platform::audio::UiSound::ERROR);
    return false;
  }
  model_.set_test_sequence_active(true);
  test_sequence_index_ = 0;
  if (!open_test_sequence_item_(test_sequence_index_)) {
    model_.set_test_sequence_active(false);
    return false;
  }
  platform::audio::play_ui_sound(platform::audio::UiSound::START);
  return true;
}

bool AppViewModel::save_config_() {
  if (config_store_.save()) {
    return true;
  }
  LOG_ERROR("failed to save config {}: {}",
            config_store_.path().string(),
            config_store_.last_error());
  return false;
}

bool AppViewModel::resume_full_test_sequence() {
  if (!session_manager_.recoverable() && !load_recoverable_test_session()) {
    return false;
  }

  test_sequence_index_ = session_manager_.next_incomplete_index();
  if (test_sequence_index_ >= test_sequence_size()) {
    if (!session_manager_.finalize()) {
      LOG_ERROR("failed to finalize recovered test session: {}", session_manager_.last_error());
      platform::audio::play_ui_sound(platform::audio::UiSound::ERROR);
      return false;
    }
    model_.set_test_sequence_active(false);
    show_test_result_page();
    platform::audio::play_ui_sound(platform::audio::UiSound::COMPLETE);
    return true;
  }

  model_.set_test_sequence_active(true);
  if (!open_test_sequence_item_(test_sequence_index_)) {
    model_.set_test_sequence_active(false);
    return false;
  }
  platform::audio::play_ui_sound(platform::audio::UiSound::START);
  return true;
}

void AppViewModel::show_single_test_page(model::AppPage page) {
  model_.set_test_sequence_active(false);
  show_page_(page);
}

void AppViewModel::refresh_current_page() { current_page_subject_.notify(); }

bool AppViewModel::begin_current_test_attempt() {
  const auto* item = model_.test_sequence_active() ? sequence_item(test_sequence_index_) : nullptr;
  return item && session_manager_.begin_test(item->id);
}

bool AppViewModel::record_current_test_evidence(const std::string& key,
                                                model::EvidenceValue value) {
  const auto* item = model_.test_sequence_active() ? sequence_item(test_sequence_index_) : nullptr;
  return item && session_manager_.record_evidence(item->id, key, std::move(value));
}

bool AppViewModel::record_current_test_evidence(const model::TestEvidence& evidence) {
  const auto* item = model_.test_sequence_active() ? sequence_item(test_sequence_index_) : nullptr;
  return item && session_manager_.record_evidence(item->id, evidence);
}

void AppViewModel::complete_current_test() { complete_current_test(model::TestResult::PASS); }

void AppViewModel::complete_current_test(model::TestResult result) {
  if (model_.test_sequence_active()) {
    const auto* item = sequence_item(test_sequence_index_);
    if (!item || !session_manager_.complete_test(item->id, result)) {
      LOG_ERROR("failed to persist test result: {}", session_manager_.last_error());
      platform::audio::play_ui_sound(platform::audio::UiSound::ERROR);
      return;
    }
  }

  const bool sequence_complete =
      model_.test_sequence_active() && test_sequence_index_ + 1 >= test_sequence_size();
  platform::audio::play_ui_sound(result == model::TestResult::FAIL
                                     ? platform::audio::UiSound::ERROR
                                     : (sequence_complete ? platform::audio::UiSound::COMPLETE
                                                          : platform::audio::UiSound::SUCCESS));
  advance_test_sequence_();
}

void AppViewModel::complete_current_test_with_details(
    model::TestResult result,
    const std::vector<model::NamedTestResult>& detail_results) {
  if (model_.test_sequence_active()) {
    const auto* item = sequence_item(test_sequence_index_);
    if (!item) {
      return;
    }
    if (!detail_results.empty()) {
      session_manager_.record_evidence(item->id,
                                       "subtest_count",
                                       model::EvidenceValue::number(detail_results.size()));
    }
    if (!session_manager_.complete_test(item->id, result, detail_results)) {
      LOG_ERROR("failed to persist detailed test result: {}", session_manager_.last_error());
      platform::audio::play_ui_sound(platform::audio::UiSound::ERROR);
      return;
    }
  }

  const bool sequence_complete =
      model_.test_sequence_active() && test_sequence_index_ + 1 >= test_sequence_size();
  platform::audio::play_ui_sound(result == model::TestResult::FAIL
                                     ? platform::audio::UiSound::ERROR
                                     : (sequence_complete ? platform::audio::UiSound::COMPLETE
                                                          : platform::audio::UiSound::SUCCESS));
  advance_test_sequence_();
}

void AppViewModel::advance_test_sequence_() {
  if (!model_.test_sequence_active()) {
    show_start_page();
    return;
  }

  ++test_sequence_index_;
  if (const auto* item = sequence_item(test_sequence_index_)) {
    if (!session_manager_.set_current_test(item->id)) {
      LOG_ERROR("failed to persist current test: {}", session_manager_.last_error());
      platform::audio::play_ui_sound(platform::audio::UiSound::ERROR);
      model_.set_test_sequence_active(false);
      show_start_page();
      return;
    }
    show_page_(item->page);
    return;
  }

  model_.set_test_sequence_active(false);
  show_test_result_page();
}

bool AppViewModel::open_test_sequence_item_(std::size_t index) {
  const auto* item = sequence_item(index);
  if (!item) {
    show_start_page();
    return false;
  }
  if (!session_manager_.set_current_test(item->id)) {
    LOG_ERROR("failed to persist current test: {}", session_manager_.last_error());
    platform::audio::play_ui_sound(platform::audio::UiSound::ERROR);
    show_start_page();
    return false;
  }
  show_page_(item->page);
  return true;
}

const char* AppViewModel::current_test_name() const {
  if (model_.test_sequence_active()) {
    if (const auto* item = sequence_item(test_sequence_index_)) {
      return item->name;
    }
  }

  switch (model_.current_page()) {
    case model::AppPage::KEYBOARD_TEST:
      return "Input Test";
    case model::AppPage::LCD_TEST:
      return "Display Test";
    case model::AppPage::AUDIO_TEST:
      return "Audio Test";
    case model::AppPage::CAMERA_TEST:
      return "Camera Test";
    case model::AppPage::IR_SEND_TEST:
      return "IR Sender";
    case model::AppPage::IR_RECEIVE_TEST:
      return "IR Receiver";
    case model::AppPage::IR_FIXTURE_TEST:
      return "IR Fixture Test";
    case model::AppPage::WIFI_TEST:
      return "Wi-Fi";
    case model::AppPage::BT_TEST:
      return "Bluetooth";
    case model::AppPage::ETH_TEST:
      return "Ethernet";
    case model::AppPage::USB_TEST:
      return "USB";
    case model::AppPage::HDMI_TEST:
      return "HDMI";
    case model::AppPage::I2C_TEST:
      return "I2C";
    case model::AppPage::SPI_TEST:
      return "SPI";
    case model::AppPage::UART_TEST:
      return "UART";
    case model::AppPage::EXT_IO_TEST:
      return "EXT.IO";
    case model::AppPage::LINK_TEST:
      return "Link Test";
    case model::AppPage::POWER_INFO:
      return "Power Information";
    case model::AppPage::IMU_TEST:
      return "IMU Test";
    case model::AppPage::DEVICE_INFO:
      return "Device Information";
    case model::AppPage::PY32_UPGRADE:
      return "IOE1 Upgrade";
    case model::AppPage::PERF_TEST:
      return "Performance Test";
    case model::AppPage::CPU_BENCHMARK:
      return "CPU Benchmark";
    case model::AppPage::MEM_STRESS_TEST:
      return "Mem Stress Test";
    case model::AppPage::SD_CARD_TEST:
      return "SD Card Test";
    case model::AppPage::CAP_FIXTURE_TEST:
      return "CAP Fixture Test";
    case model::AppPage::CAP_LORA_1262_TEST:
      return "CAP LoRa-1262";
    case model::AppPage::CAP_CC1101_TEST:
      return "CAP-CC1101";
    case model::AppPage::START:
    case model::AppPage::TEST_RESULT:
    default:
      return "Factory Test";
  }
}

std::size_t AppViewModel::current_test_number() const {
  if (!model_.test_sequence_active() || test_sequence_index_ >= test_sequence_size()) {
    return 0;
  }
  return test_sequence_index_ + 1;
}

std::size_t AppViewModel::test_count() const { return test_sequence_size(); }

const std::string& AppViewModel::test_session_path() const {
  return session_manager_.session_path();
}

const std::string& AppViewModel::test_result_path() const { return session_manager_.result_path(); }

const std::string& AppViewModel::test_session_id() const { return session_manager_.session_id(); }

const std::vector<model::TestRecord>& AppViewModel::test_records() const {
  return session_manager_.tests();
}

model::SessionSummary AppViewModel::test_session_summary() const {
  return session_manager_.summary();
}

bool AppViewModel::upload_test_result(std::string& error_message) {
  error_message.clear();
  if (!factory_upload_service_) {
    error_message = "Factory upload service is unavailable";
    return false;
  }

  const auto listener = factory_upload_service_->snapshot();
  if (!listener.listener_online) {
    error_message = "Factory upload listener is offline";
    LOG_ERROR("factory upload request rejected: listener heartbeat age={}ms",
              listener.listener_heartbeat_age_ms);
    return false;
  }

  auto payload = session_manager_.build_upload_result_json(false);
  if (payload.empty()) {
    error_message = "Completed test result is unavailable";
    LOG_ERROR("factory upload request rejected: {}", error_message);
    return false;
  }
  return factory_upload_service_->queue_result(std::move(payload), error_message);
}

TestUploadSnapshot AppViewModel::test_upload_snapshot() const {
  TestUploadSnapshot result;
  if (!factory_upload_service_) {
    result.message = "Factory upload service is unavailable";
    return result;
  }

  const auto snapshot              = factory_upload_service_->snapshot();
  result.message                   = snapshot.message;
  result.port                      = snapshot.port;
  result.revision                  = snapshot.revision;
  result.listener_online           = snapshot.listener_online;
  result.listener_heartbeat_age_ms = snapshot.listener_heartbeat_age_ms;
  switch (snapshot.state) {
    case platform::factory_upload::UploadState::SEARCHING:
      result.state = TestUploadState::SEARCHING;
      break;
    case platform::factory_upload::UploadState::WAITING_HANDSHAKE:
      result.state = TestUploadState::WAITING_HANDSHAKE;
      break;
    case platform::factory_upload::UploadState::READY:
      result.state = TestUploadState::READY;
      break;
    case platform::factory_upload::UploadState::QUEUED:
      result.state = TestUploadState::QUEUED;
      break;
    case platform::factory_upload::UploadState::SENDING:
      result.state = TestUploadState::SENDING;
      break;
    case platform::factory_upload::UploadState::WAITING_RESULT_ACK:
      result.state = TestUploadState::WAITING_RESULT_ACK;
      break;
    case platform::factory_upload::UploadState::SUCCEEDED:
      result.state = TestUploadState::SUCCEEDED;
      break;
    case platform::factory_upload::UploadState::FAILED:
      result.state = TestUploadState::FAILED;
      break;
    case platform::factory_upload::UploadState::UNAVAILABLE:
    default:
      result.state = TestUploadState::UNAVAILABLE;
      break;
  }
  return result;
}

void AppViewModel::set_back_request_handler(BackRequestHandler handler, void* user_data) {
  back_request_handler_   = handler;
  back_request_user_data_ = user_data;
}

void AppViewModel::clear_back_request_handler(BackRequestHandler handler, void* user_data) {
  if (back_request_handler_ == handler && back_request_user_data_ == user_data) {
    back_request_handler_   = nullptr;
    back_request_user_data_ = nullptr;
  }
}

void AppViewModel::request_back_or_quit() {
  if (back_request_handler_ && back_request_handler_(back_request_user_data_)) {
    return;
  }

  if (model_.current_page() == model::AppPage::START) {
    request_quit();
    return;
  }

  show_start_page();
}

void AppViewModel::request_quit() { quit_requested_subject_.set(true); }

void AppViewModel::publish_all_() {
  title_msgid_ = model_.app_title();
  title_subject_.set(translations_.translate(title_msgid_).c_str());
  dark_mode_subject_.set(model_.dark_mode());
  current_page_subject_.set(page_to_int(model_.current_page()));
}

void AppViewModel::show_page_(model::AppPage page) {
  clear_nav_actions();
  set_title_alignment(LV_ALIGN_LEFT_MID, 8, 0);
  model_.set_current_page(page);
  publish_all_();
}

}  // namespace viewmodel
