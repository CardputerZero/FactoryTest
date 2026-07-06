/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "placeholder_test_page.h"

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"

namespace screen {

PlaceholderTestPage::PlaceholderTestPage(viewmodel::AppViewModel& app_view_model,
                                         app::AssetManager& assets,
                                         const char* title,
                                         const char* message)
    : BaseScreen(app_view_model, assets),
      title_(title),
      message_(message) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  set_default_test_nav_();
  init();
}

PlaceholderTestPage::~PlaceholderTestPage() = default;

void PlaceholderTestPage::build_content(lv_obj_t* content) {
  auto* group = lv_obj_create(content);
  lv_obj_remove_style_all(group);
  lv_obj_set_size(group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(group, 10, 0);
  lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(group);

  auto* title_label = lv_label_create(group);
  const auto title = app_view_model_ref_().tr(title_ ? title_ : "Placeholder Test");
  lv_label_set_text(title_label, title.c_str());
  auto* title_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 18);
  lv_obj_set_style_text_font(title_label, title_font ? title_font : &lv_font_montserrat_18, 0);
  reactive::bind_theme(title_label,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  auto* body_label = lv_label_create(group);
  const auto message =
      app_view_model_ref_().tr(message_ ? message_ : "This test page is not implemented yet.");
  lv_label_set_text(body_label, message.c_str());
  lv_obj_set_width(body_label, 260);
  lv_obj_set_style_text_align(body_label, LV_TEXT_ALIGN_CENTER, 0);
  auto* body_font =
      assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 14);
  lv_obj_set_style_text_font(body_label, body_font ? body_font : &lv_font_montserrat_14, 0);
  reactive::bind_theme(body_label,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
}

}  // namespace screen
