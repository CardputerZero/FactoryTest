/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_test_page.h"

#include <cstdint>
#include <vector>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_VIEWPORT_WIDTH  = view::K_SCREEN_WIDTH;
constexpr int32_t K_VIEWPORT_HEIGHT = 106;
constexpr int32_t K_MENU_WIDTH      = 300;
constexpr int32_t K_MENU_HEIGHT     = 106;

const char* icon_for_page(model::PerfSubPage page) {
  switch (page) {
    case model::PerfSubPage::CPU:
      return view::ICON_CPU;
    case model::PerfSubPage::MEM:
      return view::ICON_MEMORY;
    case model::PerfSubPage::SD:
      return view::ICON_HARDDRIVE;
    case model::PerfSubPage::MENU:
    default:
      return "";
  }
}

const char* title_for_page(model::PerfSubPage page) {
  switch (page) {
    case model::PerfSubPage::CPU:
      return "CPU Benchmark";
    case model::PerfSubPage::MEM:
      return "Memory Stress";
    case model::PerfSubPage::SD:
      return "SD Card Test";
    case model::PerfSubPage::MENU:
    default:
      return "Performance Test";
  }
}

}  // namespace

PerfTestPage::PerfTestPage(viewmodel::AppViewModel& app_view_model,
                           viewmodel::PerfTestViewModel& perf_view_model,
                           app::AssetManager& assets)
    : BaseScreen(app_view_model, assets),
      perf_view_model_(perf_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  if (!perf_view_model_.is_direct_subpage_active()) {
    perf_view_model_.show_menu();
  }
  set_default_test_nav_();
  app_view_model_ref_().set_back_request_handler(back_request_handler, this);
  init();
  platform::set_key_listener(key_listener, this);
}

PerfTestPage::~PerfTestPage() {
  app_view_model_ref_().clear_back_request_handler(back_request_handler, this);
  platform::clear_key_listener(key_listener, this);
  if (selected_observer_handle_) {
    lv_observer_remove(selected_observer_handle_);
    selected_observer_handle_ = nullptr;
  }
  if (active_page_observer_handle_) {
    lv_observer_remove(active_page_observer_handle_);
    active_page_observer_handle_ = nullptr;
  }
  menu_list_.reset();
  cpu_view_.reset();
  mem_view_.reset();
  sd_view_.reset();
  perf_view_model_.clear_direct_subpage();
}

void PerfTestPage::build_content(lv_obj_t* content) {
  const auto& items = perf_view_model_.items();

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

  auto* text_font = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"), 14);
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
  menu_list_->set_selected_index(perf_view_model_.selected_index());
  menu_list_->set_focused(perf_view_model_.is_menu_active());

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

  selected_observer_handle_    = reactive::observe_obj(menu_list_->root(),
                                                       perf_view_model_.selected_index_subject(),
                                                       selected_observer,
                                                       this);
  active_page_observer_handle_ = reactive::observe_obj(plane_,
                                                       perf_view_model_.active_page_subject(),
                                                       active_page_observer,
                                                       this);

  show_page_(perf_view_model_.active_page());
}

bool PerfTestPage::back_request_handler(void* user_data) {
  auto* page = static_cast<PerfTestPage*>(user_data);
  if (!page || page->app_view_model_ref_().current_page() != model::AppPage::PERF_TEST) {
    return false;
  }
  return page->perf_view_model_.request_back();
}

std::size_t PerfTestPage::viewport_index_(model::PerfSubPage page) const {
  switch (page) {
    case model::PerfSubPage::CPU:
      return 1;
    case model::PerfSubPage::MEM:
      return 2;
    case model::PerfSubPage::SD:
      return 3;
    case model::PerfSubPage::MENU:
    default:
      return 0;
  }
}

lv_obj_t* PerfTestPage::viewport_for_page_(model::PerfSubPage page) const {
  if (!plane_) {
    return nullptr;
  }
  return lv_obj_get_child(plane_, static_cast<int32_t>(viewport_index_(page)));
}

