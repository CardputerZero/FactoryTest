/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "imu_service.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace platform::imu {
namespace {

std::string trim(std::string value) {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
                return !is_space(static_cast<unsigned char>(ch));
              }));
  value.erase(std::find_if(value.rbegin(),
                           value.rend(),
                           [&](char ch) { return !is_space(static_cast<unsigned char>(ch)); })
                  .base(),
              value.end());
  return value;
}

std::string lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool read_text_file(const std::filesystem::path& path, std::string& value) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  value = trim(buffer.str());
  return true;
}

bool read_double_file(const std::filesystem::path& path, double& value) {
  std::string text;
  if (!read_text_file(path, text)) {
    return false;
  }

  try {
    value = std::stod(text);
  } catch (...) {
    return false;
  }
  return true;
}

bool contains_bmi270(const std::string& text) {
  const auto lower = lower_copy(text);
  return lower.find(K_BMI270_DEVICE_NAME) != std::string::npos ||
         lower.find(K_BMI270_COMPATIBLE) != std::string::npos;
}

bool contains_bmm150(const std::string& text) {
  const auto lower = lower_copy(text);
  return lower.find(K_BMM150_DEVICE_NAME) != std::string::npos ||
         lower.find(K_BMM150_COMPATIBLE) != std::string::npos;
}

bool is_i2c_bmi270_node(const std::filesystem::path& path) {
  const auto filename = path.filename().string();
  if (filename.size() >= std::char_traits<char>::length(K_BMI270_I2C_ADDRESS_SUFFIX) &&
      filename.compare(
          filename.size() - std::char_traits<char>::length(K_BMI270_I2C_ADDRESS_SUFFIX),
          std::char_traits<char>::length(K_BMI270_I2C_ADDRESS_SUFFIX),
          K_BMI270_I2C_ADDRESS_SUFFIX) == 0) {
    return true;
  }

  std::string text;
  if (read_text_file(path / "name", text) && contains_bmi270(text)) {
    return true;
  }
  if (read_text_file(path / "modalias", text) && contains_bmi270(text)) {
    return true;
  }
  if (read_text_file(path / "of_node" / "compatible", text) && contains_bmi270(text)) {
    return true;
  }
  return false;
}

bool find_i2c_node(std::string& i2c_path) {
  const std::filesystem::path root(K_BMI270_I2C_SYSFS_ROOT);
  if (!std::filesystem::exists(root)) {
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(root)) {
    if (!entry.is_directory() && !entry.is_symlink()) {
      continue;
    }
    if (is_i2c_bmi270_node(entry.path())) {
      i2c_path = entry.path().string();
      return true;
    }
  }
  return false;
}

bool is_iio_bmi270_node(const std::filesystem::path& path) {
  std::string name;
  if (read_text_file(path / "name", name) && contains_bmi270(name)) {
    return true;
  }

  return std::filesystem::exists(path / K_IIO_ACCEL_X_RAW) &&
         std::filesystem::exists(path / K_IIO_GYRO_X_RAW);
}

bool is_iio_bmm150_node(const std::filesystem::path& path) {
  std::string name;
  if (read_text_file(path / "name", name) && contains_bmm150(name)) {
    return true;
  }

  return std::filesystem::exists(path / K_IIO_MAGN_X_RAW) &&
         std::filesystem::exists(path / K_IIO_MAGN_Y_RAW) &&
         std::filesystem::exists(path / K_IIO_MAGN_Z_RAW);
}

bool read_iio_display_name(const std::filesystem::path& path,
                           const char* fallback,
                           std::string& display_name) {
  if (!read_text_file(path / "name", display_name) || display_name.empty()) {
    display_name = fallback && fallback[0] != '\0' ? fallback : path.filename().string();
  }
  return true;
}

bool find_iio_node(const char* preferred_path,
                   bool (*matches)(const std::filesystem::path&),
                   const char* fallback_name,
                   std::string& iio_path,
                   std::string& display_name) {
  if (preferred_path && preferred_path[0] != '\0') {
    const std::filesystem::path preferred(preferred_path);
    if (std::filesystem::exists(preferred) && matches(preferred)) {
      iio_path = preferred.string();
      read_iio_display_name(preferred, fallback_name, display_name);
      return true;
    }
  }

  const std::filesystem::path root(K_BMI270_IIO_SYSFS_ROOT);
  if (!std::filesystem::exists(root)) {
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(root)) {
    if (!entry.is_directory() && !entry.is_symlink()) {
      continue;
    }
    if (!matches(entry.path())) {
      continue;
    }

    iio_path = entry.path().string();
    read_iio_display_name(entry.path(), fallback_name, display_name);
    return true;
  }
  return false;
}

bool read_scaled_axis(const std::filesystem::path& root,
                      const char* raw_file,
                      double scale,
                      double& value) {
  double raw = 0.0;
  if (!read_double_file(root / raw_file, raw)) {
    return false;
  }
  value = raw * scale;
  return true;
}

}  // namespace

