/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "precheck_model.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

#include "audio_service.h"
#include "camera_service.h"
#include "cap_fixture_test_model.h"
#include "device_info_service.h"
#include "ethernet_service.h"
#include "hdmi_service.h"
#include "imu_service.h"
#include "ir_fixture_test_model.h"
#include "logger.h"
#include "power_service.h"
#include "usb_service.h"

namespace precheck {
namespace {

constexpr const char* K_AUDIO_RECORDING = "/tmp/factory-precheck-audio.wav";

constexpr unsigned int K_CAMERA_FRAME_TIMEOUT_MS = 3000;
constexpr unsigned int K_CAMERA_FRAME_POLL_INTERVAL_MS = 20;

// 左侧 USB-C / USB-A 连接在 Bus 001 的 Hub（devpath 1）下。Device 编号会随
// 反复插拔漂移，物理拓扑编码保持不变：下游 Port 003 = USB-C，Port 002 = USB-A。
constexpr uint32_t K_USB_C_BUS          = 1;
constexpr const char* K_USB_C_PORT_PATH = "1.3";
constexpr uint32_t K_USB_A_BUS          = 1;
constexpr const char* K_USB_A_PORT_PATH = "1.2";

// 固定索引，与 make_items() 中的顺序一一对应。
constexpr std::size_t K_DEVICE_INFO_ITEM     = 0;
constexpr std::size_t K_USB_A_ITEM           = 1;
constexpr std::size_t K_USB_C_ITEM           = 2;
constexpr std::size_t K_HDMI_ITEM            = 3;
constexpr std::size_t K_CAP_FIXTURE_ITEM     = 4;
constexpr std::size_t K_IR_FIXTURE_ITEM      = 5;
constexpr std::size_t K_ETHERNET_ITEM        = 6;
constexpr std::size_t K_BATTERY_STATUS_ITEM  = 7;
constexpr std::size_t K_BATTERY_LEVEL_ITEM   = 8;
constexpr std::size_t K_BATTERY_VOLTAGE_ITEM = 9;
constexpr std::size_t K_BATTERY_CURRENT_ITEM = 10;
constexpr std::size_t K_AUDIO_ITEM           = 11;
constexpr std::size_t K_CAMERA_ITEM          = 12;
constexpr std::size_t K_IMU_ITEM             = 13;

std::vector<CapFixtureSubItem> make_cap_fixture_sub_items() {
  return {
      {"Power", CheckState::PENDING, "等待"},
      {"I2C", CheckState::PENDING, "等待"},
      {"Grove I2C", CheckState::PENDING, "等待"},
      {"UART", CheckState::PENDING, "等待"},
      {"Grove UART", CheckState::PENDING, "等待"},
      {"SPI", CheckState::PENDING, "等待"},
      {"USB", CheckState::PENDING, "等待"},
      {"GPIO", CheckState::PENDING, "等待"},
  };
}

std::vector<IrFixtureSubItem> make_ir_fixture_sub_items() {
  return {
      {"IR TX", CheckState::PENDING, "等待"},
      {"IR RX", CheckState::PENDING, "等待"},
  };
}

std::vector<CheckItem> make_items() {
  return {
      {"device_info", "设备信息", CheckState::PENDING, "等待"},
      {"usb_a", "左侧 USB-A", CheckState::PENDING, "等待"},
      {"usb_c", "左侧 USB-C", CheckState::PENDING, "等待"},
      {"hdmi", "HDMI 状态", CheckState::PENDING, "等待"},
      {"cap_fixture", "CAP 治具检测", CheckState::PENDING, "等待", make_cap_fixture_sub_items(), {}},
      {"ir_fixture", "IR 治具检测", CheckState::PENDING, "等待", {}, make_ir_fixture_sub_items()},
      {"ethernet", "以太网", CheckState::PENDING, "等待"},
      {"battery_status", "电池充电状态", CheckState::PENDING, "等待"},
      {"battery_level", "电池电量", CheckState::PENDING, "等待"},
      {"battery_voltage", "电池电压", CheckState::PENDING, "等待"},
      {"battery_current", "电池电流", CheckState::PENDING, "等待"},
      {"audio", "音频录放", CheckState::PENDING, "等待"},
      {"camera", "相机", CheckState::PENDING, "等待"},
      {"imu", "IMU", CheckState::PENDING, "等待"},
  };
}

CheckState cap_fixture_sub_state(model::CapFixtureItemState state) {
  switch (state) {
    case model::CapFixtureItemState::RUNNING:
      return CheckState::RUNNING;
    case model::CapFixtureItemState::PASSED:
      return CheckState::PASSED;
    case model::CapFixtureItemState::FAILED:
      return CheckState::FAILED;
    case model::CapFixtureItemState::PENDING:
    default:
      return CheckState::PENDING;
  }
}

CheckState ir_fixture_sub_state(model::IrFixtureItemState state) {
  switch (state) {
    case model::IrFixtureItemState::RUNNING:
      return CheckState::RUNNING;
    case model::IrFixtureItemState::PASSED:
      return CheckState::PASSED;
    case model::IrFixtureItemState::FAILED:
      return CheckState::FAILED;
    case model::IrFixtureItemState::PENDING:
    default:
      return CheckState::PENDING;
  }
}

bool skip_camera_and_imu() {
  return platform::device_info::product_model() ==
         platform::device_info::ProductModel::CARDPUTER_ZERO_LITE;
}

const char* battery_status_text(const std::string& status) {
  if (status == "Charging") return "充电中";
  if (status == "Discharging") return "放电中";
  if (status == "Full") return "已充满";
  if (status == "Not charging") return "未充电";
  if (status == "Unknown") return "未知";
  return status.empty() ? "未读取到充电状态" : status.c_str();
}

}  // namespace

PrecheckModel::PrecheckModel() : snapshot_{} {
  snapshot_.items = make_items();
  const auto model = platform::device_info::product_model();
  snapshot_.device_name = platform::device_info::product_model_name(model);
  snapshot_.serial_number = platform::device_info::read_serial_number();
}

PrecheckModel::~PrecheckModel() { cancel(); }

void PrecheckModel::start() {
  if (worker_.joinable()) {
    worker_.join();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (snapshot_.state == RunState::RUNNING) {
    return;
  }
  snapshot_.items = make_items();
  snapshot_.current_index = 0;
  snapshot_.completed = 0;
  snapshot_.passed = 0;
  snapshot_.failed = 0;
  snapshot_.state = RunState::RUNNING;
  snapshot_.headline = "正在执行自动初检";
  cancel_requested_.store(false);
  worker_ = std::thread(&PrecheckModel::run_, this);
}

void PrecheckModel::cancel() {
  cancel_requested_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (snapshot_.state == RunState::RUNNING) {
    snapshot_.state = RunState::CANCELED;
    snapshot_.headline = "初检已取消";
  }
}

void PrecheckModel::retry_failed() {
  if (worker_.joinable()) {
    worker_.join();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (snapshot_.state != RunState::FAILED && snapshot_.state != RunState::PASSED) {
    return;
  }
  for (auto& item : snapshot_.items) {
    if (item.state == CheckState::FAILED) {
      item.state = CheckState::PENDING;
      item.detail = "等待重试";
      --snapshot_.completed;
      --snapshot_.failed;
      if (item.id == "cap_fixture") {
        for (auto& sub : item.cap_sub_items) {
          sub.state = CheckState::PENDING;
          sub.detail = "等待";
        }
      }
      if (item.id == "ir_fixture") {
        for (auto& sub : item.ir_sub_items) {
          sub.state = CheckState::PENDING;
          sub.detail = "等待";
        }
      }
    }
  }
  snapshot_.state = RunState::RUNNING;
  snapshot_.headline = "正在重试失败项目";
  cancel_requested_.store(false);
  worker_ = std::thread(&PrecheckModel::run_, this);
}

bool PrecheckModel::safe_poweroff(std::string& error_message) {
  const bool ret = platform::power::safe_shutdown(error_message);
  if (!ret) {
    LOG_ERROR("Unable to perform safe poweroff, err: %s", error_message.c_str());
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.headline = "关机失败: " + error_message;
  }
  return ret;
}

bool PrecheckModel::safe_reboot(std::string& error_message) {
  const bool ret = platform::power::safe_reboot(error_message);
  if (!ret) {
    LOG_ERROR("Unable to perform safe reboot, err: %s", error_message.c_str());
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.headline = "重启失败: " + error_message;
  }
  return ret;
}

Snapshot PrecheckModel::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

bool PrecheckModel::canceled_() const { return cancel_requested_.load(); }

void PrecheckModel::publish_(RunState state, const std::string& headline) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.state = state;
  snapshot_.headline = headline;
}

void PrecheckModel::run_() {
  if (skip_camera_and_imu()) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto index : {K_CAMERA_ITEM, K_IMU_ITEM}) {
      auto& item = snapshot_.items[index];
      item.state = CheckState::SKIPPED;
      item.detail = "机型不支持";
      ++snapshot_.completed;
    }
    LOG_INFO("skipping camera and IMU checks for device model {}", snapshot_.device_name);
  }

