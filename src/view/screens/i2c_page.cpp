/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "i2c_page.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "bindings.h"
#include "io_page_common.h"
#include "theme.h"

namespace screen {
namespace {

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
  lv_obj_set_style_pad_row(panel, 0, 0);
  lv_obj_set_style_pad_top(panel, 0, 0);
  lv_obj_set_style_pad_bottom(panel, 2, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 0);
  reactive::bind_theme(panel, app_view_model.dark_mode_subject(), reactive::ThemeRole::SURFACE);
  return panel;
}

const char* i2c_cell_text(model::I2cAddressState state, uint8_t address) {
  static char present_cells[128][3]{};
  if (state == model::I2cAddressState::KERNEL_DRIVER) {
    return "UU";
  }
  if (state == model::I2cAddressState::ABSENT) {
    return "--";
  }

  constexpr char HEX[]      = "0123456789abcdef";
  present_cells[address][0] = HEX[(address >> 4) & 0x0f];
  present_cells[address][1] = HEX[address & 0x0f];
  present_cells[address][2] = '\0';
  return present_cells[address];
}

std::array<model::I2cAddressState, 128> i2c_address_states(
    const std::vector<model::I2cAddress>& addresses) {
  std::array<model::I2cAddressState, 128> states{};
  states.fill(model::I2cAddressState::ABSENT);
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

void set_i2c_cell_color(lv_obj_t* label,
                        viewmodel::AppViewModel& app_view_model,
                        model::I2cAddressState state,
                        uint8_t address) {
  if (!label) {
    return;
  }

  const bool reserved = address < 0x03 || address > 0x77;
  const auto colors   = view::palette(app_view_model.is_dark_mode());
  if (state == model::I2cAddressState::PRESENT && !reserved) {
    lv_obj_set_style_text_color(label, colors.success, 0);
  } else if (state == model::I2cAddressState::KERNEL_DRIVER && !reserved) {
    lv_obj_set_style_text_color(label, colors.warning, 0);
  } else {
    lv_obj_set_style_text_color(label, reserved ? colors.text_disabled : colors.text_muted, 0);
  }
}

}  // namespace

I2cConnectivityView::I2cConnectivityView(viewmodel::I2cConnectivityViewModel& view_model)
    : view_model_(view_model) {
  cell_states_.fill(model::I2cAddressState::ABSENT);
}

I2cConnectivityView::~I2cConnectivityView() {
  delete_timer(refresh_timer_);
  scanning_popup_.reset();
  if (panel_ && lv_obj_is_valid(panel_)) {
    lv_obj_delete(panel_);
  }
  panel_ = nullptr;
  card_  = nullptr;
  grid_  = nullptr;
  cell_labels_.fill(nullptr);
}

void I2cConnectivityView::build(lv_obj_t* parent,
                                viewmodel::AppViewModel& app_view_model,
                                app::AssetManager& assets) {
  app_view_model_ = &app_view_model;
  assets_         = &assets;
  panel_          = build_i2c_panel(parent, app_view_model);
  build_static_content_();
  refresh_();
  refresh_timer_ = lv_timer_create(refresh_timer_cb, 500, this);
}

void I2cConnectivityView::build_static_content_() {
  if (!panel_ || !app_view_model_ || !assets_) {
    return;
  }

  auto* header_font = assets_->load_font("inter-semibold.ttf", 10);
  auto* cell_font   = assets_->load_font("inter-medium.ttf", 10);
  const auto colors = view::palette(app_view_model_->is_dark_mode());

  card_ = lv_obj_create(panel_);
  lv_obj_remove_style_all(card_);
  lv_obj_set_width(card_, K_I2C_CARD_WIDTH);
  lv_obj_set_height(card_, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(card_, 3, 0);
  lv_obj_set_style_pad_row(card_, 0, 0);
  lv_obj_set_style_radius(card_, 8, 0);
  lv_obj_set_style_bg_opa(card_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card_, colors.button, 0);
  lv_obj_set_style_border_width(card_, 1, 0);
  lv_obj_set_style_border_color(card_, colors.border, 0);
  lv_obj_set_style_outline_width(card_, 1, 0);
  lv_obj_set_style_outline_pad(card_, 0, 0);
  lv_obj_set_style_outline_opa(card_, LV_OPA_20, 0);
  lv_obj_set_style_outline_color(card_, colors.border, 0);
  lv_obj_clear_flag(card_, LV_OBJ_FLAG_SCROLLABLE);

  grid_ = lv_obj_create(card_);
  lv_obj_remove_style_all(grid_);
  lv_obj_set_width(grid_, LV_SIZE_CONTENT);
  lv_obj_set_height(grid_, LV_SIZE_CONTENT);
  lv_obj_set_style_grid_column_dsc_array(grid_, K_I2C_GRID_COLUMNS, 0);
  lv_obj_set_style_grid_row_dsc_array(grid_, K_I2C_GRID_ROWS, 0);
  lv_obj_set_style_pad_column(grid_, 2, 0);
  lv_obj_set_style_pad_row(grid_, 2, 0);
  lv_obj_set_layout(grid_, LV_LAYOUT_GRID);
  lv_obj_clear_flag(grid_, LV_OBJ_FLAG_SCROLLABLE);

  constexpr char HEX[] = "0123456789abcdef";
  for (uint8_t col = 0; col <= 0x0f; ++col) {
    char text[2] = {HEX[col], '\0'};
    auto* label  = add_i2c_text_label(grid_,
                                      *app_view_model_,
                                      header_font ? header_font : &lv_font_montserrat_12,
                                      text);
    lv_obj_set_grid_cell(label,
                         LV_GRID_ALIGN_CENTER,
                         static_cast<int32_t>(col) + 1,
                         1,
                         LV_GRID_ALIGN_CENTER,
                         0,
                         1);
  }

  for (uint8_t row = 0; row <= 0x70; row = static_cast<uint8_t>(row + 0x10)) {
    char row_text[4]       = {HEX[(row >> 4) & 0x0f], '0', ':', '\0'};
    auto* row_label        = add_i2c_text_label(grid_,
                                                *app_view_model_,
                                                header_font ? header_font : &lv_font_montserrat_12,
                                                row_text);
    const int32_t grid_row = static_cast<int32_t>(row / 0x10) + 1;
    lv_obj_set_grid_cell(row_label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, grid_row, 1);

    for (uint8_t col = 0; col <= 0x0f; ++col) {
      const uint8_t address = static_cast<uint8_t>(row + col);
      const bool reserved   = address < 0x03 || address > 0x77;
      auto* label           = add_i2c_text_label(grid_,
                                                 *app_view_model_,
                                                 cell_font ? cell_font : &lv_font_montserrat_12,
                                                 reserved ? "" : "--");
      lv_obj_set_width(label, K_I2C_CELL_WIDTH);
      set_i2c_cell_color(label, *app_view_model_, model::I2cAddressState::ABSENT, address);
      lv_obj_set_grid_cell(label,
                           LV_GRID_ALIGN_CENTER,
                           static_cast<int32_t>(col) + 1,
                           1,
                           LV_GRID_ALIGN_CENTER,
                           grid_row,
                           1);
      cell_labels_[address] = label;
    }
  }

  panel_initialized_ = true;
  lv_obj_update_layout(panel_);
}

void I2cConnectivityView::update_content_() {
  if (!panel_initialized_ || !app_view_model_) {
    return;
  }

  const auto states = i2c_address_states(view_model_.addresses());
  for (std::size_t address_index = 0; address_index < cell_labels_.size(); ++address_index) {
    const auto address  = static_cast<uint8_t>(address_index);
    const bool reserved = address < 0x03 || address > 0x77;
    if (!cell_labels_[address] || (states[address] == cell_states_[address] && !reserved)) {
      continue;
    }
    lv_label_set_text(cell_labels_[address],
                      reserved ? "" : i2c_cell_text(states[address], address));
    set_i2c_cell_color(cell_labels_[address], *app_view_model_, states[address], address);
    cell_states_[address] = states[address];
  }
}

void I2cConnectivityView::update_scanning_popup_() {
  if (!app_view_model_) {
    return;
  }

  if (!scanning_popup_) {
    view::widgets::PopupConfig config;
    config.width       = 168;
    config.height      = 40;
    config.offset_y    = 0;
    config.radius      = 10;
    config.pad_all     = 6;
    config.label_width = 152;
    config.message     = "Scanning I2C...";
    config.font = assets_ ? assets_->load_font("inter-semibold.ttf", 12) : &lv_font_montserrat_12;
    scanning_popup_ =
        std::make_unique<view::widgets::Popup>(lv_layer_top(), *app_view_model_, config);
    scanning_popup_->build();
  }

  if (view_model_.is_scanning()) {
    scanning_popup_->show();
  } else {
    scanning_popup_->hide();
  }
}

void I2cConnectivityView::refresh_() {
  const bool changed = view_model_.refresh();
  if ((changed || !panel_initialized_) && app_view_model_ && assets_) {
    if (!panel_initialized_) {
      build_static_content_();
    }
    update_content_();
  }
  update_scanning_popup_();
}

void I2cConnectivityView::refresh_timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<I2cConnectivityView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->refresh_();
  }
}

}  // namespace screen