bool find_bmi270_device(ImuDevice& device, std::string& error_message) {
  find_i2c_node(device.i2c_path);

  if (!find_iio_node(K_BMI270_IIO_DEVICE_PATH,
                     is_iio_bmi270_node,
                     K_BMI270_DEVICE_NAME,
                     device.iio_path,
                     device.display_name)) {
    error_message = "BMI270 IIO device not found. Check that the bmi270 kernel driver is loaded.";
    return false;
  }

  if (!find_iio_node(K_BMM150_IIO_DEVICE_PATH,
                     is_iio_bmm150_node,
                     K_BMM150_DEVICE_NAME,
                     device.mag_iio_path,
                     device.mag_display_name)) {
    error_message = "BMM150 IIO device not found. Check that the bmm150 kernel driver is loaded.";
    return false;
  }

  if (device.display_name.empty()) {
    device.display_name = K_BMI270_DEVICE_NAME;
  }
  if (device.mag_display_name.empty()) {
    device.mag_display_name = K_BMM150_DEVICE_NAME;
  }
  return true;
}

bool read_six_axis(const ImuDevice& device, SixAxisReading& reading, std::string& error_message) {
  const std::filesystem::path root(device.iio_path);
  double accel_scale = 0.0;
  double gyro_scale  = 0.0;
  if (!read_double_file(root / K_IIO_ACCEL_SCALE, accel_scale)) {
    error_message = "Failed to read BMI270 accel scale.";
    return false;
  }
  if (!read_double_file(root / K_IIO_GYRO_SCALE, gyro_scale)) {
    error_message = "Failed to read BMI270 gyro scale.";
    return false;
  }

  if (!read_scaled_axis(root, K_IIO_ACCEL_X_RAW, accel_scale, reading.accel_x) ||
      !read_scaled_axis(root, K_IIO_ACCEL_Y_RAW, accel_scale, reading.accel_y) ||
      !read_scaled_axis(root, K_IIO_ACCEL_Z_RAW, accel_scale, reading.accel_z) ||
      !read_scaled_axis(root, K_IIO_GYRO_X_RAW, gyro_scale, reading.gyro_x) ||
      !read_scaled_axis(root, K_IIO_GYRO_Y_RAW, gyro_scale, reading.gyro_y) ||
      !read_scaled_axis(root, K_IIO_GYRO_Z_RAW, gyro_scale, reading.gyro_z)) {
    error_message = "Failed to read BMI270 six-axis data.";
    return false;
  }

  return true;
}

bool read_nine_axis(const ImuDevice& device, NineAxisReading& reading, std::string& error_message) {
  const std::filesystem::path imu_root(device.iio_path);
  const std::filesystem::path mag_root(device.mag_iio_path);
  double accel_scale = 0.0;
  double gyro_scale  = 0.0;
  double magn_scale  = 0.0;
  if (!read_double_file(imu_root / K_IIO_ACCEL_SCALE, accel_scale)) {
    error_message = "Failed to read BMI270 accel scale.";
    return false;
  }
  if (!read_double_file(imu_root / K_IIO_GYRO_SCALE, gyro_scale)) {
    error_message = "Failed to read BMI270 gyro scale.";
    return false;
  }
  if (!read_double_file(mag_root / K_IIO_MAGN_SCALE, magn_scale)) {
    error_message = "Failed to read BMM150 magnetometer scale.";
    return false;
  }

  if (!read_scaled_axis(imu_root, K_IIO_ACCEL_X_RAW, accel_scale, reading.accel_x) ||
      !read_scaled_axis(imu_root, K_IIO_ACCEL_Y_RAW, accel_scale, reading.accel_y) ||
      !read_scaled_axis(imu_root, K_IIO_ACCEL_Z_RAW, accel_scale, reading.accel_z) ||
      !read_scaled_axis(imu_root, K_IIO_GYRO_X_RAW, gyro_scale, reading.gyro_x) ||
      !read_scaled_axis(imu_root, K_IIO_GYRO_Y_RAW, gyro_scale, reading.gyro_y) ||
      !read_scaled_axis(imu_root, K_IIO_GYRO_Z_RAW, gyro_scale, reading.gyro_z) ||
      !read_scaled_axis(mag_root, K_IIO_MAGN_X_RAW, magn_scale, reading.magn_x) ||
      !read_scaled_axis(mag_root, K_IIO_MAGN_Y_RAW, magn_scale, reading.magn_y) ||
      !read_scaled_axis(mag_root, K_IIO_MAGN_Z_RAW, magn_scale, reading.magn_z)) {
    error_message = "Failed to read BMI270/BMM150 nine-axis data.";
    return false;
  }

  return true;
}

}  // namespace platform::imu