  for (std::size_t i = 0; i < snapshot().items.size(); ++i) {
    if (canceled_()) {
      publish_(RunState::CANCELED, "初检已取消");
      return;
    }
    if (snapshot().items[i].state != CheckState::PENDING) {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.current_index = i;
      snapshot_.items[i].state = CheckState::RUNNING;
      snapshot_.items[i].detail = "检测中";
    }
    const bool passed = run_check_(i);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto& item = snapshot_.items[i];
      if (cancel_requested_.load()) {
        item.state = CheckState::CANCELED;
        item.detail = "已取消";
        snapshot_.state = RunState::CANCELED;
        snapshot_.headline = "初检已取消";
        return;
      }
      item.state = passed ? CheckState::PASSED : CheckState::FAILED;
      if (item.detail == "检测中") {
        item.detail = passed ? "通过" : "失败";
      }
      ++snapshot_.completed;
      passed ? ++snapshot_.passed : ++snapshot_.failed;
    }
  }
  const auto final = snapshot();
  publish_(final.failed == 0 ? RunState::PASSED : RunState::FAILED,
           final.failed == 0 ? "自动初检通过" : "自动初检失败");
}

bool PrecheckModel::run_check_(std::size_t index) {
  std::string error;
  switch (index) {
    case K_DEVICE_INFO_ITEM: {
      const auto fields = platform::device_info::read_device_info_fields();
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail = fields.empty() ? "未读取到设备信息" :
                                                std::to_string(fields.size()) + " 项信息";
      return !fields.empty();
    }
    case K_HDMI_ITEM: {
      const auto info = platform::connectivity::read_hdmi_info(error);
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail = error.empty() ? info.status : error;
      return error.empty() && info.connected;
    }
    case K_USB_C_ITEM:
      return run_usb_port_check_(index, K_USB_C_BUS, K_USB_C_PORT_PATH);
    case K_USB_A_ITEM:
      return run_usb_port_check_(index, K_USB_A_BUS, K_USB_A_PORT_PATH);
    case K_CAP_FIXTURE_ITEM:
      return run_cap_fixture_check_(index);
    case K_IR_FIXTURE_ITEM:
      return run_ir_fixture_check_(index);
    case K_ETHERNET_ITEM: {
      const auto info = platform::connectivity::read_ethernet_info(error);
      std::lock_guard<std::mutex> lock(mutex_);
      auto& item = snapshot_.items[index];
      if (!error.empty()) {
        item.detail = error;
        return false;
      }
      if (info.interface_name.empty()) {
        item.detail = "未找到以太网设备";
        return false;
      }
      if (!info.connected) {
        if (info.state == "unavailable") {
          item.detail = "网线未插入";
        } else if (info.state == "disconnected") {
          item.detail = "网线已插入，未获取到 IP 地址";
        } else {
          item.detail = "未连接: " + info.state;
        }
        return false;
      }
      if (info.ip4_address.empty()) {
        item.detail = "已连接，未获取到 IP 地址";
        return false;
      }
      item.detail = info.interface_name + " " + info.ip4_address;
      return true;
    }
    case K_BATTERY_STATUS_ITEM: {
      platform::power::PowerSupplyInfo info;
      const bool read_ok = platform::power::read_battery_info(info, error);
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail =
          read_ok ? battery_status_text(info.status) : (error.empty() ? "未读取到电池信息" : error);
      return read_ok && !info.status.empty();
    }
    case K_BATTERY_LEVEL_ITEM: {
      platform::power::PowerSupplyInfo info;
      const bool read_ok = platform::power::read_battery_info(info, error);
      const bool ok = read_ok && info.capacity_percent >= 0;
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail =
          ok ? std::to_string(info.capacity_percent) + "%"
             : (error.empty() ? "未读取到电量" : error);
      return ok;
    }
    case K_BATTERY_VOLTAGE_ITEM: {
      platform::power::PowerSupplyInfo info;
      const bool read_ok = platform::power::read_battery_info(info, error);
      const bool ok = read_ok && info.voltage_now_uv > 0;
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail =
          ok ? std::to_string(info.voltage_now_uv / 1000) + " mV"
             : (error.empty() ? "未读取到电压" : error);
      return ok;
    }
    case K_BATTERY_CURRENT_ITEM: {
      platform::power::PowerSupplyInfo info;
      const bool read_ok = platform::power::read_battery_info(info, error);
      const bool ok = read_ok && info.current_now_ua != -1;
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail =
          ok ? std::to_string(info.current_now_ua / 1000) + " mA"
             : (error.empty() ? "未读取到电流" : error);
      return ok;
    }
    case K_AUDIO_ITEM: {
      platform::audio::AudioDevice device;
      const bool found = platform::audio::find_audio_device(device, error);
      bool passed = found && platform::audio::record_wav(device, K_AUDIO_RECORDING, 1);
      if (passed) {
        passed = platform::audio::play_wav(device, K_AUDIO_RECORDING);
      }
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail = passed ? "录音/播放通过" : (error.empty() ? "录音或播放失败" : error);
      return passed;
    }
    case K_CAMERA_ITEM: {
      platform::camera::CameraInfo camera;
      const bool found = platform::camera::find_mipi_csi_camera(camera, error);
      platform::camera::PreviewSession preview;
      bool passed = found && preview.start(camera, error);
      std::vector<uint16_t> frame;
      int width = 0;
      int height = 0;
      if (passed) {
        // A successful libcamera start only means that requests were queued;
        // the first completed request arrives asynchronously.
        passed = false;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(K_CAMERA_FRAME_TIMEOUT_MS);
        while (!canceled_() && std::chrono::steady_clock::now() < deadline) {
          if (preview.copy_frame_rgb565(frame, width, height)) {
            passed = true;
            break;
          }
          std::this_thread::sleep_for(
              std::chrono::milliseconds(K_CAMERA_FRAME_POLL_INTERVAL_MS));
        }
        if (!passed && error.empty()) {
          error = canceled_() ? "相机检测已取消" : "等待相机首帧超时";
        }
      }
      preview.stop();
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail = passed ? (std::to_string(width) + "x" + std::to_string(height))
                                             : (error.empty() ? "未取得有效帧" : error);
      return passed;
    }
    case K_IMU_ITEM: {
      platform::imu::ImuDevice device;
      platform::imu::NineAxisReading reading;
      const bool found = platform::imu::find_bmi270_device(device, error);
      const bool passed = found && platform::imu::read_nine_axis(device, reading, error) &&
                          std::isfinite(reading.accel_x) && std::isfinite(reading.gyro_x);
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail = passed ? "读取通过" : (error.empty() ? "读取失败" : error);
      return passed;
    }
    default:
      return false;
  }
}