void PerfTestPage::reset_subpage_views_(model::PerfSubPage keep_page) {
  const auto reset_view = [this, keep_page](model::PerfSubPage page, auto& view) {
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

  reset_view(model::PerfSubPage::CPU, cpu_view_);
  reset_view(model::PerfSubPage::MEM, mem_view_);
  reset_view(model::PerfSubPage::SD, sd_view_);
}

void PerfTestPage::ensure_subpage_view_(model::PerfSubPage page) {
  if (page == model::PerfSubPage::MENU) {
    return;
  }

  auto* viewport = viewport_for_page_(page);
  if (!viewport) {
    return;
  }

  switch (page) {
    case model::PerfSubPage::CPU:
      if (!cpu_view_) {
        cpu_view_ = std::make_unique<PerfCpuView>();
        cpu_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      }
      break;
    case model::PerfSubPage::MEM:
      if (!mem_view_) {
        mem_view_ = std::make_unique<PerfMemView>();
        mem_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      }
      break;
    case model::PerfSubPage::SD:
      if (!sd_view_) {
        sd_view_ = std::make_unique<PerfSdView>();
        sd_view_->build(viewport, app_view_model_ref_(), assets_ref_());
      }
      break;
    case model::PerfSubPage::MENU:
    default:
      break;
  }
}

void PerfTestPage::show_page_(model::PerfSubPage page) {
  if (!plane_) {
    return;
  }
  reset_subpage_views_(page);
  ensure_subpage_view_(page);
  update_title_(page);
  update_nav_actions_();
  if (menu_list_) {
    menu_list_->set_focused(page == model::PerfSubPage::MENU);
  }
  const auto index = viewport_index_(page);
  lv_obj_set_x(plane_, -static_cast<int32_t>(index) * K_VIEWPORT_WIDTH);
}

void PerfTestPage::update_selection_(std::size_t index) {
  if (menu_list_) {
    menu_list_->set_selected_index(index);
  }
}

void PerfTestPage::update_nav_actions_() {
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() {
    app_view_model_ref_().request_back_or_quit();
  });
  set_nav_action_('8', view::ICON_CHECK_SQUARE, [this]() {
    show_test_result_dialog_();
  });
}

void PerfTestPage::update_title_(model::PerfSubPage page) {
  app_view_model_ref_().set_title_text(title_for_page(page));
}

void PerfTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<PerfTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  switch (key) {
    case LV_KEY_UP:
    case 'f':
    case 'F':
    case LV_KEY_LEFT:
      if (page->perf_view_model_.is_menu_active()) {
        page->perf_view_model_.select_previous();
      }
      break;
    case LV_KEY_DOWN:
    case 'x':
    case 'X':
    case LV_KEY_RIGHT:
      if (page->perf_view_model_.is_menu_active()) {
        page->perf_view_model_.select_next();
      }
      break;
    case LV_KEY_ENTER:
    case '6':
      page->perf_view_model_.activate_selected();
      break;
    default:
      break;
  }
}

void PerfTestPage::menu_item_clicked(std::size_t index, void* user_data) {
  auto* page = static_cast<PerfTestPage*>(user_data);
  if (!page) {
    return;
  }
  page->perf_view_model_.set_selected_index(index);
  page->perf_view_model_.activate_selected();
}

void PerfTestPage::selected_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<PerfTestPage*>(lv_observer_get_user_data(observer));
  if (page) {
    page->update_selection_(static_cast<std::size_t>(lv_subject_get_int(subject)));
  }
}

void PerfTestPage::active_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<PerfTestPage*>(lv_observer_get_user_data(observer));
  if (page) {
    page->show_page_(static_cast<model::PerfSubPage>(lv_subject_get_int(subject)));
  }
}

}  // namespace screen
