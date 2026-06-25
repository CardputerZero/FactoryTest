/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <memory>

#include "base_screen.h"
#include "icon_list.h"
#include "popup.h"
#include "start_menu_viewmodel.h"

namespace screen {

class StartScreen : public BaseScreen {
 public:
  StartScreen(viewmodel::AppViewModel&       app_view_model,
              viewmodel::StartMenuViewModel& menu_view_model,
              app::AssetManager&             assets);
  ~StartScreen() override;

 protected:
  void        build_content(lv_obj_t* content) override;
  void        update_selection_(int32_t selected_index);
  void        activate_selected_item_();
  void        show_exit_popup_();
  void        hide_exit_popup_();
  static void key_listener(uint32_t key, const char* key_name, void* user_data);
  static void item_clicked_cb(std::size_t index, void* user_data);
  static void selected_observer(lv_observer_t* observer, lv_subject_t* subject);

 private:
  viewmodel::StartMenuViewModel&           menu_view_model_;
  std::unique_ptr<view::widgets::IconList> list_{};
  std::unique_ptr<view::widgets::Popup>    exit_popup_{};
  lv_observer_t*                           selected_observer_handle_{nullptr};
};

}  // namespace screen
