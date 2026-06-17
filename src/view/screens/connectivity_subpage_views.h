/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <vector>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "connectivity_test_viewmodel.h"
#include "icon_card.h"
#include "lvgl.h"

namespace screen {

class WifiConnectivityView {
 public:
  explicit WifiConnectivityView(viewmodel::WifiConnectivityViewModel& view_model);
  ~WifiConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::WifiConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* panel_{nullptr};
  bool panel_initialized_{false};
  lv_timer_t* refresh_timer_{nullptr};
};

class BluetoothConnectivityView {
 public:
  explicit BluetoothConnectivityView(viewmodel::BluetoothConnectivityViewModel& view_model);
  ~BluetoothConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::BluetoothConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* panel_{nullptr};
  bool panel_initialized_{false};
  lv_timer_t* refresh_timer_{nullptr};
};

class EthernetConnectivityView {
 public:
  explicit EthernetConnectivityView(viewmodel::EthernetConnectivityViewModel& view_model);
  ~EthernetConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::EthernetConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* list_{nullptr};
  std::vector<std::unique_ptr<view::widgets::IconCard>> cards_{};
  lv_timer_t* refresh_timer_{nullptr};
};

class UsbConnectivityView {
 public:
  explicit UsbConnectivityView(viewmodel::UsbConnectivityViewModel& view_model);
  ~UsbConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::UsbConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* list_{nullptr};
  std::vector<std::unique_ptr<view::widgets::IconCard>> cards_{};
  lv_timer_t* refresh_timer_{nullptr};
};

class I2cConnectivityView {
 public:
  explicit I2cConnectivityView(viewmodel::I2cConnectivityViewModel& view_model);
  ~I2cConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::I2cConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* panel_{nullptr};
  bool panel_initialized_{false};
  lv_timer_t* refresh_timer_{nullptr};
};

class SpiConnectivityView {
 public:
  explicit SpiConnectivityView(viewmodel::SpiConnectivityViewModel& view_model);
  ~SpiConnectivityView();
  void build(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model, app::AssetManager& assets);

 private:
  void refresh_();
  static void refresh_timer_cb(lv_timer_t* timer);

  viewmodel::SpiConnectivityViewModel& view_model_;
  viewmodel::AppViewModel* app_view_model_{nullptr};
  app::AssetManager* assets_{nullptr};
  lv_obj_t* list_{nullptr};
  std::vector<std::unique_ptr<view::widgets::IconCard>> cards_{};
  lv_timer_t* refresh_timer_{nullptr};
};

}  // namespace screen
