/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace platform::power {

struct PowerSupplyInfo {
  std::string device_name;
  std::string type;
  std::string status;
  std::string health;
  std::string technology;
  std::string manufacturer;
  std::string capacity_level;
  int32_t capacity_percent{-1};
  int32_t present{-1};
  int32_t cycle_count{-1};
  int64_t voltage_now_uv{-1};
  int64_t voltage_instant_uv{-1};
  int64_t current_now_ua{-1};
  int64_t current_instant_ua{-1};
  int64_t charge_now_uah{-1};
  int64_t charge_full_uah{-1};
  int64_t charge_full_design_uah{-1};
  int64_t power_avg_uw{-1};
  int32_t temp_decic{-100000};
};

bool read_battery_info(PowerSupplyInfo& info, std::string& error_message);

}  // namespace platform::power
