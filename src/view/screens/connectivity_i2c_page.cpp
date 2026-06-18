/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_i2c_page.h"

#include "connectivity_subpage_common.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "bindings.h"
#include "theme.h"

namespace screen {

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

  const auto colors = view::palette(app_view_model.is_dark_mode());
  auto* card        = lv_obj_create(panel);
  lv_obj_remove_style_all(card);
  lv_obj_set_width(card, K_I2C_CARD_WIDTH);
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(card, 5, 0);
  lv_obj_set_style_pad_row(card, 4, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, colors.button, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, colors.border, 0);
  lv_obj_set_style_outline_width(card, 1, 0);
  lv_obj_set_style_outline_pad(card, 0, 0);
  lv_obj_set_style_outline_opa(card, LV_OPA_20, 0);
  lv_obj_set_style_outline_color(card, colors.border, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  std::ostringstream title_text;
  title_text << (title ? title : "I2C") << " Bus 1";
  auto* title_label = add_i2c_text_label(card,
                                         app_view_model,
                                         title_font ? title_font : &lv_font_montserrat_12,
                                         title_text.str().c_str());
  lv_obj_set_width(title_label, K_I2C_CONTENT_WIDTH);

  if (!error_message.empty() || addresses.empty()) {
    const auto status  = !error_message.empty() ? error_message : std::string{"No I2C scan result"};
    auto* status_label = add_i2c_text_label(card,
                                            app_view_model,
                                            status_font ? status_font : &lv_font_montserrat_12,
                                            status.c_str());
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_width(status_label, K_I2C_CONTENT_WIDTH);
  }

  if (addresses.empty()) {
    lv_obj_update_layout(panel);
    return;
  }

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

}  // namespace screen
