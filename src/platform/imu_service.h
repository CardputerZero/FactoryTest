/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform::imu {

inline constexpr const char* K_BMI270_I2C_SYSFS_ROOT     = "/sys/bus/i2c/devices";
inline constexpr const char* K_BMI270_IIO_SYSFS_ROOT     = "/sys/bus/iio/devices";
inline constexpr const char* K_BMI270_COMPATIBLE         = "bosch,bmi270";
inline constexpr const char* K_BMI270_DEVICE_NAME        = "bmi270";
inline constexpr const char* K_BMI270_I2C_ADDRESS_SUFFIX = "-0068";
inline constexpr const char* K_BMI270_IIO_DEVICE_PATH    = "/sys/bus/iio/devices/iio:device0";
inline constexpr const char* K_BMM150_IIO_DEVICE_PATH    = "/sys/bus/iio/devices/iio:device2";
inline constexpr const char* K_BMM150_COMPATIBLE         = "bosch,bmm150";
inline constexpr const char* K_BMM150_DEVICE_NAME        = "bmm150";
inline constexpr const char* K_IIO_ACCEL_X_RAW           = "in_accel_x_raw";
inline constexpr const char* K_IIO_ACCEL_Y_RAW           = "in_accel_y_raw";
inline constexpr const char* K_IIO_ACCEL_Z_RAW           = "in_accel_z_raw";
inline constexpr const char* K_IIO_ACCEL_SCALE           = "in_accel_scale";
inline constexpr const char* K_IIO_GYRO_X_RAW            = "in_anglvel_x_raw";
inline constexpr const char* K_IIO_GYRO_Y_RAW            = "in_anglvel_y_raw";
inline constexpr const char* K_IIO_GYRO_Z_RAW            = "in_anglvel_z_raw";
inline constexpr const char* K_IIO_GYRO_SCALE            = "in_anglvel_scale";
inline constexpr const char* K_IIO_MAGN_X_RAW            = "in_magn_x_raw";
inline constexpr const char* K_IIO_MAGN_Y_RAW            = "in_magn_y_raw";
inline constexpr const char* K_IIO_MAGN_Z_RAW            = "in_magn_z_raw";
inline constexpr const char* K_IIO_MAGN_SCALE            = "in_magn_scale";

struct ImuDevice {
  std::string i2c_path;
  std::string iio_path;
  std::string mag_iio_path;
  std::string display_name;
  std::string mag_display_name;
};

struct SixAxisReading {
  double accel_x{0.0};
  double accel_y{0.0};
  double accel_z{0.0};
  double gyro_x{0.0};
  double gyro_y{0.0};
  double gyro_z{0.0};
};

struct NineAxisReading {
  double accel_x{0.0};
  double accel_y{0.0};
  double accel_z{0.0};
  double gyro_x{0.0};
  double gyro_y{0.0};
  double gyro_z{0.0};
  double magn_x{0.0};
  double magn_y{0.0};
  double magn_z{0.0};
};

bool find_bmi270_device(ImuDevice& device, std::string& error_message);
bool read_six_axis(const ImuDevice& device, SixAxisReading& reading, std::string& error_message);
bool read_nine_axis(const ImuDevice& device, NineAxisReading& reading, std::string& error_message);

}  // namespace platform::imu
