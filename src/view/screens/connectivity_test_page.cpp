/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_test_page.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "asset_manager.h"
#include "bindings.h"
#include "gpio_service.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_VIEWPORT_WIDTH             = view::K_SCREEN_WIDTH;
constexpr int32_t K_VIEWPORT_HEIGHT            = 106;
constexpr int32_t K_MENU_WIDTH                 = 300;
constexpr int32_t K_MENU_HEIGHT                = 106;
constexpr int32_t K_SCROLL_STEP                = 34;
constexpr uint32_t K_LOADING_MODAL_MS          = 900;
constexpr uint32_t K_UART_SETTINGS_SUPPRESS_MS = 300;

const char* icon_for_page(model::ConnectivitySubPage page) {
  switch (page) {
    case model::ConnectivitySubPage::WIFI:
      return view::ICON_WIFI;
    case model::ConnectivitySubPage::BLUETOOTH:
      return view::ICON_BLUETOOTH;
    case model::ConnectivitySubPage::ETHERNET:
      return view::ICON_ETHERNET;
    case model::ConnectivitySubPage::USB:
      return view::ICON_USB;
    case model::ConnectivitySubPage::HDMI:
      return view::ICON_MONITOR;
    case model::ConnectivitySubPage::I2C:
      return view::ICON_SCAN;
    case model::ConnectivitySubPage::SPI:
      return view::ICON_LINK_SIMPLE_HOR;
    case model::ConnectivitySubPage::UART:
      return view::ICON_BROADCAST;
    case model::ConnectivitySubPage::EXT_IO:
      return view::ICON_PLUGS_CONNECTED;
    case model::ConnectivitySubPage::LINK_TEST:
      return view::ICON_GLOBE;
    case model::ConnectivitySubPage::MENU:
    default:
      return "";
  }
}

const char* title_for_page(model::ConnectivitySubPage page) {
  switch (page) {
    case model::ConnectivitySubPage::WIFI:
      return "Wi-Fi Scan";
    case model::ConnectivitySubPage::BLUETOOTH:
      return "Bluetooth Scan";
    case model::ConnectivitySubPage::ETHERNET:
      return "Ethernet Info";
    case model::ConnectivitySubPage::USB:
      return "USB Devices";
    case model::ConnectivitySubPage::HDMI:
      return "HDMI Info";
    case model::ConnectivitySubPage::I2C:
      return "I2C Scan";
    case model::ConnectivitySubPage::SPI:
      return "SPI Scan";
    case model::ConnectivitySubPage::UART:
      return "UART Console";
    case model::ConnectivitySubPage::EXT_IO:
      return "EXT.IO";
    case model::ConnectivitySubPage::LINK_TEST:
      return "Network Link Test";
    case model::ConnectivitySubPage::MENU:
    default:
      return "Connectivity Test";
  }
}

}  // namespace

ConnectivityTestPage::ConnectivityTestPage(
    viewmodel::AppViewModel& app_view_model,
    viewmodel::ConnectivityTestViewModel& connectivity_view_model,
    app::AssetManager& assets)
    : BaseScreen(app_view_model, assets),
      connectivity_view_model_(connectivity_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  if (!connectivity_view_model_.is_direct_subpage_active()) {
    connectivity_view_model_.show_menu();
  }
  set_default_test_nav_();
  app_view_model_ref_().set_back_request_handler(back_request_handler, this);
  init();
  platform::set_key_listener(key_listener, this);
  platform::set_key_release_listener(key_release_listener, this);
  platform::set_long_key_listener(long_key_listener, this);
}

