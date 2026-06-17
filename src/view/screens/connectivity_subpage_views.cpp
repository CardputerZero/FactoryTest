/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_subpage_views.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "bindings.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr uint32_t K_REFRESH_POLL_MS = 250;
constexpr int32_t K_LIST_WIDTH       = 300;
constexpr int32_t K_CARD_WIDTH       = 300;
constexpr int32_t K_CARD_HEIGHT      = 26;
constexpr int32_t K_PANEL_WIDTH      = 300;
constexpr int32_t K_PANEL_ROW_WIDTH  = 276;
constexpr int32_t K_PANEL_ROW_HEIGHT = 23;
constexpr int32_t K_PANEL_ICON_WIDTH = 24;
constexpr int32_t K_I2C_PANEL_WIDTH  = 304;
constexpr int32_t K_I2C_ROW_LABEL_W  = 22;
constexpr int32_t K_I2C_CELL_WIDTH   = 15;
constexpr int32_t K_I2C_HEADER_H     = 14;
constexpr int32_t K_I2C_CELL_HEIGHT  = 16;

const int32_t K_I2C_GRID_COLUMNS[] = {K_I2C_ROW_LABEL_W,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      K_I2C_CELL_WIDTH,
                                      LV_GRID_TEMPLATE_LAST};
const int32_t K_I2C_GRID_ROWS[]    = {K_I2C_HEADER_H,
                                      K_I2C_CELL_HEIGHT,
                                      K_I2C_CELL_HEIGHT,
                                      K_I2C_CELL_HEIGHT,
                                      K_I2C_CELL_HEIGHT,
                                      K_I2C_CELL_HEIGHT,
                                      K_I2C_CELL_HEIGHT,
                                      K_I2C_CELL_HEIGHT,
                                      K_I2C_CELL_HEIGHT,
                                      LV_GRID_TEMPLATE_LAST};

struct CardSpec {
  const char* icon{view::ICON_INFO};
  std::string title{};
  std::string value{};
  lv_label_long_mode_t value_long_mode{LV_LABEL_LONG_SCROLL};
};

struct CardFonts {
  const lv_font_t* icon{&lv_font_montserrat_14};
  const lv_font_t* title{&lv_font_montserrat_12};
  const lv_font_t* value{&lv_font_montserrat_12};
};

lv_obj_t* build_i2c_panel(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model) {
  if (!parent) {
    return nullptr;
  }

  lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_AUTO);

  auto* panel = lv_obj_create(parent);
  lv_obj_remove_style_all(panel);
  lv_obj_set_width(panel, K_I2C_PANEL_WIDTH);
  lv_obj_set_height(panel, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(panel, 4, 0);
  lv_obj_set_style_pad_top(panel, 2, 0);
  lv_obj_set_style_pad_bottom(panel, 6, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 0);
  reactive::bind_theme(panel, app_view_model.dark_mode_subject(), reactive::ThemeRole::SURFACE);
  return panel;
}

lv_obj_t* build_card_list(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model) {
  if (!parent) {
    return nullptr;
  }

  auto* list = lv_obj_create(parent);
  lv_obj_remove_style_all(list);
  lv_obj_set_width(list, K_LIST_WIDTH);
  lv_obj_set_height(list, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_style_pad_top(list, 2, 0);
  lv_obj_set_style_pad_bottom(list, 4, 0);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);
  reactive::bind_theme(list, app_view_model.dark_mode_subject(), reactive::ThemeRole::SURFACE);
  return list;
}

CardFonts load_card_fonts(app::AssetManager& assets) {
  auto* icon_font  = assets.load_font("Phosphor-Fill.ttf", 14);
  auto* title_font = assets.load_font("inter-semibold.ttf", 11);
  auto* value_font = assets.load_font("inter-medium.ttf", 11);
  return {icon_font ? icon_font : &lv_font_montserrat_14,
          title_font ? title_font : &lv_font_montserrat_12,
          value_font ? value_font : &lv_font_montserrat_12};
}

