/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "connectivity_subpage_common.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "bindings.h"

namespace screen {

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

void delete_timer(lv_timer_t*& timer) {
  if (timer) {
    lv_timer_delete(timer);
    timer = nullptr;
  }
}

}  // namespace screen