ConnectivityTestPage::~ConnectivityTestPage() {
  app_view_model_ref_().clear_back_request_handler(back_request_handler, this);
  platform::clear_long_key_listener(long_key_listener, this);
  platform::clear_key_release_listener(key_release_listener, this);
  platform::clear_key_listener(key_listener, this);
  if (selected_observer_handle_) {
    lv_observer_remove(selected_observer_handle_);
    selected_observer_handle_ = nullptr;
  }
  if (active_page_observer_handle_) {
    lv_observer_remove(active_page_observer_handle_);
    active_page_observer_handle_ = nullptr;
  }
  if (link_restart_observer_handle_) {
    lv_observer_remove(link_restart_observer_handle_);
    link_restart_observer_handle_ = nullptr;
  }
  if (link_settings_observer_handle_) {
    lv_observer_remove(link_settings_observer_handle_);
    link_settings_observer_handle_ = nullptr;
  }
  if (uart_settings_observer_handle_) {
    lv_observer_remove(uart_settings_observer_handle_);
    uart_settings_observer_handle_ = nullptr;
  }
  if (loading_modal_timer_) {
    lv_timer_delete(loading_modal_timer_);
    loading_modal_timer_ = nullptr;
  }
  hide_loading_modal_();
  menu_list_.reset();
  wifi_view_.reset();
  bluetooth_view_.reset();
  ethernet_view_.reset();
  usb_view_.reset();
  hdmi_view_.reset();
  i2c_view_.reset();
  spi_view_.reset();
  uart_view_.reset();
  ext_io_view_.reset();
  link_view_.reset();
  connectivity_view_model_.clear_direct_subpage();
}