void rebuild_cards(lv_obj_t* list,
                   std::vector<std::unique_ptr<view::widgets::IconCard>>& cards,
                   viewmodel::AppViewModel& app_view_model,
                   app::AssetManager& assets,
                   const std::vector<CardSpec>& specs) {
  if (!list) {
    return;
  }

  cards.clear();
  cards.reserve(specs.size());
  const auto fonts = load_card_fonts(assets);
  for (const auto& spec : specs) {
    auto card = std::make_unique<view::widgets::IconCard>(list,
                                                          app_view_model,
                                                          spec.icon,
                                                          spec.title.c_str(),
                                                          spec.value.c_str(),
                                                          fonts.icon,
                                                          fonts.title,
                                                          fonts.value,
                                                          K_CARD_WIDTH,
                                                          K_CARD_HEIGHT,
                                                          spec.value_long_mode);
    card->build();
    cards.push_back(std::move(card));
  }
  lv_obj_update_layout(list);
  auto* viewport = lv_obj_get_parent(list);
  if (viewport) {
    lv_obj_update_layout(viewport);
    const int32_t current_y = lv_obj_get_scroll_y(viewport);
    const int32_t max_y     = current_y + lv_obj_get_scroll_bottom(viewport);
    const int32_t target_y  = max_y > 0 ? max_y : 0;
    if (current_y > target_y) {
      lv_obj_scroll_to_y(viewport, target_y, LV_ANIM_OFF);
    }
  }
}

bool cards_need_refresh(const std::vector<std::unique_ptr<view::widgets::IconCard>>& cards,
                        bool data_changed) {
  return data_changed || cards.empty();
}

lv_obj_t* build_scan_panel(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model) {
  if (!parent) {
    return nullptr;
  }

  auto* panel = lv_obj_create(parent);
  lv_obj_remove_style_all(panel);
  lv_obj_set_width(panel, K_PANEL_WIDTH);
  lv_obj_set_height(panel, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 2);
  reactive::bind_theme(panel, app_view_model.dark_mode_subject(), reactive::ThemeRole::BUTTON);
  return panel;
}

const char* wifi_icon_for_strength(int32_t strength_percent) {
  if (strength_percent < 0) {
    return view::ICON_WIFI_NONE;
  }
  if (strength_percent < 35) {
    return view::ICON_WIFI_LOW;
  }
  if (strength_percent < 70) {
    return view::ICON_WIFI_MEDIUM;
  }
  return view::ICON_WIFI_HIGH;
}

struct ScanPanelRow {
  const char* icon{view::ICON_INFO};
  std::string text{};
};

void add_scan_panel_divider(lv_obj_t* panel, viewmodel::AppViewModel& app_view_model) {
  auto* divider = lv_obj_create(panel);
  lv_obj_remove_style_all(divider);
  lv_obj_set_size(divider, K_PANEL_ROW_WIDTH, 1);
  lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
  reactive::bind_theme(divider, app_view_model.dark_mode_subject(), reactive::ThemeRole::BAR);
}

