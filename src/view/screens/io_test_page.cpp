/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "io_test_page.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "asset_manager.h"
#include "bindings.h"
#include "gpio_service.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_SCROLL_STEP       = 34;
constexpr uint32_t K_LOADING_MODAL_MS = 900;

model::ConnectivitySubPage connectivity_page_for_app_page(model::AppPage page) {
  switch (page) {
    case model::AppPage::WIFI_TEST:
      return model::ConnectivitySubPage::WIFI;
    case model::AppPage::BT_TEST:
      return model::ConnectivitySubPage::BLUETOOTH;
    case model::AppPage::ETH_TEST:
      return model::ConnectivitySubPage::ETHERNET;
    case model::AppPage::USB_TEST:
      return model::ConnectivitySubPage::USB;
    case model::AppPage::HDMI_TEST:
      return model::ConnectivitySubPage::HDMI;
    case model::AppPage::I2C_TEST:
      return model::ConnectivitySubPage::I2C;
    case model::AppPage::SPI_TEST:
      return model::ConnectivitySubPage::SPI;
    case model::AppPage::UART_TEST:
      return model::ConnectivitySubPage::UART;
    case model::AppPage::EXT_IO_TEST:
      return model::ConnectivitySubPage::EXT_IO;
    case model::AppPage::LINK_TEST:
      return model::ConnectivitySubPage::LINK_TEST;
    default:
      return model::ConnectivitySubPage::MENU;
  }
}

const char* loading_title(model::AppPage page) {
  switch (page) {
    case model::AppPage::I2C_TEST:
      return nullptr;
    case model::AppPage::SPI_TEST:
      return "Scanning SPI...";
    case model::AppPage::LINK_TEST:
      return "Testing Link...";
    case model::AppPage::UART_TEST:
      return "Opening UART...";
    case model::AppPage::ETH_TEST:
      return "Reading Ethernet...";
    case model::AppPage::HDMI_TEST:
      return "Reading HDMI...";
    case model::AppPage::EXT_IO_TEST:
      return nullptr;
    default:
      return "Scanning...";
  }
}

}  // namespace

IoTestPage::IoTestPage(viewmodel::AppViewModel& app_view_model,
                       viewmodel::ConnectivityTestViewModel& connectivity_view_model,
                       app::AssetManager& assets,
                       model::AppPage page)
    : BaseScreen(app_view_model, assets),
      connectivity_view_model_(connectivity_view_model),
      page_(page) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  connectivity_view_model_.show_subpage(connectivity_page_for_app_page(page_), true);
  update_nav_actions_();
  init();
  platform::set_key_listener(key_listener, this);
  platform::set_key_release_listener(key_release_listener, this);
  platform::set_long_key_listener(long_key_listener, this);
}

IoTestPage::~IoTestPage() {
  platform::clear_long_key_listener(long_key_listener, this);
  platform::clear_key_release_listener(key_release_listener, this);
  platform::clear_key_listener(key_listener, this);
  if (loading_modal_timer_) {
    lv_timer_delete(loading_modal_timer_);
    loading_modal_timer_ = nullptr;
  }
  hide_loading_modal_();
  confirm_hold_popup_.reset();
  link_view_.reset();
  ext_io_view_.reset();
  uart_view_.reset();
  spi_view_.reset();
  i2c_view_.reset();
  hdmi_view_.reset();
  usb_view_.reset();
  ethernet_view_.reset();
  bluetooth_view_.reset();
  wifi_view_.reset();
  connectivity_view_model_.clear_direct_subpage();
}

