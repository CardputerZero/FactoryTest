/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base_screen.h"
#include "connectivity_test_viewmodel.h"
#include "icon_list.h"
#include "perf_test_viewmodel.h"
#include "popup.h"
#include "start_menu_viewmodel.h"

namespace screen {

class StartScreen : public BaseScreen {
 public:
  StartScreen(viewmodel::AppViewModel& app_view_model,
              viewmodel::StartMenuViewModel& menu_view_model,
              viewmodel::PerfTestViewModel& perf_view_model,
              viewmodel::ConnectivityTestViewModel& connectivity_view_model,
              app::AssetManager& assets);
  ~StartScreen() override;

 protected:
  void build_content(lv_obj_t* content) override;
  void update_selection_(int32_t selected_index);
  void update_category_(int32_t selected_category_index);
  void activate_selected_item_();
  void show_exit_popup_();
  void hide_exit_popup_();
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void item_clicked_cb(std::size_t index, void* user_data);
  static void tab_clicked_cb(lv_event_t* event);
  static void selected_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void category_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void theme_observer(lv_observer_t* observer, lv_subject_t* subject);
  static void width_anim_cb(void* obj, int32_t value);
  static void x_anim_cb(void* obj, int32_t value);
  static void list_width_anim_cb(void* obj, int32_t value);
  static void tab_fill_width_anim_cb(void* obj, int32_t value);
  static void tab_fill_completed_cb(lv_anim_t* anim);
  static void tab_scale_anim_cb(void* obj, int32_t value);

 private:
  enum class FocusArea {
    LIST,
    DRAWER,
  };

  struct DrawerTab {
    model::StartMenuCategory category;
    lv_obj_t* button{nullptr};
    lv_obj_t* fill{nullptr};
    lv_obj_t* label{nullptr};
  };

  std::vector<view::widgets::IconList::Item> current_list_items_() const;
  void rebuild_list_();
  void build_drawer_(lv_obj_t* content);
  void set_drawer_open_(bool open, bool animate);
  void toggle_drawer_(bool animate);
  void set_focus_area_(FocusArea area);
  void apply_screen_theme_(bool dark_mode);
  void apply_content_theme_(bool dark_mode);
  void apply_drawer_theme_(bool dark_mode);
  void apply_tab_style_(DrawerTab& tab, bool dark_mode);
  void animate_obj_width_(lv_obj_t* obj, int32_t from, int32_t to);
  void animate_obj_x_(lv_obj_t* obj, int32_t from, int32_t to);
  void animate_list_width_(int32_t from, int32_t to);
  void animate_tab_fill_width_(lv_obj_t* fill, int32_t from, int32_t to);
  static void animate_tab_pulse_(lv_obj_t* button);

  viewmodel::StartMenuViewModel& menu_view_model_;
  viewmodel::PerfTestViewModel& perf_view_model_;
  viewmodel::ConnectivityTestViewModel& connectivity_view_model_;
  std::unique_ptr<view::widgets::IconList> list_{};
  std::unique_ptr<view::widgets::Popup> exit_popup_{};
  lv_obj_t* drawer_{nullptr};
  lv_obj_t* list_viewport_{nullptr};
  std::array<DrawerTab, model::StartMenuModel::K_CATEGORY_COUNT> tabs_{};
  FocusArea focus_area_{FocusArea::LIST};
  bool drawer_open_{true};
  int32_t rendered_category_index_{-1};
  lv_observer_t* selected_observer_handle_{nullptr};
  lv_observer_t* category_observer_handle_{nullptr};
  lv_observer_t* theme_observer_handle_{nullptr};
};

}  // namespace screen