void add_scan_panel_row(lv_obj_t* panel,
                        viewmodel::AppViewModel& app_view_model,
                        const CardFonts& fonts,
                        const ScanPanelRow& row) {
  auto* row_obj = lv_obj_create(panel);
  lv_obj_remove_style_all(row_obj);
  lv_obj_set_size(row_obj, K_PANEL_ROW_WIDTH, K_PANEL_ROW_HEIGHT);
  lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row_obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row_obj, 4, 0);
  lv_obj_clear_flag(row_obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(row_obj, LV_OBJ_FLAG_CLICKABLE);

  auto* icon_label = lv_label_create(row_obj);
  lv_label_set_text(icon_label, row.icon ? row.icon : view::ICON_INFO);
  lv_obj_set_width(icon_label, K_PANEL_ICON_WIDTH);
  lv_obj_set_style_text_align(icon_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(icon_label, fonts.icon, 0);
  reactive::bind_theme(icon_label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);

  auto* text_label = lv_label_create(row_obj);
  lv_label_set_text(text_label, row.text.empty() ? "<unknown>" : row.text.c_str());
  lv_label_set_long_mode(text_label, LV_LABEL_LONG_SCROLL);
  lv_obj_set_width(text_label, K_PANEL_ROW_WIDTH - K_PANEL_ICON_WIDTH - 8);
  lv_obj_set_style_text_font(text_label, fonts.value, 0);
  reactive::bind_theme(text_label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);
}

void rebuild_scan_panel(lv_obj_t* panel,
                        viewmodel::AppViewModel& app_view_model,
                        app::AssetManager& assets,
                        const std::vector<ScanPanelRow>& rows) {
  if (!panel) {
    return;
  }

  lv_obj_clean(panel);
  const auto fonts = load_card_fonts(assets);
  if (rows.empty()) {
    add_scan_panel_row(panel, app_view_model, fonts, {view::ICON_INFO, "No devices found"});
  } else {
    for (std::size_t i = 0; i < rows.size(); ++i) {
      if (i > 0) {
        add_scan_panel_divider(panel, app_view_model);
      }
      add_scan_panel_row(panel, app_view_model, fonts, rows[i]);
    }
  }

  lv_obj_update_layout(panel);
  auto* viewport = lv_obj_get_parent(panel);
  if (viewport) {
    lv_obj_update_layout(viewport);
    const int32_t current_y = lv_obj_get_scroll_y(viewport);
    const int32_t max_y     = current_y + lv_obj_get_scroll_bottom(viewport);
    const int32_t target_y  = max_y > 0 ? max_y : 0;
    if (current_y > target_y) {
      lv_obj_scroll_to_y(viewport, target_y, LV_ANIM_OFF);
    }
  }
}

const char* i2c_cell_text(model::ConnectivityI2cAddressState state, uint8_t address) {
  static char present_cells[128][3]{};
  if (state == model::ConnectivityI2cAddressState::KERNEL_DRIVER) {
    return "UU";
  }
  if (state == model::ConnectivityI2cAddressState::ABSENT) {
    return "--";
  }

  constexpr char HEX[]      = "0123456789abcdef";
  present_cells[address][0] = HEX[(address >> 4) & 0x0f];
  present_cells[address][1] = HEX[address & 0x0f];
  present_cells[address][2] = '\0';
  return present_cells[address];
}

std::vector<ScanPanelRow> wifi_panel_rows(const std::vector<model::ConnectivityScanInfo>& items,
                                          const std::string& error_message) {
  std::vector<ScanPanelRow> rows;
  rows.reserve(items.size() + 1);
  if (!error_message.empty()) {
    rows.push_back({view::ICON_INFO, error_message});
  }
  for (const auto& item : items) {
    rows.push_back({wifi_icon_for_strength(item.strength_percent),
                    item.name.empty() ? std::string{"<hidden>"} : item.name});
  }
  if (items.empty() && error_message.empty()) {
    rows.push_back({view::ICON_INFO, "No Wi-Fi access points found"});
  }
  return rows;
}

std::vector<ScanPanelRow> bluetooth_panel_rows(
    const std::vector<model::ConnectivityScanInfo>& items,
    const std::string& error_message) {
  std::vector<ScanPanelRow> rows;
  rows.reserve(items.size() + 1);
  if (!error_message.empty()) {
    rows.push_back({view::ICON_INFO, error_message});
  }
  for (const auto& item : items) {
    const auto text = !item.name.empty()
                          ? item.name
                          : (!item.detail.empty() ? item.detail : std::string{"Bluetooth Device"});
    rows.push_back({view::ICON_BLUETOOTH, text});
  }
  if (items.empty() && error_message.empty()) {
    rows.push_back({view::ICON_INFO, "No Bluetooth devices found"});
  }
  return rows;
}

std::vector<CardSpec> ethernet_card_specs(const std::vector<model::ConnectivityInfoField>& fields,
                                          const std::string& error_message) {
  std::vector<CardSpec> specs;
  specs.reserve(fields.size() + 1);
  if (!error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", error_message});
  }
  for (const auto& field : fields) {
    specs.push_back({view::ICON_ETHERNET, field.label, field.value});
  }
  if (fields.empty() && error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", "No Ethernet information"});
  }
  return specs;
}

std::vector<CardSpec> usb_card_specs(const std::vector<model::ConnectivityScanInfo>& items,
                                     const std::string& error_message) {
  std::vector<CardSpec> specs;
  specs.reserve(items.size() + 1);
  if (!error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", error_message});
  }
  for (const auto& item : items) {
    const bool name_only = item.detail.empty() && !item.name.empty() && item.name != "USB Device";
    const auto title =
        name_only ? std::string{} : (item.detail.empty() ? std::string{"USB"} : item.detail);
    const auto value = item.name.empty() ? std::string{"USB Device"} : item.name;
    specs.push_back({view::ICON_USB, title, value});
  }
  if (items.empty() && error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", "No USB devices found"});
  }
  return specs;
}

std::vector<CardSpec> spi_card_specs(const std::vector<model::ConnectivityScanInfo>& items,
                                     const std::string& error_message) {
  std::vector<CardSpec> specs;
  specs.reserve(items.size() + 1);
  if (!error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", error_message});
  }
  for (const auto& item : items) {
    specs.push_back(
        {view::ICON_LINK_SIMPLE_HOR, "Device", item.name.empty() ? "<unknown>" : item.name});
  }
  if (items.empty() && error_message.empty()) {
    specs.push_back({view::ICON_INFO, "Status", "No SPI devices found"});
  }
  return specs;
}

std::array<model::ConnectivityI2cAddressState, 128> i2c_address_states(
    const std::vector<model::ConnectivityI2cAddressInfo>& addresses) {
  std::array<model::ConnectivityI2cAddressState, 128> states{};
  states.fill(model::ConnectivityI2cAddressState::ABSENT);
  for (const auto& address : addresses) {
    if (address.address < states.size()) {
      states[address.address] = address.state;
    }
  }
  return states;
}

lv_obj_t* add_i2c_text_label(lv_obj_t* parent,
                             viewmodel::AppViewModel& app_view_model,
                             const lv_font_t* font,
                             const char* text,
                             lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
  auto* label = lv_label_create(parent);
  lv_label_set_text(label, text ? text : "");
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_align(label, align, 0);
  reactive::bind_theme(label, app_view_model.dark_mode_subject(), reactive::ThemeRole::TEXT);
  return label;
}

void add_i2c_grid_header(lv_obj_t* grid,
                         viewmodel::AppViewModel& app_view_model,
                         const lv_font_t* font) {
  constexpr char HEX[] = "0123456789abcdef";
  for (uint8_t col = 0; col <= 0x0f; ++col) {
    char text[2] = {HEX[col], '\0'};
    auto* label  = add_i2c_text_label(grid, app_view_model, font, text);
    lv_obj_set_grid_cell(label,
                         LV_GRID_ALIGN_CENTER,
                         static_cast<int32_t>(col) + 1,
                         1,
                         LV_GRID_ALIGN_CENTER,
                         0,
                         1);
  }
}

void add_i2c_grid_row(lv_obj_t* grid,
                      viewmodel::AppViewModel& app_view_model,
                      const lv_font_t* header_font,
                      const lv_font_t* cell_font,
                      const std::array<model::ConnectivityI2cAddressState, 128>& states,
                      uint8_t row) {
  constexpr char HEX[]   = "0123456789abcdef";
  char row_text[4]       = {HEX[(row >> 4) & 0x0f], '0', ':', '\0'};
  auto* row_label        = add_i2c_text_label(grid, app_view_model, header_font, row_text);
  const int32_t grid_row = static_cast<int32_t>(row / 0x10) + 1;
  lv_obj_set_grid_cell(row_label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, grid_row, 1);

  for (uint8_t col = 0; col <= 0x0f; ++col) {
    const uint8_t address = static_cast<uint8_t>(row + col);
    const char* text =
        (address < 0x03 || address > 0x77) ? "" : i2c_cell_text(states[address], address);
    const bool reserved = address < 0x03 || address > 0x77;
    const auto colors   = view::palette(app_view_model.is_dark_mode());

    auto* label = add_i2c_text_label(grid, app_view_model, cell_font, text);
    lv_obj_set_width(label, K_I2C_CELL_WIDTH);

    if (states[address] == model::ConnectivityI2cAddressState::PRESENT && !reserved) {
      lv_obj_set_style_text_color(label, colors.success, 0);
    } else if (states[address] == model::ConnectivityI2cAddressState::KERNEL_DRIVER && !reserved) {
      lv_obj_set_style_text_color(label, colors.warning, 0);
    } else {
      lv_obj_set_style_text_color(label, reserved ? colors.text_disabled : colors.text_muted, 0);
    }

    lv_obj_set_grid_cell(label,
                         LV_GRID_ALIGN_CENTER,
                         static_cast<int32_t>(col) + 1,
                         1,
                         LV_GRID_ALIGN_CENTER,
                         grid_row,
                         1);
  }
}

void rebuild_i2c_panel(lv_obj_t* panel,
                       viewmodel::AppViewModel& app_view_model,
                       app::AssetManager& assets,
                       const char* title,
                       const std::vector<model::ConnectivityI2cAddressInfo>& addresses,
                       const std::string& error_message) {
  if (!panel) {
    return;
  }

  lv_obj_clean(panel);
  auto* title_font  = assets.load_font("inter-semibold.ttf", 12);
  auto* status_font = assets.load_font("inter-medium.ttf", 11);
  auto* header_font = assets.load_font("inter-semibold.ttf", 10);
  auto* cell_font   = assets.load_font("inter-medium.ttf", 10);

  std::ostringstream title_text;
  title_text << (title ? title : "I2C") << " Bus 1";
  auto* title_label = add_i2c_text_label(panel,
                                         app_view_model,
                                         title_font ? title_font : &lv_font_montserrat_12,
                                         title_text.str().c_str());
  lv_obj_set_width(title_label, K_I2C_PANEL_WIDTH);

  if (!error_message.empty() || addresses.empty()) {
    const auto status  = !error_message.empty() ? error_message : std::string{"No I2C scan result"};
    auto* status_label = add_i2c_text_label(panel,
                                            app_view_model,
                                            status_font ? status_font : &lv_font_montserrat_12,
                                            status.c_str());
    lv_obj_set_width(status_label, K_I2C_PANEL_WIDTH);
  }

  if (addresses.empty()) {
    return;
  }

  const auto colors = view::palette(app_view_model.is_dark_mode());
  auto* card        = lv_obj_create(panel);
  lv_obj_remove_style_all(card);
  lv_obj_set_width(card, K_I2C_PANEL_WIDTH);
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(card, 5, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, colors.button, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_outline_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  auto* grid = lv_obj_create(card);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, LV_SIZE_CONTENT);
  lv_obj_set_height(grid, LV_SIZE_CONTENT);
  lv_obj_set_style_grid_column_dsc_array(grid, K_I2C_GRID_COLUMNS, 0);
  lv_obj_set_style_grid_row_dsc_array(grid, K_I2C_GRID_ROWS, 0);
  lv_obj_set_style_pad_column(grid, 2, 0);
  lv_obj_set_style_pad_row(grid, 2, 0);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(grid, app_view_model.dark_mode_subject(), reactive::ThemeRole::SURFACE);

  const auto states = i2c_address_states(addresses);
  add_i2c_grid_header(grid, app_view_model, header_font ? header_font : &lv_font_montserrat_12);
  for (uint8_t row = 0; row <= 0x70; row = static_cast<uint8_t>(row + 0x10)) {
    add_i2c_grid_row(grid,
                     app_view_model,
                     header_font ? header_font : &lv_font_montserrat_12,
                     cell_font ? cell_font : &lv_font_montserrat_12,
                     states,
                     row);
  }

  lv_obj_update_layout(panel);
  auto* viewport = lv_obj_get_parent(panel);
  if (viewport) {
    lv_obj_update_layout(viewport);
  }
}

void delete_timer(lv_timer_t*& timer) {
  if (timer) {
    lv_timer_delete(timer);
    timer = nullptr;
  }
}

}  // namespace