void ConnectivityTestPage::build_content(lv_obj_t* content) {
  const auto& items = connectivity_view_model_.items();

  plane_ = lv_obj_create(content);
  lv_obj_remove_style_all(plane_);
  lv_obj_set_size(plane_,
                  K_VIEWPORT_WIDTH * static_cast<int32_t>(items.size() + 1),
                  K_VIEWPORT_HEIGHT);
  lv_obj_align(plane_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(plane_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(plane_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  auto* text_font = assets_ref_().load_font("inter-medium.ttf", 14);
  auto* icon_font = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  std::vector<view::widgets::IconList::Item> list_items;
  list_items.reserve(items.size());
  for (const auto& item : items) {
    list_items.push_back({icon_for_page(item.target_page), item.title});
  }

  auto* menu_viewport = lv_obj_create(plane_);
  lv_obj_remove_style_all(menu_viewport);
  lv_obj_set_size(menu_viewport, K_VIEWPORT_WIDTH, K_VIEWPORT_HEIGHT);
  lv_obj_align(menu_viewport, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(menu_viewport, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(menu_viewport,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  menu_list_ =
      std::make_unique<view::widgets::IconList>(menu_viewport,
                                                app_view_model_ref_(),
                                                list_items,
                                                text_font ? text_font : &lv_font_montserrat_14,
                                                icon_font ? icon_font : &lv_font_montserrat_14,
                                                K_MENU_WIDTH,
                                                K_MENU_HEIGHT,
                                                menu_item_clicked,
                                                this);
  menu_list_->build();
  menu_list_->set_selected_index(connectivity_view_model_.selected_index());
  menu_list_->set_focused(connectivity_view_model_.is_menu_active());

  for (std::size_t i = 0; i < items.size(); ++i) {
    auto* viewport = lv_obj_create(plane_);
    lv_obj_remove_style_all(viewport);
    lv_obj_set_size(viewport, K_VIEWPORT_WIDTH, K_VIEWPORT_HEIGHT);
    lv_obj_align(viewport, LV_ALIGN_TOP_LEFT, K_VIEWPORT_WIDTH * static_cast<int32_t>(i + 1), 0);
    lv_obj_add_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(viewport, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(viewport, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(viewport, 0, 0);
    reactive::bind_theme(viewport,
                         app_view_model_ref_().dark_mode_subject(),
                         reactive::ThemeRole::SURFACE);
  }

  selected_observer_handle_ =
      reactive::observe_obj(menu_list_->root(),
                            connectivity_view_model_.selected_index_subject(),
                            selected_observer,
                            this);
  active_page_observer_handle_ =
      reactive::observe_obj(plane_,
                            connectivity_view_model_.active_page_subject(),
                            active_page_observer,
                            this);
  link_restart_observer_handle_ =
      reactive::observe_obj(plane_,
                            connectivity_view_model_.link_restart_request_subject(),
                            link_restart_request_observer,
                            this);
  link_settings_observer_handle_ =
      reactive::observe_obj(plane_,
                            connectivity_view_model_.link_settings_request_subject(),
                            link_settings_request_observer,
                            this);
  uart_settings_observer_handle_ =
      reactive::observe_obj(plane_,
                            connectivity_view_model_.uart_settings_request_subject(),
                            uart_settings_request_observer,
                            this);
  show_page_(connectivity_view_model_.active_page(), false);
}

bool ConnectivityTestPage::back_request_handler(void* user_data) {
  auto* page = static_cast<ConnectivityTestPage*>(user_data);
  if (!page || page->app_view_model_ref_().current_page() != model::AppPage::CONNECTIVITY_TEST) {
    return false;
  }

  return page->connectivity_view_model_.request_back();
}

std::size_t ConnectivityTestPage::viewport_index_(model::ConnectivitySubPage page) const {
  switch (page) {
    case model::ConnectivitySubPage::WIFI:
      return 1;
    case model::ConnectivitySubPage::BLUETOOTH:
      return 2;
    case model::ConnectivitySubPage::ETHERNET:
      return 3;
    case model::ConnectivitySubPage::USB:
      return 4;
    case model::ConnectivitySubPage::HDMI:
      return 5;
    case model::ConnectivitySubPage::I2C:
      return 6;
    case model::ConnectivitySubPage::SPI:
      return 7;
    case model::ConnectivitySubPage::UART:
      return 8;
    case model::ConnectivitySubPage::EXT_IO:
      return 9;
    case model::ConnectivitySubPage::LINK_TEST:
      return 10;
    case model::ConnectivitySubPage::MENU:
    default:
      return 0;
  }
}

void ConnectivityTestPage::show_page_(model::ConnectivitySubPage page, bool animate) {
  if (!plane_) {
    return;
  }

  reset_subpage_views_(page);
  switch_external_bus_(page);
  ensure_subpage_view_(page);
  update_title_(page);
  update_nav_actions_();
  const auto index = viewport_index_(page);
  if (menu_list_) {
    menu_list_->set_focused(page == model::ConnectivitySubPage::MENU);
  }
  lv_obj_set_x(plane_, -static_cast<int32_t>(index) * K_VIEWPORT_WIDTH);
  if (page == model::ConnectivitySubPage::MENU) {
    hide_loading_modal_();
  } else {
    show_loading_modal_(page);
  }
  LV_UNUSED(animate);
}

void ConnectivityTestPage::switch_external_bus_(model::ConnectivitySubPage page) {
  if (page == model::ConnectivitySubPage::I2C || page == model::ConnectivitySubPage::UART) {
    std::string error;
    if (page == model::ConnectivitySubPage::I2C) {
      platform::gpio::set_external_bus_i2c_mode(true, error);
    } else {
      platform::gpio::set_external_bus_uart_mode(error);
    }
  }

  if (page == model::ConnectivitySubPage::UART) {
    uart_page_entered_at_        = lv_tick_get();
    suppress_uart_settings_once_ = true;
  } else {
    suppress_uart_settings_once_ = false;
  }
}

bool ConnectivityTestPage::should_suppress_uart_settings_() {
  if (!suppress_uart_settings_once_) {
    return false;
  }
  suppress_uart_settings_once_ = false;
  return lv_tick_elaps(uart_page_entered_at_) < K_UART_SETTINGS_SUPPRESS_MS;
}

void ConnectivityTestPage::update_title_(model::ConnectivitySubPage page) {
  app_view_model_ref_().set_title_text(title_for_page(page));
}

void ConnectivityTestPage::update_selection_(std::size_t index) {
  if (menu_list_) {
    menu_list_->set_selected_index(index);
  }
}

void ConnectivityTestPage::scroll_active_page_(int32_t direction) {
  if (!plane_ || direction == 0 || connectivity_view_model_.is_menu_active()) {
    return;
  }

  const auto index = viewport_index_(connectivity_view_model_.active_page());
  auto* viewport   = lv_obj_get_child(plane_, static_cast<int32_t>(index));
  if (viewport) {
    lv_obj_update_layout(viewport);
    const int32_t current_y = lv_obj_get_scroll_y(viewport);
    const int32_t max_y     = current_y + lv_obj_get_scroll_bottom(viewport);
    if (max_y <= 0) {
      return;
    }

    const int32_t target_y = std::clamp(current_y + direction * K_SCROLL_STEP, 0, max_y);
    lv_obj_scroll_to_y(viewport, target_y, LV_ANIM_ON);
  }
}

lv_obj_t* ConnectivityTestPage::viewport_for_page_(model::ConnectivitySubPage page) const {
  if (!plane_) {
    return nullptr;
  }
  return lv_obj_get_child(plane_, static_cast<int32_t>(viewport_index_(page)));
}

void ConnectivityTestPage::reset_subpage_views_(model::ConnectivitySubPage keep_page) {
  const auto reset_page = [this, keep_page](model::ConnectivitySubPage page, auto& view) {
    if (keep_page == page) {
      return;
    }

    view.reset();
    auto* viewport = viewport_for_page_(page);
    if (viewport && lv_obj_is_valid(viewport)) {
      lv_obj_clean(viewport);
      lv_obj_scroll_to_y(viewport, 0, LV_ANIM_OFF);
    }
  };

  reset_page(model::ConnectivitySubPage::WIFI, wifi_view_);
  reset_page(model::ConnectivitySubPage::BLUETOOTH, bluetooth_view_);
  reset_page(model::ConnectivitySubPage::ETHERNET, ethernet_view_);
  reset_page(model::ConnectivitySubPage::USB, usb_view_);
  reset_page(model::ConnectivitySubPage::HDMI, hdmi_view_);
  reset_page(model::ConnectivitySubPage::I2C, i2c_view_);
  reset_page(model::ConnectivitySubPage::SPI, spi_view_);
  reset_page(model::ConnectivitySubPage::UART, uart_view_);
  reset_page(model::ConnectivitySubPage::EXT_IO, ext_io_view_);
  reset_page(model::ConnectivitySubPage::LINK_TEST, link_view_);
}

void ConnectivityTestPage::ensure_subpage_view_(model::ConnectivitySubPage page) {
  if (page == model::ConnectivitySubPage::MENU) {
    return;
  }

  switch (page) {
    case model::ConnectivitySubPage::WIFI:
      if (wifi_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::BLUETOOTH:
      if (bluetooth_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::ETHERNET:
      if (ethernet_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::USB:
      if (usb_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::HDMI:
      if (hdmi_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::I2C:
      if (i2c_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::SPI:
      if (spi_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::UART:
      if (uart_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::EXT_IO:
      if (ext_io_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::LINK_TEST:
      if (link_view_) {
        return;
      }
      break;
    case model::ConnectivitySubPage::MENU:
    default:
      return;
  }

  build_subpage_view_(viewport_for_page_(page), page);
}

void ConnectivityTestPage::build_subpage_view_(lv_obj_t* viewport,
                                               model::ConnectivitySubPage page) {
  if (!viewport) {
    return;
  }

  switch (page) {
    case model::ConnectivitySubPage::WIFI:
      wifi_view_ =
          std::make_unique<WifiConnectivityView>(connectivity_view_model_.wifi_view_model());
      wifi_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::BLUETOOTH:
      bluetooth_view_ = std::make_unique<BluetoothConnectivityView>(
          connectivity_view_model_.bluetooth_view_model());
      bluetooth_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::ETHERNET:
      ethernet_view_ = std::make_unique<EthernetConnectivityView>(
          connectivity_view_model_.ethernet_view_model());
      ethernet_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::USB:
      usb_view_ = std::make_unique<UsbConnectivityView>(connectivity_view_model_.usb_view_model());
      usb_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::HDMI:
      hdmi_view_ =
          std::make_unique<HdmiConnectivityView>(connectivity_view_model_.hdmi_view_model());
      hdmi_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::I2C:
      i2c_view_ = std::make_unique<I2cConnectivityView>(connectivity_view_model_.i2c_view_model());
      i2c_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::SPI:
      spi_view_ = std::make_unique<SpiConnectivityView>(connectivity_view_model_.spi_view_model());
      spi_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::UART:
      uart_view_ = std::make_unique<UartConnectivityView>();
      uart_view_->build(viewport, root(), app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::EXT_IO:
      ext_io_view_ = std::make_unique<ExtIoConnectivityView>();
      ext_io_view_->build(viewport, root(), app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::LINK_TEST:
      link_view_ =
          std::make_unique<LinkConnectivityView>(connectivity_view_model_.link_view_model());
      link_view_->build(viewport, root(), app_view_model_ref_(), assets_ref_());
      break;
    case model::ConnectivitySubPage::MENU:
    default:
      break;
  }
}

void ConnectivityTestPage::update_nav_actions_() {
  app_view_model_ref_().clear_nav_actions();
  const auto page = connectivity_view_model_.active_page();
  set_nav_action_(
      '4',
      view::ICON_ARROW_U_UP_LEFT,
      [this]() { app_view_model_ref_().request_back_or_quit(); },
      page == model::ConnectivitySubPage::UART ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED);

  if (page == model::ConnectivitySubPage::LINK_TEST) {
    set_nav_action_('5', view::ICON_ARROWS_CLOCKWISE, [this]() {
      connectivity_view_model_.request_link_restart();
    });
    set_nav_action_('7', view::ICON_GEAR_FINE, [this]() {
      connectivity_view_model_.request_link_settings();
    });
  } else if (page == model::ConnectivitySubPage::UART) {
    set_nav_action_(
        '6',
        view::ICON_GEAR_FINE,
        [this]() { connectivity_view_model_.request_uart_settings(); },
        LV_EVENT_LONG_PRESSED);
  } else if (page == model::ConnectivitySubPage::EXT_IO) {
    set_nav_action_('6', view::ICON_GEAR_FINE, [this]() {
      if (ext_io_view_) {
        ext_io_view_->show_config_dialog();
      }
    });
  }

  set_nav_action_('8', view::ICON_CHECK_SQUARE, [this]() {
    show_test_result_dialog_();
  });
}

void ConnectivityTestPage::show_loading_modal_(model::ConnectivitySubPage page) {
  if (!root()) {
    return;
  }

  if (page == model::ConnectivitySubPage::I2C) {
    hide_loading_modal_();
    return;
  }
  if (page == model::ConnectivitySubPage::EXT_IO) {
    hide_loading_modal_();
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

  auto* label       = lv_label_create(loading_modal_);
  const char* title = page == model::ConnectivitySubPage::I2C         ? "Scanning I2C..."
                      : page == model::ConnectivitySubPage::SPI       ? "Scanning SPI..."
                      : page == model::ConnectivitySubPage::LINK_TEST ? "Testing Link..."
                      : page == model::ConnectivitySubPage::UART      ? "Opening UART..."
                      : page == model::ConnectivitySubPage::ETHERNET  ? "Reading Ethernet..."
                      : page == model::ConnectivitySubPage::HDMI      ? "Reading HDMI..."
                                                                      : "Scanning...";
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

void ConnectivityTestPage::hide_loading_modal_() {
  if (loading_modal_ && lv_obj_is_valid(loading_modal_)) {
    lv_obj_delete(loading_modal_);
  }
  loading_modal_ = nullptr;
}

void ConnectivityTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<ConnectivityTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  const bool uart_page_active =
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::UART;
  if (uart_page_active && page->uart_view_ && page->uart_view_->handle_key(key, key_name)) {
    return;
  }

  const bool link_page_active =
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::LINK_TEST;
  if (link_page_active && page->link_view_) {
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

  const bool ext_io_page_active =
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::EXT_IO;
  if (ext_io_page_active && page->ext_io_view_ && page->ext_io_view_->handle_key(key, key_name)) {
    return;
  }

  if (key == 't' || key == 'T') {
    page->app_view_model_ref_().toggle_dark_mode();
    return;
  }

  switch (key) {
    case LV_KEY_UP:
    case '5':
    case 'f':
    case 'F':
    case LV_KEY_LEFT:
      if (page->connectivity_view_model_.is_menu_active()) {
        page->connectivity_view_model_.select_previous();
      } else {
        page->scroll_active_page_(-1);
      }
      break;
    case LV_KEY_DOWN:
    case '7':
    case 'x':
    case 'X':
    case LV_KEY_RIGHT:
      if (page->connectivity_view_model_.is_menu_active()) {
        page->connectivity_view_model_.select_next();
      } else {
        page->scroll_active_page_(1);
      }
      break;
    case LV_KEY_ENTER:
    case '6':
      page->connectivity_view_model_.activate_selected();
      break;
    case 'r':
    case 'R':
      page->connectivity_view_model_.refresh_active();
      break;
    default:
      break;
  }
}

void ConnectivityTestPage::key_release_listener(uint32_t key,
                                                const char* key_name,
                                                void* user_data) {
  auto* page = static_cast<ConnectivityTestPage*>(user_data);
  if (!page) {
    return;
  }

  const bool uart_page_active =
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::UART;
  if (uart_page_active && page->uart_view_) {
    page->uart_view_->handle_key_release(key, key_name);
  }
}

void ConnectivityTestPage::long_key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<ConnectivityTestPage*>(user_data);
  if (!page) {
    return;
  }

  const bool uart_page_active =
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::UART;
  if (uart_page_active && page->uart_view_) {
    if (page->uart_view_->handle_long_key(key, key_name)) {
      return;
    }
    if (key == LV_KEY_ESC || key == 27) {
      page->app_view_model_ref_().request_back_or_quit();
      return;
    }
  }
}

void ConnectivityTestPage::menu_item_clicked(std::size_t index, void* user_data) {
  auto* page = static_cast<ConnectivityTestPage*>(user_data);
  if (!page) {
    return;
  }

  page->connectivity_view_model_.set_selected_index(index);
  page->connectivity_view_model_.activate_selected();
}

void ConnectivityTestPage::selected_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<ConnectivityTestPage*>(lv_observer_get_user_data(observer));
  if (page) {
    page->update_selection_(static_cast<std::size_t>(lv_subject_get_int(subject)));
  }
}

void ConnectivityTestPage::active_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<ConnectivityTestPage*>(lv_observer_get_user_data(observer));
  if (page) {
    page->show_page_(static_cast<model::ConnectivitySubPage>(lv_subject_get_int(subject)));
  }
}

void ConnectivityTestPage::link_restart_request_observer(lv_observer_t* observer,
                                                         lv_subject_t* subject) {
  LV_UNUSED(subject);

  auto* page = static_cast<ConnectivityTestPage*>(lv_observer_get_user_data(observer));
  if (page && page->link_view_ && !page->link_view_->dialog_visible() &&
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::LINK_TEST) {
    page->link_view_->restart();
  }
}

void ConnectivityTestPage::link_settings_request_observer(lv_observer_t* observer,
                                                          lv_subject_t* subject) {
  LV_UNUSED(subject);

  auto* page = static_cast<ConnectivityTestPage*>(lv_observer_get_user_data(observer));
  if (page && page->link_view_ &&
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::LINK_TEST) {
    page->link_view_->show_config_dialog();
  }
}

void ConnectivityTestPage::uart_settings_request_observer(lv_observer_t* observer,
                                                          lv_subject_t* subject) {
  LV_UNUSED(subject);

  auto* page = static_cast<ConnectivityTestPage*>(lv_observer_get_user_data(observer));
  if (page && page->uart_view_ &&
      page->connectivity_view_model_.active_page() == model::ConnectivitySubPage::UART) {
    if (page->should_suppress_uart_settings_()) {
      return;
    }
    page->uart_view_->show_config_dialog();
  }
}

void ConnectivityTestPage::loading_modal_timer_cb(lv_timer_t* timer) {
  auto* page = static_cast<ConnectivityTestPage*>(lv_timer_get_user_data(timer));
  if (page) {
    page->hide_loading_modal_();
  }
}

}  // namespace screen
