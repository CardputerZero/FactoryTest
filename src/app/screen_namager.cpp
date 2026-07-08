/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "audio_test_page.h"
#include "camera_test_page.h"
#include "device_info_page.h"
#include "imu_test_page.h"
#include "io_test_page.h"
#include "ir_test_page.h"
#include "keyboard_test_page.h"
#include "lcd_test_page.h"
#include "linux_input.h"
#include "perf_single_test_page.h"
#include "perf_test_page.h"
#include "placeholder_test_page.h"
#include "power_info_page.h"
#include "screen_manager.h"
#include "start_screen.h"
#include "test_result_page.h"

namespace app {
namespace {

void flush_requested_page_async(void* user_data) {
  auto* manager = static_cast<ScreenManager*>(user_data);
  if (manager) {
    manager->flush_requested_page();
  }
}

void current_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* manager = static_cast<ScreenManager*>(lv_observer_get_target(observer));
  if (!manager) {
    return;
  }

  manager->request_page(static_cast<model::AppPage>(lv_subject_get_int(subject)));
}

const char* placeholder_title(model::AppPage page) {
  switch (page) {
    case model::AppPage::AUDIO_TEST:
      return "Audio Test";
    case model::AppPage::CAMERA_TEST:
      return "Camera Test";
    case model::AppPage::IR_SEND_TEST:
      return "IR Sender";
    case model::AppPage::IR_RECEIVE_TEST:
      return "IR Receiver";
    case model::AppPage::IMU_TEST:
      return "IMU Test";
    case model::AppPage::POWER_INFO:
      return "Power Information";
    case model::AppPage::DEVICE_INFO:
      return "Device Information";
    case model::AppPage::PERF_TEST:
      return "Performance Test";
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
    case model::AppPage::CPU_BENCHMARK:
      return "CPU Benchmark";
    case model::AppPage::MEM_STRESS_TEST:
      return "Mem Stress Test";
    case model::AppPage::SD_CARD_TEST:
      return "SD Card Test";
    case model::AppPage::TEST_RESULT:
      return "Test Result";
    case model::AppPage::START:
    case model::AppPage::KEYBOARD_TEST:
    case model::AppPage::LCD_TEST:
      return "Factory Test";
  }

  return "Factory Test";
}

}  // namespace

ScreenManager::ScreenManager(viewmodel::AppViewModel& app_view_model,
                             viewmodel::StartMenuViewModel& start_menu_view_model,
                             viewmodel::KeyboardTestViewModel& keyboard_view_model,
                             viewmodel::LcdTestViewModel& lcd_view_model,
                             viewmodel::ConnectivityTestViewModel& connectivity_view_model,
                             viewmodel::PerfTestViewModel& perf_view_model,
                             AssetManager& assets)
    : app_view_model_(app_view_model),
      start_menu_view_model_(start_menu_view_model),
      keyboard_view_model_(keyboard_view_model),
      lcd_view_model_(lcd_view_model),
      connectivity_view_model_(connectivity_view_model),
      perf_view_model_(perf_view_model),
      assets_(assets) {}

ScreenManager::~ScreenManager() {
  lv_async_call_cancel(flush_requested_page_async, this);

  if (page_observer_) {
    lv_observer_remove(page_observer_);
    page_observer_ = nullptr;
  }
}

void ScreenManager::start() {
  if (page_observer_) {
    return;
  }

  page_observer_ = lv_subject_add_observer_with_target(app_view_model_.current_page_subject(),
                                                       current_page_observer,
                                                       this,
                                                       nullptr);
  request_page(app_view_model_.current_page());
}

void ScreenManager::show_start_page() {
  load_screen_(std::make_unique<screen::StartScreen>(app_view_model_,
                                                     start_menu_view_model_,
                                                     perf_view_model_,
                                                     connectivity_view_model_,
                                                     assets_));
  loaded_page_     = model::AppPage::START;
  has_loaded_page_ = true;
}

void ScreenManager::show_keyboard_test_page() {
  load_screen_(
      std::make_unique<screen::KeyboardTestPage>(app_view_model_, keyboard_view_model_, assets_));
  loaded_page_     = model::AppPage::KEYBOARD_TEST;
  has_loaded_page_ = true;
}

void ScreenManager::show_lcd_test_page() {
  load_screen_(std::make_unique<screen::LcdTestPage>(app_view_model_, lcd_view_model_, assets_));
  loaded_page_     = model::AppPage::LCD_TEST;
  has_loaded_page_ = true;
}

void ScreenManager::show_audio_test_page() {
  load_screen_(std::make_unique<screen::AudioTestPage>(app_view_model_, assets_));
  loaded_page_     = model::AppPage::AUDIO_TEST;
  has_loaded_page_ = true;
}

void ScreenManager::show_camera_test_page() {
  load_screen_(std::make_unique<screen::CameraTestPage>(app_view_model_, assets_));
  loaded_page_     = model::AppPage::CAMERA_TEST;
  has_loaded_page_ = true;
}

void ScreenManager::show_ir_test_page(model::AppPage page) {
  const auto subpage = page == model::AppPage::IR_RECEIVE_TEST
                           ? screen::IrTestPage::SubPage::RECEIVER
                           : screen::IrTestPage::SubPage::SENDER;
  load_screen_(std::make_unique<screen::IrTestPage>(app_view_model_, assets_, subpage));
  loaded_page_     = page;
  has_loaded_page_ = true;
}