WifiConnectivityView::WifiConnectivityView(viewmodel::WifiConnectivityViewModel& view_model)
    : view_model_(view_model) {}

WifiConnectivityView::~WifiConnectivityView() { delete_timer(refresh_timer_); }

void WifiConnectivityView::build(lv_obj_t* parent,
                                 viewmodel::AppViewModel& app_view_model,
                                 app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  panel_          = build_scan_panel(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void WifiConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if ((changed || !panel_initialized_) && app_view_model_ && assets_) {
    const auto rows = wifi_panel_rows(view_model_.scan_items(), view_model_.error_message());
    rebuild_scan_panel(panel_, *app_view_model_, *assets_, rows);
    panel_initialized_ = true;
  }
}

void WifiConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<WifiConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

BluetoothConnectivityView::BluetoothConnectivityView(
    viewmodel::BluetoothConnectivityViewModel& view_model)
    : view_model_(view_model) {}

BluetoothConnectivityView::~BluetoothConnectivityView() { delete_timer(refresh_timer_); }

void BluetoothConnectivityView::build(lv_obj_t* parent,
                                      viewmodel::AppViewModel& app_view_model,
                                      app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  panel_          = build_scan_panel(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void BluetoothConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if ((changed || !panel_initialized_) && app_view_model_ && assets_) {
    const auto rows = bluetooth_panel_rows(view_model_.scan_items(), view_model_.error_message());
    rebuild_scan_panel(panel_, *app_view_model_, *assets_, rows);
    panel_initialized_ = true;
  }
}

void BluetoothConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<BluetoothConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

EthernetConnectivityView::EthernetConnectivityView(
    viewmodel::EthernetConnectivityViewModel& view_model)
    : view_model_(view_model) {}

EthernetConnectivityView::~EthernetConnectivityView() { delete_timer(refresh_timer_); }

void EthernetConnectivityView::build(lv_obj_t* parent,
                                     viewmodel::AppViewModel& app_view_model,
                                     app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  list_           = build_card_list(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void EthernetConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if (cards_need_refresh(cards_, changed) && app_view_model_ && assets_) {
    const auto specs = ethernet_card_specs(view_model_.fields(), view_model_.error_message());
    rebuild_cards(list_, cards_, *app_view_model_, *assets_, specs);
  }
}

void EthernetConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<EthernetConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

UsbConnectivityView::UsbConnectivityView(viewmodel::UsbConnectivityViewModel& view_model)
    : view_model_(view_model) {}

UsbConnectivityView::~UsbConnectivityView() { delete_timer(refresh_timer_); }

void UsbConnectivityView::build(lv_obj_t* parent,
                                viewmodel::AppViewModel& app_view_model,
                                app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  list_           = build_card_list(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void UsbConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if (cards_need_refresh(cards_, changed) && app_view_model_ && assets_) {
    const auto specs = usb_card_specs(view_model_.devices(), view_model_.error_message());
    rebuild_cards(list_, cards_, *app_view_model_, *assets_, specs);
  }
}

void UsbConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<UsbConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

I2cConnectivityView::I2cConnectivityView(viewmodel::I2cConnectivityViewModel& view_model)
    : view_model_(view_model) {}

I2cConnectivityView::~I2cConnectivityView() { delete_timer(refresh_timer_); }

void I2cConnectivityView::build(lv_obj_t* parent,
                                viewmodel::AppViewModel& app_view_model,
                                app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  panel_          = build_i2c_panel(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void I2cConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if ((changed || !panel_initialized_) && app_view_model_ && assets_) {
    rebuild_i2c_panel(panel_,
                      *app_view_model_,
                      *assets_,
                      view_model_.title_text(),
                      view_model_.addresses(),
                      view_model_.error_message());
    panel_initialized_ = true;
  }
}

void I2cConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<I2cConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

SpiConnectivityView::SpiConnectivityView(viewmodel::SpiConnectivityViewModel& view_model)
    : view_model_(view_model) {}

SpiConnectivityView::~SpiConnectivityView() { delete_timer(refresh_timer_); }

void SpiConnectivityView::build(lv_obj_t* parent,
                                viewmodel::AppViewModel& app_view_model,
                                app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  list_           = build_card_list(parent, app_view_model);
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, K_REFRESH_POLL_MS, this);
}

void SpiConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if (cards_need_refresh(cards_, changed) && app_view_model_ && assets_) {
    const auto specs = spi_card_specs(view_model_.devices(), view_model_.error_message());
    rebuild_cards(list_, cards_, *app_view_model_, *assets_, specs);
  }
}

void SpiConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<SpiConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

}  // namespace screen
