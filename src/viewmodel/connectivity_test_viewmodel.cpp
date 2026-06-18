/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_test_viewmodel.h"

#include <utility>

namespace viewmodel {
namespace {

int page_to_int(model::ConnectivitySubPage page) { return static_cast<int>(page); }

}  // namespace

const char* WifiConnectivityViewModel::title_text() const { return model_.title(); }

bool WifiConnectivityViewModel::refresh(bool force_refresh) {
  return model_.refresh(force_refresh);
}

const std::vector<model::ConnectivityScanInfo>& WifiConnectivityViewModel::scan_items() const {
  return model_.scan_items();
}

const std::string& WifiConnectivityViewModel::error_message() const {
  return model_.error_message();
}

const char* BluetoothConnectivityViewModel::title_text() const { return model_.title(); }

bool BluetoothConnectivityViewModel::refresh(bool force_refresh) {
  return model_.refresh(force_refresh);
}

const std::vector<model::ConnectivityScanInfo>& BluetoothConnectivityViewModel::scan_items() const {
  return model_.scan_items();
}

const std::string& BluetoothConnectivityViewModel::error_message() const {
  return model_.error_message();
}

const char* EthernetConnectivityViewModel::title_text() const { return model_.title(); }

bool EthernetConnectivityViewModel::refresh(bool force_refresh) {
  return model_.refresh(force_refresh);
}

const std::vector<model::ConnectivityInfoField>& EthernetConnectivityViewModel::fields() const {
  return model_.fields();
}

const std::string& EthernetConnectivityViewModel::error_message() const {
  return model_.error_message();
}

const char* UsbConnectivityViewModel::title_text() const { return model_.title(); }

bool UsbConnectivityViewModel::refresh(bool force_refresh) { return model_.refresh(force_refresh); }

const std::vector<model::ConnectivityScanInfo>& UsbConnectivityViewModel::devices() const {
  return model_.devices();
}

const std::string& UsbConnectivityViewModel::error_message() const {
  return model_.error_message();
}

const char* I2cConnectivityViewModel::title_text() const { return model_.title(); }

bool I2cConnectivityViewModel::refresh(bool force_refresh) { return model_.refresh(force_refresh); }

const std::vector<model::ConnectivityI2cAddressInfo>& I2cConnectivityViewModel::addresses() const {
  return model_.addresses();
}

const std::string& I2cConnectivityViewModel::error_message() const {
  return model_.error_message();
}

const char* SpiConnectivityViewModel::title_text() const { return model_.title(); }

bool SpiConnectivityViewModel::refresh(bool force_refresh) { return model_.refresh(force_refresh); }

const std::vector<model::ConnectivityScanInfo>& SpiConnectivityViewModel::devices() const {
  return model_.devices();
}

const std::string& SpiConnectivityViewModel::error_message() const {
  return model_.error_message();
}

const char* LinkConnectivityViewModel::title_text() const { return model_.title(); }

bool LinkConnectivityViewModel::refresh(bool force_refresh) {
  return model_.refresh(force_refresh);
}

const model::LinkTestSnapshot& LinkConnectivityViewModel::snapshot() const {
  return model_.snapshot();
}

const model::LinkTestSettings& LinkConnectivityViewModel::settings() const {
  return model_.settings();
}

void LinkConnectivityViewModel::set_settings(model::LinkTestSettings settings) {
  model_.set_settings(std::move(settings));
}

ConnectivityTestViewModel::ConnectivityTestViewModel()
    : selected_index_subject_(static_cast<int32_t>(model_.selected_index())),
      active_page_subject_(page_to_int(model_.active_page())) {}

ConnectivityTestViewModel::~ConnectivityTestViewModel() = default;

lv_subject_t* ConnectivityTestViewModel::selected_index_subject() {
  return selected_index_subject_.native();
}

lv_subject_t* ConnectivityTestViewModel::active_page_subject() {
  return active_page_subject_.native();
}

lv_subject_t* ConnectivityTestViewModel::link_restart_request_subject() {
  return link_restart_request_subject_.native();
}

lv_subject_t* ConnectivityTestViewModel::link_settings_request_subject() {
  return link_settings_request_subject_.native();
}

const std::array<model::ConnectivityMenuItem, model::ConnectivityTestModel::K_ITEM_COUNT>&
ConnectivityTestViewModel::items() const {
  return model_.items();
}

std::size_t ConnectivityTestViewModel::selected_index() const { return model_.selected_index(); }

model::ConnectivitySubPage ConnectivityTestViewModel::active_page() const {
  return model_.active_page();
}

bool ConnectivityTestViewModel::is_menu_active() const {
  return model_.active_page() == model::ConnectivitySubPage::MENU;
}

void ConnectivityTestViewModel::select_previous() {
  if (!is_menu_active()) {
    return;
  }

  model_.select_previous();
  publish_all_();
}

void ConnectivityTestViewModel::select_next() {
  if (!is_menu_active()) {
    return;
  }

  model_.select_next();
  publish_all_();
}

void ConnectivityTestViewModel::set_selected_index(std::size_t index) {
  model_.set_selected_index(index);
  publish_all_();
}

void ConnectivityTestViewModel::activate_selected() {
  if (!is_menu_active()) {
    return;
  }

  model_.activate_selected();
  publish_all_();
}

void ConnectivityTestViewModel::show_menu() {
  model_.show_menu();
  publish_all_();
}

bool ConnectivityTestViewModel::request_back() {
  if (is_menu_active()) {
    return false;
  }

  show_menu();
  return true;
}

bool ConnectivityTestViewModel::refresh_active() {
  switch (model_.active_page()) {
    case model::ConnectivitySubPage::WIFI:
      return wifi_view_model_.refresh(true);
    case model::ConnectivitySubPage::BLUETOOTH:
      return bluetooth_view_model_.refresh(true);
    case model::ConnectivitySubPage::ETHERNET:
      return ethernet_view_model_.refresh(true);
    case model::ConnectivitySubPage::USB:
      return usb_view_model_.refresh(true);
    case model::ConnectivitySubPage::I2C:
      return i2c_view_model_.refresh(true);
    case model::ConnectivitySubPage::SPI:
      return spi_view_model_.refresh(true);
    case model::ConnectivitySubPage::LINK_TEST:
      return link_view_model_.refresh(true);
    case model::ConnectivitySubPage::MENU:
    default:
      return false;
  }
}

void ConnectivityTestViewModel::request_link_restart() {
  link_restart_request_subject_.set(++link_restart_request_count_);
}

void ConnectivityTestViewModel::request_link_settings() {
  link_settings_request_subject_.set(++link_settings_request_count_);
}

WifiConnectivityViewModel& ConnectivityTestViewModel::wifi_view_model() { return wifi_view_model_; }

BluetoothConnectivityViewModel& ConnectivityTestViewModel::bluetooth_view_model() {
  return bluetooth_view_model_;
}

EthernetConnectivityViewModel& ConnectivityTestViewModel::ethernet_view_model() {
  return ethernet_view_model_;
}

UsbConnectivityViewModel& ConnectivityTestViewModel::usb_view_model() { return usb_view_model_; }

I2cConnectivityViewModel& ConnectivityTestViewModel::i2c_view_model() { return i2c_view_model_; }

SpiConnectivityViewModel& ConnectivityTestViewModel::spi_view_model() { return spi_view_model_; }

LinkConnectivityViewModel& ConnectivityTestViewModel::link_view_model() { return link_view_model_; }

void ConnectivityTestViewModel::publish_all_() {
  selected_index_subject_.set(static_cast<int32_t>(model_.selected_index()));
  active_page_subject_.set(page_to_int(model_.active_page()));
}

}  // namespace viewmodel