void ScreenManager::show_imu_test_page() {
  load_screen_(std::make_unique<screen::ImuTestPage>(app_view_model_, assets_));
  loaded_page_     = model::AppPage::IMU_TEST;
  has_loaded_page_ = true;
}

void ScreenManager::show_power_info_page() {
  load_screen_(std::make_unique<screen::PowerInfoPage>(app_view_model_, assets_));
  loaded_page_     = model::AppPage::POWER_INFO;
  has_loaded_page_ = true;
}

void ScreenManager::show_device_info_page() {
  load_screen_(std::make_unique<screen::DeviceInfoPage>(app_view_model_, assets_));
  loaded_page_     = model::AppPage::DEVICE_INFO;
  has_loaded_page_ = true;
}

void ScreenManager::show_perf_test_page() {
  if (app_view_model_.is_test_sequence_active()) {
    perf_view_model_.show_menu();
  }
  load_screen_(std::make_unique<screen::PerfTestPage>(app_view_model_, perf_view_model_, assets_));
  loaded_page_     = model::AppPage::PERF_TEST;
  has_loaded_page_ = true;
}

void ScreenManager::show_io_test_page(model::AppPage page) {
  load_screen_(std::make_unique<screen::IoTestPage>(app_view_model_,
                                                    connectivity_view_model_,
                                                    assets_,
                                                    page));
  loaded_page_     = page;
  has_loaded_page_ = true;
}

void ScreenManager::show_perf_single_test_page(model::AppPage page) {
  load_screen_(std::make_unique<screen::PerfSingleTestPage>(app_view_model_, assets_, page));
  loaded_page_     = page;
  has_loaded_page_ = true;
}

void ScreenManager::show_test_result_page() {
  load_screen_(std::make_unique<screen::TestResultPage>(app_view_model_, assets_));
  loaded_page_     = model::AppPage::TEST_RESULT;
  has_loaded_page_ = true;
}

void ScreenManager::show_placeholder_page(model::AppPage page) {
  load_screen_(
      std::make_unique<screen::PlaceholderTestPage>(app_view_model_,
                                                    assets_,
                                                    placeholder_title(page),
                                                    "This test page is not implemented yet."));
  loaded_page_     = page;
  has_loaded_page_ = true;
}

void ScreenManager::request_page(model::AppPage page) {
  requested_page_ = page;

  if (page_switch_scheduled_) {
    return;
  }

  page_switch_scheduled_ = lv_async_call(flush_requested_page_async, this) == LV_RESULT_OK;
}

void ScreenManager::flush_requested_page() {
  page_switch_scheduled_ = false;

  if (has_loaded_page_ && requested_page_ == loaded_page_) {
    if (!app_view_model_.is_test_sequence_active()) {
      return;
    }
    has_loaded_page_ = false;
  }

  switch (requested_page_) {
    case model::AppPage::START:
      show_start_page();
      return;
    case model::AppPage::KEYBOARD_TEST:
      show_keyboard_test_page();
      return;
    case model::AppPage::LCD_TEST:
      show_lcd_test_page();
      return;
    case model::AppPage::AUDIO_TEST:
      show_audio_test_page();
      return;
    case model::AppPage::CAMERA_TEST:
      show_camera_test_page();
      return;
    case model::AppPage::IR_SEND_TEST:
    case model::AppPage::IR_RECEIVE_TEST:
      show_ir_test_page(requested_page_);
      return;
    case model::AppPage::IMU_TEST:
      show_imu_test_page();
      return;
    case model::AppPage::POWER_INFO:
      show_power_info_page();
      return;
    case model::AppPage::DEVICE_INFO:
      show_device_info_page();
      return;
    case model::AppPage::PERF_TEST:
      show_perf_test_page();
      return;
    case model::AppPage::WIFI_TEST:
    case model::AppPage::BT_TEST:
    case model::AppPage::ETH_TEST:
    case model::AppPage::USB_TEST:
    case model::AppPage::HDMI_TEST:
    case model::AppPage::I2C_TEST:
    case model::AppPage::SPI_TEST:
    case model::AppPage::UART_TEST:
    case model::AppPage::EXT_IO_TEST:
    case model::AppPage::LINK_TEST:
      show_io_test_page(requested_page_);
      return;
    case model::AppPage::CPU_BENCHMARK:
    case model::AppPage::MEM_STRESS_TEST:
    case model::AppPage::SD_CARD_TEST:
      show_perf_single_test_page(requested_page_);
      return;
    case model::AppPage::TEST_RESULT:
      show_test_result_page();
      return;
  }
}

lv_obj_t* ScreenManager::current_screen() const {
  return current_screen_ ? current_screen_->root() : nullptr;
}

void ScreenManager::load_screen_(std::unique_ptr<screen::BaseScreen> next_screen) {
  if (!next_screen || !next_screen->root()) {
    return;
  }

  platform::reset_key_router_state();
  lv_screen_load(next_screen->root());
  current_screen_ = std::move(next_screen);
}

}  // namespace app
