/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

namespace platform::device_info {

enum class ProductModel {
  CARDPUTER_ZERO,
  CARDPUTER_ZERO_LITE,
};

struct DeviceInfoField {
  std::string label;
  std::string value;
};

ProductModel product_model();
const char* product_model_name(ProductModel model);
std::vector<DeviceInfoField> read_device_info_fields();

}  // namespace platform::device_info
