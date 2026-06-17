/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "start_screen.h"

#include <cstdint>
#include <vector>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_MENU_WIDTH       = 300;
constexpr int32_t K_MENU_HEIGHT      = 106;
constexpr const char* K_MENU_ICONS[] = {
    view::ICON_FLASK,
    view::ICON_KEYBOARD,
    view::ICON_MONITOR,
    view::ICON_MICROPHONE,
    view::ICON_CAMERA,
    view::ICON_PLUGS_CONNECTED,
    view::ICON_LIGHTNING,
    view::ICON_VECTOR_THREE,
    view::ICON_INFO,
};

}  // namespace

StartScreen::StartScreen(viewmodel::AppViewModel& app_view_model,
                         viewmodel::StartMenuViewModel& menu_view_model,
                         app::AssetManager& assets)
    : BaseScreen(app_view_model, assets, nullptr, &menu_view_model),
      menu_view_model_(menu_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  init();
  platform::set_key_listener(key_listener, this);
}

StartScreen::~StartScreen() {
  platform::clear_key_listener(key_listener, this);
  if (selected_observer_handle_) {
    lv_observer_remove(selected_observer_handle_);
    selected_observer_handle_ = nullptr;
  }
  list_.reset();
}

void StartScreen::build_content(lv_obj_t* content) {
  auto* text_font = assets_ref_().load_font("inter-medium.ttf", 14);
  auto* icon_font = assets_ref_().load_font("Phosphor-Fill.ttf", 14);

  std::vector<view::widgets::IconList::Item> items;
  items.reserve(menu_view_model_.items().size());
  std::size_t index = 0;
  for (const auto& item : menu_view_model_.items()) {
    const auto* icon = index < (sizeof(K_MENU_ICONS) / sizeof(K_MENU_ICONS[0]))
                           ? K_MENU_ICONS[index]
                           : view::ICON_INFO;
    items.push_back({icon, item.title});
    ++index;
  }

  list_ = std::make_unique<view::widgets::IconList>(content,
                                                    app_view_model_ref_(),
                                                    items,
                                                    text_font ? text_font : &lv_font_montserrat_14,
                                                    icon_font ? icon_font : &lv_font_montserrat_14,
                                                    K_MENU_WIDTH,
                                                    K_MENU_HEIGHT,
                                                    item_clicked_cb,
                                                    this);
  list_->build();
  list_->set_focused(true);
  list_->set_selected_index(menu_view_model_.selected_index());

  selected_observer_handle_ = reactive::observe_obj(list_->root(),
                                                    menu_view_model_.selected_index_subject(),
                                                    selected_observer,
                                                    this);
}

void StartScreen::update_selection_(int32_t selected_index) {
  if (!list_) {
    return;
  }

  list_->set_selected_index(static_cast<std::size_t>(selected_index));
  if (!list_->is_focused()) {
    list_->set_focused(true);
  }
}

void StartScreen::activate_selected_item_() {
  const auto& item = menu_view_model_.selected_item();
  if (item.starts_sequence) {
    app_view_model_ref_().start_full_test_sequence();
    return;
  }

  app_view_model_ref_().show_single_test_page(item.target_page);
}

void StartScreen::key_listener(uint32_t key, const char* key_name, void* user_data) {
  LV_UNUSED(key_name);

  auto* page = static_cast<StartScreen*>(user_data);
  if (!page) {
    return;
  }

  if (page->list_ && !page->list_->is_focused()) {
    page->list_->set_focused(true);
  }

  switch (key) {
    case LV_KEY_UP:
    case 'f':
    case 'F':
      page->menu_view_model_.select_previous();
      break;
    case LV_KEY_DOWN:
    case 'x':
    case 'X':
      page->menu_view_model_.select_next();
      break;
    case LV_KEY_ENTER:
      page->activate_selected_item_();
      break;
    default:
      break;
  }
}

void StartScreen::item_clicked_cb(std::size_t index, void* user_data) {
  auto* page = static_cast<StartScreen*>(user_data);
  if (!page) {
    return;
  }

  page->menu_view_model_.set_selected_index(index);
  page->activate_selected_item_();
}

void StartScreen::selected_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<StartScreen*>(lv_observer_get_user_data(observer));
  if (page) {
    page->update_selection_(lv_subject_get_int(subject));
  }
}

}  // namespace screen