void IoTestPage::build_content(lv_obj_t* content) {
  viewport_ = lv_obj_create(content);
  lv_obj_remove_style_all(viewport_);
  lv_obj_set_size(viewport_, LV_PCT(100), LV_PCT(100));
  lv_obj_align(viewport_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(viewport_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(viewport_, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(viewport_, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_all(viewport_, 0, 0);
  reactive::bind_theme(viewport_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  switch (page_) {
    case model::AppPage::WIFI_TEST:
      wifi_view_ = std::make_unique<WifiConnectivityView>(connectivity_view_model_.wifi_view_model());
      wifi_view_->build(viewport_, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::BT_TEST:
      bluetooth_view_ =
          std::make_unique<BluetoothConnectivityView>(connectivity_view_model_.bluetooth_view_model());
      bluetooth_view_->build(viewport_, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::ETH_TEST:
      ethernet_view_ =
          std::make_unique<EthernetConnectivityView>(connectivity_view_model_.ethernet_view_model());
      ethernet_view_->build(viewport_, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::USB_TEST:
      usb_view_ = std::make_unique<UsbConnectivityView>(connectivity_view_model_.usb_view_model());
      usb_view_->build(viewport_, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::HDMI_TEST:
      hdmi_view_ = std::make_unique<HdmiConnectivityView>(connectivity_view_model_.hdmi_view_model());
      hdmi_view_->build(viewport_, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::I2C_TEST:
      {
        std::string error;
        platform::gpio::set_external_bus_i2c_mode(true, error);
      }
      i2c_view_ = std::make_unique<I2cConnectivityView>(connectivity_view_model_.i2c_view_model());
      i2c_view_->build(viewport_, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::SPI_TEST:
      spi_view_ = std::make_unique<SpiConnectivityView>(connectivity_view_model_.spi_view_model());
      spi_view_->build(viewport_, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::UART_TEST:
      {
        std::string error;
        platform::gpio::set_external_bus_uart_mode(error);
      }
      uart_view_ = std::make_unique<UartConnectivityView>();
      uart_view_->build(viewport_, root(), app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::EXT_IO_TEST:
      ext_io_view_ = std::make_unique<ExtIoConnectivityView>();
      ext_io_view_->build(viewport_, root(), app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::LINK_TEST:
      link_view_ =
          std::make_unique<LinkConnectivityView>(connectivity_view_model_.link_view_model());
      link_view_->build(viewport_, root(), app_view_model_ref_(), assets_ref_());
      break;
    default:
      break;
  }

  show_loading_modal_();
}

void IoTestPage::update_nav_actions_() {
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_(
      '4',
      view::ICON_ARROW_U_UP_LEFT,
      [this]() { app_view_model_ref_().request_back_or_quit(); },
      is_uart_page_() ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED);

  if (is_link_page_()) {
    set_nav_action_('5', view::ICON_ARROWS_CLOCKWISE, [this]() {
      if (link_view_) {
        link_view_->restart();
      }
    });
    set_nav_action_('7', view::ICON_GEAR_FINE, [this]() {
      if (link_view_) {
        link_view_->show_config_dialog();
      }
    });
  } else if (is_uart_page_()) {
    set_nav_action_(
        '6',
        view::ICON_GEAR_FINE,
        [this]() {
          if (uart_view_) {
            uart_view_->show_config_dialog();
          }
        },
        LV_EVENT_LONG_PRESSED);
  } else if (is_ext_io_page_()) {
    set_nav_action_('6', view::ICON_GEAR_FINE, [this]() {
      if (ext_io_view_) {
        ext_io_view_->show_config_dialog();
      }
    });
  }

  if (is_uart_page_()) {
    set_nav_action_(
        '8',
        view::ICON_CHECK_SQUARE,
        [this]() {
          hide_confirm_hold_popup_();
          show_test_result_dialog_();
        },
        LV_EVENT_LONG_PRESSED,
        viewmodel::NavHoldTarget::SUCCESS,
        false,
        [this]() { show_confirm_hold_popup_(); },
        [this]() { hide_confirm_hold_popup_(); });
  } else {
    set_nav_action_('8', view::ICON_CHECK_SQUARE, [this]() { show_test_result_dialog_(); });
  }
}

void IoTestPage::scroll_viewport_(int32_t direction) {
  if (!viewport_ || direction == 0) {
    return;
  }

  lv_obj_update_layout(viewport_);
  const int32_t current_y = lv_obj_get_scroll_y(viewport_);
  const int32_t max_y     = current_y + lv_obj_get_scroll_bottom(viewport_);
  if (max_y <= 0) {
    return;
  }

  const int32_t target_y = std::clamp(current_y + direction * K_SCROLL_STEP, 0, max_y);
  lv_obj_scroll_to_y(viewport_, target_y, LV_ANIM_ON);
}

void IoTestPage::show_confirm_hold_popup_() {
  if (!root()) {
    return;
  }
  if (!confirm_hold_popup_) {
    view::widgets::PopupConfig config;
    config.width       = 250;
    config.label_width = 234;
    config.message     = "Hold 8 to confirm test";
    config.tone        = view::widgets::PopupTone::SUCCESS;
    confirm_hold_popup_ =
        std::make_unique<view::widgets::Popup>(root(), app_view_model_ref_(), config);
    confirm_hold_popup_->build();
  }
  confirm_hold_popup_->show();
}

void IoTestPage::hide_confirm_hold_popup_() {
  if (confirm_hold_popup_) {
    confirm_hold_popup_->hide();
  }
}

void IoTestPage::show_loading_modal_() {
  const char* title = loading_title(page_);
  if (!root() || !title) {
    return;
  }

  hide_loading_modal_();
  loading_modal_ = lv_obj_create(root());
  lv_obj_remove_style_all(loading_modal_);
  lv_obj_set_size(loading_modal_, 176, 52);
  lv_obj_align(loading_modal_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(loading_modal_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(loading_modal_, LV_OBJ_FLAG_CLICKABLE);
  reactive::bind_theme(loading_modal_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::BUTTON);

  auto* label = lv_label_create(loading_modal_);
  lv_label_set_text(label, title);
  auto* font = assets_ref_().load_font("inter-semibold.ttf", 12);
  lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, 0);
  reactive::bind_theme(label, app_view_model_ref_().dark_mode_subject(), reactive::ThemeRole::TEXT);
  lv_obj_center(label);
  lv_obj_move_foreground(loading_modal_);

  if (!loading_modal_timer_) {
    loading_modal_timer_ = lv_timer_create(loading_modal_timer_cb, K_LOADING_MODAL_MS, this);
    lv_timer_set_auto_delete(loading_modal_timer_, false);
  }
  lv_timer_set_repeat_count(loading_modal_timer_, 1);
  lv_timer_resume(loading_modal_timer_);
  lv_timer_reset(loading_modal_timer_);
}

void IoTestPage::hide_loading_modal_() {
  if (loading_modal_ && lv_obj_is_valid(loading_modal_)) {
    lv_obj_delete(loading_modal_);
  }
  loading_modal_ = nullptr;
}

bool IoTestPage::is_uart_page_() const { return page_ == model::AppPage::UART_TEST; }

bool IoTestPage::is_ext_io_page_() const { return page_ == model::AppPage::EXT_IO_TEST; }

bool IoTestPage::is_link_page_() const { return page_ == model::AppPage::LINK_TEST; }

void IoTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<IoTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  if (page->is_uart_page_() && page->uart_view_ &&
      page->uart_view_->handle_key(key, key_name)) {
    return;
  }

  if (page->is_link_page_() && page->link_view_) {
    if (page->link_view_->handle_key(key, key_name)) {
      return;
    }
    if (key == '5' || key == 'r' || key == 'R') {
      page->link_view_->restart();
      return;
    }
    if (key == '7' || key == 's' || key == 'S') {
      page->link_view_->show_config_dialog();
      return;
    }
  }

  if (page->is_ext_io_page_() && page->ext_io_view_ &&
      page->ext_io_view_->handle_key(key, key_name)) {
    return;
  }

  switch (key) {
    case LV_KEY_UP:
    case LV_KEY_LEFT:
    case '5':
    case 'f':
    case 'F':
      page->scroll_viewport_(-1);
      break;
    case LV_KEY_DOWN:
    case LV_KEY_RIGHT:
    case '7':
    case 'x':
    case 'X':
      page->scroll_viewport_(1);
      break;
    case 'r':
    case 'R':
      page->connectivity_view_model_.refresh_active();
      break;
    default:
      break;
  }
}

void IoTestPage::key_release_listener(uint32_t key,
                                      const char* key_name,
                                      void* user_data) {
  auto* page = static_cast<IoTestPage*>(user_data);
  if (page && page->is_uart_page_() && page->uart_view_) {
    page->uart_view_->handle_key_release(key, key_name);
  }
}

void IoTestPage::long_key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<IoTestPage*>(user_data);
  if (!page || !page->is_uart_page_() || !page->uart_view_) {
    return;
  }
  if (page->uart_view_->handle_long_key(key, key_name)) {
    return;
  }
  if (key == LV_KEY_ESC || key == 27) {
    page->app_view_model_ref_().request_back_or_quit();
  }
}

void IoTestPage::loading_modal_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<IoTestPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->hide_loading_modal_();
  }
}

}  // namespace screen
