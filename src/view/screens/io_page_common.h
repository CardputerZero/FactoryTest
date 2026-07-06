/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "connectivity_test_viewmodel.h"
#include "icon_card.h"
#include "lvgl.h"
#include "ui_const.h"

namespace screen {

constexpr uint32_t K_REFRESH_POLL_MS = 250;
constexpr int32_t K_LIST_WIDTH       = 300;
constexpr int32_t K_CARD_WIDTH       = 300;
constexpr int32_t K_CARD_HEIGHT      = 26;
constexpr int32_t K_PANEL_WIDTH      = 300;
constexpr int32_t K_PANEL_ROW_WIDTH  = 276;
constexpr int32_t K_PANEL_ROW_HEIGHT = 23;
constexpr int32_t K_PANEL_ICON_WIDTH = 24;
constexpr int32_t K_I2C_PANEL_WIDTH   = 304;
constexpr int32_t K_I2C_CARD_WIDTH    = 300;
constexpr int32_t K_I2C_CONTENT_WIDTH = 286;
constexpr int32_t K_I2C_ROW_LABEL_W   = 22;
constexpr int32_t K_I2C_CELL_WIDTH    = 15;
constexpr int32_t K_I2C_HEADER_H      = 14;
constexpr int32_t K_I2C_CELL_HEIGHT   = 16;
constexpr int32_t K_LINK_PANEL_WIDTH  = 304;
constexpr int32_t K_LINK_CARD_WIDTH   = 300;
constexpr int32_t K_LINK_ROW_WIDTH    = 280;
constexpr int32_t K_LINK_ROW_HEIGHT   = 29;

inline const int32_t K_I2C_GRID_COLUMNS[] = {K_I2C_ROW_LABEL_W,
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
inline const int32_t K_I2C_GRID_ROWS[] = {K_I2C_HEADER_H,
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

struct ScanPanelRow {
  const char* icon{view::ICON_INFO};
  std::string text{};
};

lv_obj_t* build_card_list(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model);
CardFonts load_card_fonts(app::AssetManager& assets, viewmodel::AppViewModel& app_view_model);
void rebuild_cards(lv_obj_t* list,
                   std::vector<std::unique_ptr<view::widgets::IconCard>>& cards,
                   viewmodel::AppViewModel& app_view_model,
                   app::AssetManager& assets,
                   const std::vector<CardSpec>& specs);
bool cards_need_refresh(const std::vector<std::unique_ptr<view::widgets::IconCard>>& cards,
                        bool data_changed);
lv_obj_t* build_scan_panel(lv_obj_t* parent, viewmodel::AppViewModel& app_view_model);
void rebuild_scan_panel(lv_obj_t* panel,
                        viewmodel::AppViewModel& app_view_model,
                        app::AssetManager& assets,
                        const std::vector<ScanPanelRow>& rows);
void delete_timer(lv_timer_t*& timer);

}  // namespace screen