// Runs the same CAP fixture sequence used by factory_test through the shared
// model and mirrors each sub item into the precheck snapshot. The shared model
// owns Grove routing: grove_fun is enabled only inside run_grove_i2c_(), never
// as part of precheck initialization or another CAP sub-test.
bool PrecheckModel::run_cap_fixture_check_(std::size_t index) {
  // Replacing the previous instance first destroys it, which joins its worker
  // and waits for hardware cleanup before a new sequence starts.
  cap_fixture_model_ = std::make_unique<model::CapFixtureTestModel>();
  cap_fixture_model_->start();

  for (;;) {
    if (canceled_()) {
      cap_fixture_model_->cancel();
      return false;
    }
    const auto fixture = cap_fixture_model_->snapshot();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto& item = snapshot_.items[index];
      const auto count = std::min(fixture.items.size(), item.cap_sub_items.size());
      for (std::size_t i = 0; i < count; ++i) {
        item.cap_sub_items[i].name = fixture.items[i].name ? fixture.items[i].name : "";
        item.cap_sub_items[i].state = cap_fixture_sub_state(fixture.items[i].state);
        item.cap_sub_items[i].detail = fixture.items[i].detail;
      }
    }
    if (fixture.state == model::CapFixtureRunState::PASSED) {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail = "8 项全部通过";
      return true;
    }
    if (fixture.state == model::CapFixtureRunState::FAILED ||
        fixture.state == model::CapFixtureRunState::CANCELED) {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail =
          fixture.error_message.empty() ? "检测失败" : fixture.error_message;
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

// Reuse the factory_test IR fixture sequence: send four NEC32 frames first,
// then receive and validate the fixture's five replies.
bool PrecheckModel::run_ir_fixture_check_(std::size_t index) {
  ir_fixture_model_ = std::make_unique<model::IrFixtureTestModel>();
  ir_fixture_model_->start();

  for (;;) {
    if (canceled_()) {
      ir_fixture_model_->cancel();
      return false;
    }
    const auto fixture = ir_fixture_model_->snapshot();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto& item = snapshot_.items[index];
      const auto count = std::min(fixture.items.size(), item.ir_sub_items.size());
      for (std::size_t i = 0; i < count; ++i) {
        item.ir_sub_items[i].name = fixture.items[i].name ? fixture.items[i].name : "";
        item.ir_sub_items[i].state = ir_fixture_sub_state(fixture.items[i].state);
        item.ir_sub_items[i].detail = fixture.items[i].detail;
      }
      item.detail = fixture.headline;
    }
    if (fixture.state == model::IrFixtureRunState::PASSED) {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail = "IR 治具返回帧全部通过";
      return true;
    }
    if (fixture.state == model::IrFixtureRunState::FAILED ||
        fixture.state == model::IrFixtureRunState::CANCELED) {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot_.items[index].detail =
          fixture.error_message.empty() ? "IR 治具检测失败" : fixture.error_message;
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

// 按固定的 USB 物理拓扑位置检查设备。Device 编号只用于诊断，不参与判定。
bool PrecheckModel::run_usb_port_check_(std::size_t index,
                                        uint32_t expected_bus,
                                        const std::string& expected_port_path) {
  std::string error;
  const auto devices = platform::connectivity::list_usb_port_devices(error);
  std::lock_guard<std::mutex> lock(mutex_);
  auto& item = snapshot_.items[index];
  if (!error.empty()) {
    item.detail = error;
    return false;
  }
  for (const auto& device : devices) {
    if (platform::connectivity::is_usb_device_at_port(
            device, expected_bus, expected_port_path)) {
      const std::string vid = device.vid.empty() ? "????" : device.vid;
      const std::string pid = device.pid.empty() ? "????" : device.pid;
      item.detail = vid + ":" + pid + " " + (device.name.empty() ? "USB 设备" : device.name);
      return true;
    }
  }
  item.detail = "未枚举到 Bus " + std::to_string(expected_bus) + " Port " + expected_port_path;
  return false;
}

const char* run_state_text(RunState state) {
  switch (state) {
    case RunState::RUNNING: return "运行中";
    case RunState::PASSED: return "通过";
    case RunState::FAILED: return "失败";
    case RunState::CANCELED: return "已取消";
    case RunState::IDLE: default: return "待机";
  }
}

const char* check_state_text(CheckState state) {
  switch (state) {
    case CheckState::RUNNING: return "运行";
    case CheckState::PASSED: return "通过";
    case CheckState::FAILED: return "失败";
    case CheckState::CANCELED: return "取消";
    case CheckState::SKIPPED: return "跳过";
    case CheckState::PENDING: default: return "等待";
  }
}

}  // namespace precheck
