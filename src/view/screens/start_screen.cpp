/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "start_screen.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int32_t K_DRAWER_WIDTH        = 125;
constexpr int32_t K_MENU_HEIGHT         = 106;
constexpr int32_t K_LIST_CLOSED_WIDTH   = 280;
constexpr int32_t K_LIST_OPEN_WIDTH     = view::K_SCREEN_WIDTH - K_DRAWER_WIDTH - 10;
constexpr int32_t K_TAB_WIDTH           = 112;
constexpr int32_t K_TAB_HEIGHT          = 26;
constexpr int32_t K_DRAWER_ANIM_MS      = 180;
constexpr int32_t K_TAB_FILL_ANIM_MS    = 190;
constexpr int32_t K_TAB_FILL_WIDTH      = K_TAB_WIDTH;
constexpr int32_t K_TAB_FILL_HEIGHT     = K_TAB_HEIGHT;
constexpr int32_t K_TAB_PULSE_SCALE     = LV_SCALE_NONE + 12;
constexpr int32_t K_TAB_PULSE_ANIM_MS   = 90;
constexpr const char* K_CATEGORY_TEXT[] = {"BASIC", "PERIPH", "COMMS"};

int32_t list_width_for_drawer_state(bool drawer_open) {
  return drawer_open ? K_LIST_OPEN_WIDTH : K_LIST_CLOSED_WIDTH;
}

const char* category_text(model::StartMenuCategory category) {
  const auto index = static_cast<std::size_t>(category);
  return index < (sizeof(K_CATEGORY_TEXT) / sizeof(K_CATEGORY_TEXT[0])) ? K_CATEGORY_TEXT[index]
                                                                        : "Tab";
}

lv_color_t category_color(model::StartMenuCategory category, const view::ThemePalette& colors) {
  switch (category) {
    case model::StartMenuCategory::PERIPH:
      return colors.warning;
    case model::StartMenuCategory::COMMS:
      return colors.info;
    case model::StartMenuCategory::AUTO:
    default:
      return colors.primary;
  }
}

bool is_tab_key(uint32_t key) {
  bool is_tab = key == '\t';
#ifdef LV_KEY_NEXT
  is_tab = is_tab || key == LV_KEY_NEXT;
#endif
  return is_tab;
}

bool is_drawer_toggle_key(uint32_t key) { return key == '6' || is_tab_key(key); }

bool is_exit_key(uint32_t key) { return key == LV_KEY_ESC || key == '4'; }

enum class FtlSequenceKey {
  UP,
  DOWN,
  LEFT,
  RIGHT,
  B,
  A,
};

constexpr std::array<FtlSequenceKey, 12> K_FTL_EASTER_EGG_SEQUENCE = {
    FtlSequenceKey::UP,
    FtlSequenceKey::UP,
    FtlSequenceKey::DOWN,
    FtlSequenceKey::DOWN,
    FtlSequenceKey::LEFT,
    FtlSequenceKey::LEFT,
    FtlSequenceKey::RIGHT,
    FtlSequenceKey::RIGHT,
    FtlSequenceKey::B,
    FtlSequenceKey::A,
    FtlSequenceKey::B,
    FtlSequenceKey::A,
};

bool key_matches_ftl_sequence(uint32_t key, FtlSequenceKey sequence_key) {
  switch (sequence_key) {
    case FtlSequenceKey::UP:
      return key == LV_KEY_UP || key == 'f' || key == 'F';
    case FtlSequenceKey::DOWN:
      return key == LV_KEY_DOWN || key == 'x' || key == 'X';
    case FtlSequenceKey::LEFT:
      return key == LV_KEY_LEFT || key == 'z' || key == 'Z';
    case FtlSequenceKey::RIGHT:
      return key == LV_KEY_RIGHT || key == 'c' || key == 'C';
    case FtlSequenceKey::B:
      return key == 'b' || key == 'B';
    case FtlSequenceKey::A:
      return key == 'a' || key == 'A';
  }
  return false;
}

}  // namespace

StartScreen::StartScreen(viewmodel::AppViewModel& app_view_model,
                         viewmodel::StartMenuViewModel& menu_view_model,
                         viewmodel::PerfTestViewModel& perf_view_model,
                         viewmodel::ConnectivityTestViewModel& connectivity_view_model,
                         app::AssetManager& assets)
    : BaseScreen(app_view_model, assets),
      menu_view_model_(menu_view_model),
      perf_view_model_(perf_view_model),
      connectivity_view_model_(connectivity_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  drawer_open_ = !menu_view_model_.is_drawer_hidden();
  set_nav_action_(
      '4',
      view::ICON_SIGN_OUT,
      [this]() {
        hide_exit_popup_();
        app_view_model_ref_().request_quit();
      },
      LV_EVENT_LONG_PRESSED,
      viewmodel::NavHoldTarget::ERROR);
  viewmodel::NavAction exit_action = app_view_model_ref_().nav_actions()[0];
  exit_action.press_action         = [this]() { show_exit_popup_(); };
  exit_action.release_action       = [this]() { hide_exit_popup_(); };
  app_view_model_ref_().set_keypad_nav_action('4', std::move(exit_action));
  set_nav_action_('6',
                  view::ICON_SIDEBAR,
                  nullptr,
                  LV_EVENT_CLICKED,
                  viewmodel::NavHoldTarget::NONE,
                  true);
  set_nav_action_('8',
                  view::ICON_CHECK_SQUARE,
                  nullptr,
                  LV_EVENT_CLICKED,
                  viewmodel::NavHoldTarget::NONE,
                  true);
  init();
  platform::set_key_listener(key_listener, this);
  platform::set_key_release_listener(key_release_listener, this);
  platform::set_long_key_listener(long_key_listener, this);
}

StartScreen::~StartScreen() {
  platform::clear_long_key_listener(long_key_listener, this);
  platform::clear_key_release_listener(key_release_listener, this);
  platform::clear_key_listener(key_listener, this);
  lv_anim_delete(this, list_width_anim_cb);
  for (auto& tab : tabs_) {
    if (tab.fill) {
      lv_anim_delete(tab.fill, tab_fill_width_anim_cb);
    }
    if (tab.button) {
      lv_anim_delete(tab.button, tab_scale_anim_cb);
    }
  }
  if (selected_observer_handle_) {
    lv_observer_remove(selected_observer_handle_);
    selected_observer_handle_ = nullptr;
  }
  if (category_observer_handle_) {
    lv_observer_remove(category_observer_handle_);
    category_observer_handle_ = nullptr;
  }
  if (theme_observer_handle_) {
    lv_observer_remove(theme_observer_handle_);
    theme_observer_handle_ = nullptr;
  }
  if (language_observer_handle_) {
    lv_observer_remove(language_observer_handle_);
    language_observer_handle_ = nullptr;
  }
  language_dialog_.reset();
  exit_popup_.reset();
  list_.reset();
}

void StartScreen::build_content(lv_obj_t* content) {
  build_drawer_(content);

  const int32_t content_x = drawer_open_ ? K_DRAWER_WIDTH : 0;
  list_viewport_          = lv_obj_create(content);
  lv_obj_remove_style_all(list_viewport_);
  lv_obj_set_size(list_viewport_, view::K_SCREEN_WIDTH - content_x, K_MENU_HEIGHT);
  lv_obj_align(list_viewport_, LV_ALIGN_TOP_LEFT, content_x, 0);
  lv_obj_clear_flag(list_viewport_, LV_OBJ_FLAG_SCROLLABLE);
  reactive::bind_theme(list_viewport_,
                       app_view_model_ref_().dark_mode_subject(),
                       reactive::ThemeRole::SURFACE);

  rebuild_list_();
  apply_content_theme_(app_view_model_ref_().is_dark_mode());

  selected_observer_handle_ = reactive::observe_obj(content,
                                                    menu_view_model_.selected_index_subject(),
                                                    selected_observer,
                                                    this);
  category_observer_handle_ = reactive::observe_obj(content,
                                                    menu_view_model_.selected_category_subject(),
                                                    category_observer,
                                                    this);
  theme_observer_handle_    = reactive::observe_obj(content,
                                                    app_view_model_ref_().dark_mode_subject(),
                                                    theme_observer,
                                                    this);
  language_observer_handle_ = reactive::observe_obj(content,
                                                    app_view_model_ref_().language_subject(),
                                                    language_observer,
                                                    this);
  set_focus_area_(FocusArea::LIST);
}

std::vector<view::widgets::IconList::Item> StartScreen::current_list_items_() {
  std::vector<view::widgets::IconList::Item> items;
  const auto category = menu_view_model_.selected_category();
  const auto count    = menu_view_model_.item_count_for_category(category);
  items.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const auto* item = menu_view_model_.item_for_category(category, i);
    if (item) {
      items.push_back({item->icon, app_view_model_ref_().tr(item->title)});
    }
  }
  return items;
}

void StartScreen::rebuild_list_() {
  if (!list_viewport_) {
    return;
  }

  list_.reset();

  auto* text_font = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"),
                                            14);
  auto* icon_font = assets_ref_().load_font("Phosphor-Fill.ttf", 14);
  auto items      = current_list_items_();
  list_ = std::make_unique<view::widgets::IconList>(list_viewport_,
                                                    app_view_model_ref_(),
                                                    items,
                                                    text_font ? text_font : &lv_font_montserrat_14,
                                                    icon_font ? icon_font : &lv_font_montserrat_14,
                                                    list_width_for_drawer_state(drawer_open_),
                                                    K_MENU_HEIGHT,
                                                    item_clicked_cb,
                                                    this);
  list_->build();
  list_->set_selected_index(menu_view_model_.selected_index());
  list_->set_focused(focus_area_ == FocusArea::LIST);
  rendered_category_index_ = static_cast<int32_t>(menu_view_model_.selected_category_index());
}

void StartScreen::build_drawer_(lv_obj_t* content) {
  drawer_ = lv_obj_create(content);
  lv_obj_remove_style_all(drawer_);
  lv_obj_set_size(drawer_, drawer_open_ ? K_DRAWER_WIDTH : 0, K_MENU_HEIGHT);
  lv_obj_align(drawer_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_flex_flow(drawer_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(drawer_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(drawer_, 4, 0);
  lv_obj_clear_flag(drawer_, LV_OBJ_FLAG_SCROLLABLE);

  const std::array<model::StartMenuCategory, model::StartMenuModel::K_CATEGORY_COUNT> categories = {
      model::StartMenuCategory::AUTO,
      model::StartMenuCategory::PERIPH,
      model::StartMenuCategory::COMMS,
  };

  auto* tab_font = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-bold.ttf"),
                                           12);
  for (std::size_t i = 0; i < tabs_.size(); ++i) {
    tabs_[i].category = categories[i];
    tabs_[i].button   = lv_button_create(drawer_);
    lv_obj_remove_style_all(tabs_[i].button);
    lv_obj_set_size(tabs_[i].button, K_TAB_WIDTH, K_TAB_HEIGHT);
    lv_obj_set_style_radius(tabs_[i].button, 6, 0);
    lv_obj_set_style_clip_corner(tabs_[i].button, true, 0);
    lv_obj_set_style_transform_pivot_x(tabs_[i].button, K_TAB_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(tabs_[i].button, K_TAB_HEIGHT / 2, 0);
    lv_obj_clear_flag(tabs_[i].button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tabs_[i].button, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(tabs_[i].button, tab_clicked_cb, LV_EVENT_CLICKED, this);

    tabs_[i].fill = lv_obj_create(tabs_[i].button);
    lv_obj_remove_style_all(tabs_[i].fill);
    lv_obj_set_size(tabs_[i].fill, 0, K_TAB_FILL_HEIGHT);
    lv_obj_set_style_radius(tabs_[i].fill, 6, 0);
    lv_obj_set_style_bg_opa(tabs_[i].fill, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tabs_[i].fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(tabs_[i].fill);

    tabs_[i].label = lv_label_create(tabs_[i].button);
    const auto translated = app_view_model_ref_().tr(category_text(categories[i]));
    lv_label_set_text(tabs_[i].label, translated.c_str());
    lv_obj_center(tabs_[i].label);
    if (tab_font) {
      lv_obj_set_style_text_font(tabs_[i].label, tab_font, 0);
    }
  }

  apply_drawer_theme_(app_view_model_ref_().is_dark_mode());
  if (!drawer_open_) {
    lv_obj_add_flag(drawer_, LV_OBJ_FLAG_HIDDEN);
  }
}

void StartScreen::set_drawer_open_(bool open, bool animate) {
  if (drawer_open_ == open && drawer_) {
    return;
  }

  drawer_open_ = open;
  menu_view_model_.set_drawer_hidden(!open);
  if (!drawer_ || !list_viewport_) {
    return;
  }

  const int32_t target_drawer_width = open ? K_DRAWER_WIDTH : 0;
  const int32_t target_content_x    = open ? K_DRAWER_WIDTH : 0;
  const int32_t target_content_w    = view::K_SCREEN_WIDTH - target_content_x;
  const int32_t target_list_w       = list_width_for_drawer_state(open);

  lv_obj_clear_flag(drawer_, LV_OBJ_FLAG_HIDDEN);
  if (!animate) {
    lv_obj_set_width(drawer_, target_drawer_width);
    lv_obj_set_x(list_viewport_, target_content_x);
    lv_obj_set_width(list_viewport_, target_content_w);
    if (list_) {
      list_->set_width(target_list_w);
    }
    if (!open) {
      lv_obj_add_flag(drawer_, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  lv_anim_delete(drawer_, width_anim_cb);
  lv_anim_delete(list_viewport_, width_anim_cb);
  lv_anim_delete(list_viewport_, x_anim_cb);
  lv_anim_delete(this, list_width_anim_cb);
  animate_obj_width_(drawer_, lv_obj_get_width(drawer_), target_drawer_width);
  animate_obj_x_(list_viewport_, lv_obj_get_x(list_viewport_), target_content_x);
  animate_obj_width_(list_viewport_, lv_obj_get_width(list_viewport_), target_content_w);
  animate_list_width_(list_ ? list_->width() : target_list_w, target_list_w);
  if (!open) {
    set_focus_area_(FocusArea::LIST);
  }
}

void StartScreen::toggle_drawer_(bool animate) {
  set_drawer_open_(!drawer_open_, animate);
  if (drawer_open_) {
    set_focus_area_(FocusArea::DRAWER);
  }
}

void StartScreen::set_focus_area_(FocusArea area) {
  focus_area_ = drawer_open_ ? area : FocusArea::LIST;
  if (list_) {
    list_->set_focused(focus_area_ == FocusArea::LIST);
  }
  apply_drawer_theme_(app_view_model_ref_().is_dark_mode());
}

bool StartScreen::update_ftl_easter_egg_(uint32_t key) {
  if (key_matches_ftl_sequence(key, K_FTL_EASTER_EGG_SEQUENCE[ftl_sequence_index_])) {
    ++ftl_sequence_index_;
    if (ftl_sequence_index_ >= K_FTL_EASTER_EGG_SEQUENCE.size()) {
      ftl_sequence_index_ = 0;
      app_view_model_ref_().request_ftl_page();
      return true;
    }
    return false;
  }

  ftl_sequence_index_ = key_matches_ftl_sequence(key, K_FTL_EASTER_EGG_SEQUENCE[0]) ? 1 : 0;
  return false;
}

void StartScreen::apply_screen_theme_(bool dark_mode) {
  apply_content_theme_(dark_mode);
  apply_drawer_theme_(dark_mode);
}

void StartScreen::apply_content_theme_(bool dark_mode) {
  if (!list_viewport_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  lv_obj_set_style_bg_opa(list_viewport_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(list_viewport_, colors.surface, 0);
  lv_obj_set_style_text_color(list_viewport_, colors.text, 0);
  if (list_) {
    list_->refresh_theme();
  }
}

void StartScreen::apply_drawer_theme_(bool dark_mode) {
  if (!drawer_) {
    return;
  }

  const auto colors = view::palette(dark_mode);
  lv_obj_set_style_bg_opa(drawer_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(drawer_, colors.surface, 0);
  lv_obj_set_style_border_width(drawer_, 1, 0);
  lv_obj_set_style_border_side(drawer_, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_border_color(drawer_, colors.border, 0);

  for (auto& tab : tabs_) {
    apply_tab_style_(tab, dark_mode);
  }
}

void StartScreen::apply_tab_style_(DrawerTab& tab, bool dark_mode) {
  if (!tab.button || !tab.fill || !tab.label) {
    return;
  }

  const auto colors           = view::palette(dark_mode);
  const bool selected         = tab.category == menu_view_model_.selected_category();
  const bool focused_selected = selected && focus_area_ == FocusArea::DRAWER;
  const auto accent_color     = category_color(tab.category, colors);
  const auto selected_bg      = lv_color_mix(accent_color, colors.surface, dark_mode ? 172 : 204);
  const auto idle_bg          = lv_color_mix(colors.button, colors.surface, dark_mode ? 128 : 76);
  const auto idle_border      = lv_color_mix(colors.border, colors.surface, dark_mode ? 152 : 96);
  const auto selected_border =
      focused_selected ? lv_color_mix(colors.text, accent_color, dark_mode ? 88 : 64)
                       : lv_color_mix(accent_color, colors.border, dark_mode ? 184 : 164);
  const int32_t current_fill_w = lv_obj_get_width(tab.fill);
  const int32_t target_fill_w  = selected ? K_TAB_FILL_WIDTH : 0;

  lv_obj_set_style_bg_opa(tab.button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(tab.button, idle_bg, 0);
  lv_obj_set_style_border_width(tab.button, 1, 0);
  lv_obj_set_style_border_color(tab.button, selected ? selected_border : idle_border, 0);
  lv_obj_set_style_outline_width(tab.button, focused_selected ? 2 : 0, 0);
  lv_obj_set_style_outline_pad(tab.button, 0, 0);
  lv_obj_set_style_outline_opa(tab.button, LV_OPA_COVER, 0);
  lv_obj_set_style_outline_color(tab.button, selected_border, 0);
  lv_obj_set_style_bg_color(tab.fill, selected_bg, 0);
  if (!focused_selected) {
    lv_anim_delete(tab.button, tab_scale_anim_cb);
    lv_obj_set_style_transform_scale(tab.button, LV_SCALE_NONE, 0);
  }
  if (current_fill_w != target_fill_w) {
    animate_tab_fill_width_(tab.fill, current_fill_w, target_fill_w);
  } else {
    lv_obj_set_width(tab.fill, target_fill_w);
    lv_obj_center(tab.fill);
  }
  lv_obj_set_style_text_color(tab.label, selected ? lv_color_white() : colors.text_muted, 0);
  lv_obj_move_foreground(tab.label);
}

void StartScreen::animate_obj_width_(lv_obj_t* obj, int32_t from, int32_t to) {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, obj);
  lv_anim_set_values(&anim, from, to);
  lv_anim_set_duration(&anim, K_DRAWER_ANIM_MS);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim, width_anim_cb);
  lv_anim_start(&anim);
}

void StartScreen::animate_obj_x_(lv_obj_t* obj, int32_t from, int32_t to) {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, obj);
  lv_anim_set_values(&anim, from, to);
  lv_anim_set_duration(&anim, K_DRAWER_ANIM_MS);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim, x_anim_cb);
  lv_anim_start(&anim);
}

void StartScreen::animate_list_width_(int32_t from, int32_t to) {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, this);
  lv_anim_set_values(&anim, from, to);
  lv_anim_set_duration(&anim, K_DRAWER_ANIM_MS);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim, list_width_anim_cb);
  lv_anim_start(&anim);
}

void StartScreen::animate_tab_fill_width_(lv_obj_t* fill, int32_t from, int32_t to) {
  if (!fill) {
    return;
  }

  lv_anim_delete(fill, tab_fill_width_anim_cb);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, fill);
  lv_anim_set_values(&anim, from, to);
  lv_anim_set_duration(&anim, K_TAB_FILL_ANIM_MS);
  lv_anim_set_path_cb(&anim, to > from ? lv_anim_path_overshoot : lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&anim, tab_fill_width_anim_cb);
  if (to > from) {
    lv_anim_set_completed_cb(&anim, tab_fill_completed_cb);
  }
  lv_anim_start(&anim);
}

void StartScreen::animate_tab_pulse(lv_obj_t* button) {
  if (!button || !lv_obj_is_valid(button)) {
    return;
  }

  lv_anim_delete(button, tab_scale_anim_cb);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, button);
  lv_anim_set_values(&anim, LV_SCALE_NONE, K_TAB_PULSE_SCALE);
  lv_anim_set_duration(&anim, K_TAB_PULSE_ANIM_MS);
  lv_anim_set_reverse_duration(&anim, K_TAB_PULSE_ANIM_MS);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim, tab_scale_anim_cb);
  lv_anim_start(&anim);
}

void StartScreen::update_selection_(int32_t selected_index) {
  if (!list_) {
    return;
  }

  list_->set_selected_index(static_cast<std::size_t>(std::max(selected_index, 0)));
}

void StartScreen::update_category_(int32_t selected_category_index) {
  if (selected_category_index == rendered_category_index_) {
    return;
  }
  rebuild_list_();
  apply_drawer_theme_(app_view_model_ref_().is_dark_mode());
}

void StartScreen::activate_selected_item_() {
  const auto& item = menu_view_model_.selected_item();
  if (std::strcmp(item.title, "Language") == 0) {
    show_language_dialog_();
    return;
  }

  if (item.starts_sequence) {
    perf_view_model_.clear_direct_subpage();
    connectivity_view_model_.clear_direct_subpage();
    app_view_model_ref_().start_full_test_sequence();
    return;
  }

  perf_view_model_.clear_direct_subpage();
  connectivity_view_model_.clear_direct_subpage();
  app_view_model_ref_().show_single_test_page(item.target_page);
}

void StartScreen::show_language_dialog_() {
  if (!root()) {
    return;
  }
  if (language_dialog_ && language_dialog_->visible()) {
    return;
  }

  platform::set_modal_key_capture(true);
  view::widgets::DialogConfig config;
  config.width                 = 250;
  config.height                = 136;
  config.title                 = "Language";
  config.shortcut_text         = "ESC: Cancel  OK: Confirm";
  config.ok_button_label       = "OK";
  config.cancel_button_label   = "Cancel";
  config.button_width          = 76;
  config.button_row_width      = 174;
  config.button_bottom_pad     = 4;
  config.body_font_size        = 12;
  config.use_nav_action_keys   = false;

  view::widgets::DialogCallbacks callbacks;
  callbacks.ok_action = [this]() {
    const auto selected = language_dropdown_ ? lv_dropdown_get_selected(language_dropdown_) : 0;
    const char* locale  = selected == 1 ? "zh_CN" : "en";
    close_language_dialog_();
    app_view_model_ref_().set_language(locale);
  };
  callbacks.cancel_action = [this]() { close_language_dialog_(); };

  language_dialog_ = std::make_unique<view::widgets::Dialog>(root(),
                                                             app_view_model_ref_(),
                                                             assets_ref_(),
                                                             config,
                                                             callbacks);
  language_dialog_->build();

  const auto options = app_view_model_ref_().tr("English") + "\n" +
                       app_view_model_ref_().tr("Chinese");
  const uint32_t selected = app_view_model_ref_().language() == "zh_CN" ? 1 : 0;
  language_dropdown_ = language_dialog_->add_dropdown(options.c_str(), selected, 176);
}

void StartScreen::close_language_dialog_() {
  if (language_dialog_) {
    language_dialog_->close();
  }
  language_dropdown_ = nullptr;
  platform::set_modal_key_capture(false);
}

bool StartScreen::handle_language_dialog_key_(uint32_t key, const char* key_name) {
  if (!language_dialog_ || !language_dialog_->visible()) {
    return false;
  }

  if (language_dropdown_ &&
      (key == LV_KEY_UP || key == 'f' || key == 'F' || key == LV_KEY_DOWN || key == 'x' ||
       key == 'X')) {
    const auto count = lv_dropdown_get_option_count(language_dropdown_);
    if (count > 0) {
      auto selected = lv_dropdown_get_selected(language_dropdown_);
      if (key == LV_KEY_UP || key == 'f' || key == 'F') {
        selected = selected == 0 ? count - 1 : selected - 1;
      } else {
        selected = selected + 1 >= count ? 0 : selected + 1;
      }
      lv_dropdown_set_selected(language_dropdown_, selected);
    }
    return true;
  }

  language_dialog_->handle_key(key, key_name);
  return true;
}

void StartScreen::refresh_language_() {
  exit_popup_.reset();

  if (list_) {
    rebuild_list_();
  }

  for (auto& tab : tabs_) {
    if (!tab.label) {
      continue;
    }
    const auto translated = app_view_model_ref_().tr(category_text(tab.category));
    lv_label_set_text(tab.label, translated.c_str());
    auto* tab_font = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-bold.ttf"),
                                             12);
    if (tab_font) {
      lv_obj_set_style_text_font(tab.label, tab_font, 0);
    }
    lv_obj_center(tab.label);
  }
  apply_drawer_theme_(app_view_model_ref_().is_dark_mode());
}

void StartScreen::show_exit_popup_() {
  if (!root()) {
    return;
  }
  if (!exit_popup_) {
    view::widgets::PopupConfig config;
    config.message = app_view_model_ref_().tr("Hold ESC/4 to Exit");
    config.font    = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-medium.ttf"),
                                             14);
    config.tone    = view::widgets::PopupTone::ERROR;
    exit_popup_    = std::make_unique<view::widgets::Popup>(root(), app_view_model_ref_(), config);
    exit_popup_->build();
  }
  exit_popup_->show();
}

void StartScreen::hide_exit_popup_() {
  if (exit_popup_) {
    exit_popup_->hide();
  }
}

void StartScreen::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<StartScreen*>(user_data);
  if (!page) {
    return;
  }

  if (page->handle_language_dialog_key_(key, key_name)) {
    return;
  }

  if (page->update_ftl_easter_egg_(key)) {
    return;
  }

  if (is_exit_key(key)) {
    page->show_exit_popup_();
    return;
  }

  if (is_drawer_toggle_key(key)) {
    page->toggle_drawer_(true);
    return;
  }

  switch (key) {
    case LV_KEY_LEFT:
    case 'z':
    case 'Z':
      if (page->drawer_open_) {
        page->set_focus_area_(FocusArea::DRAWER);
      }
      break;
    case LV_KEY_RIGHT:
    case 'c':
    case 'C':
      page->set_focus_area_(FocusArea::LIST);
      break;
    case LV_KEY_UP:
    case 'f':
    case 'F':
      if (page->focus_area_ == FocusArea::DRAWER) {
        page->menu_view_model_.select_previous_category();
      } else {
        page->menu_view_model_.select_previous();
      }
      break;
    case LV_KEY_DOWN:
    case 'x':
    case 'X':
      if (page->focus_area_ == FocusArea::DRAWER) {
        page->menu_view_model_.select_next_category();
      } else {
        page->menu_view_model_.select_next();
      }
      break;
    case LV_KEY_ENTER:
    case '8':
      if (page->focus_area_ == FocusArea::DRAWER) {
        page->set_focus_area_(FocusArea::LIST);
      } else {
        page->activate_selected_item_();
      }
      break;
    default:
      break;
  }
}

void StartScreen::key_release_listener(uint32_t key, const char* key_name, void* user_data) {
  LV_UNUSED(key_name);

  auto* page = static_cast<StartScreen*>(user_data);
  if (page && is_exit_key(key)) {
    page->hide_exit_popup_();
  }
}

void StartScreen::long_key_listener(uint32_t key, const char* key_name, void* user_data) {
  LV_UNUSED(key_name);

  auto* page = static_cast<StartScreen*>(user_data);
  if (page && is_exit_key(key)) {
    page->hide_exit_popup_();
    page->app_view_model_ref_().request_quit();
  }
}

void StartScreen::item_clicked_cb(std::size_t index, void* user_data) {
  auto* page = static_cast<StartScreen*>(user_data);
  if (!page) {
    return;
  }

  page->set_focus_area_(FocusArea::LIST);
  page->menu_view_model_.set_selected_index(index);
  page->activate_selected_item_();
}

void StartScreen::tab_clicked_cb(lv_event_t* event) {
  auto* page   = static_cast<StartScreen*>(lv_event_get_user_data(event));
  auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (!page || !target) {
    return;
  }

  for (const auto& tab : page->tabs_) {
    if (tab.button == target) {
      page->menu_view_model_.set_selected_category(tab.category);
      page->set_focus_area_(FocusArea::DRAWER);
      return;
    }
  }
}

void StartScreen::selected_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<StartScreen*>(lv_observer_get_user_data(observer));
  if (page) {
    page->update_selection_(lv_subject_get_int(subject));
  }
}

void StartScreen::category_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<StartScreen*>(lv_observer_get_user_data(observer));
  if (page) {
    page->update_category_(lv_subject_get_int(subject));
  }
}

void StartScreen::theme_observer(lv_observer_t* observer, lv_subject_t* subject) {
  auto* page = static_cast<StartScreen*>(lv_observer_get_user_data(observer));
  if (page) {
    page->apply_screen_theme_(lv_subject_get_int(subject) != 0);
  }
}

void StartScreen::language_observer(lv_observer_t* observer, lv_subject_t* subject) {
  LV_UNUSED(subject);

  auto* page = static_cast<StartScreen*>(lv_observer_get_user_data(observer));
  if (page) {
    page->refresh_language_();
  }
}

void StartScreen::width_anim_cb(void* obj, int32_t value) {
  auto* lv_obj = static_cast<lv_obj_t*>(obj);
  if (lv_obj && lv_obj_is_valid(lv_obj)) {
    lv_obj_set_width(lv_obj, value);
  }
}

void StartScreen::x_anim_cb(void* obj, int32_t value) {
  auto* lv_obj = static_cast<lv_obj_t*>(obj);
  if (lv_obj && lv_obj_is_valid(lv_obj)) {
    lv_obj_set_x(lv_obj, value);
  }
}

void StartScreen::list_width_anim_cb(void* obj, int32_t value) {
  auto* page = static_cast<StartScreen*>(obj);
  if (page && page->list_) {
    page->list_->set_width(value);
  }
}

void StartScreen::tab_fill_width_anim_cb(void* obj, int32_t value) {
  auto* fill = static_cast<lv_obj_t*>(obj);
  if (fill && lv_obj_is_valid(fill)) {
    lv_obj_set_width(fill, std::clamp(value, 0, K_TAB_FILL_WIDTH));
    lv_obj_center(fill);
  }
}

void StartScreen::tab_fill_completed_cb(lv_anim_t* anim) {
  auto* fill = anim ? static_cast<lv_obj_t*>(anim->var) : nullptr;
  if (!fill || !lv_obj_is_valid(fill) || lv_obj_get_width(fill) < K_TAB_FILL_WIDTH) {
    return;
  }

  auto* button = lv_obj_get_parent(fill);
  if (button && lv_obj_get_style_outline_width(button, LV_PART_MAIN) > 0) {
    animate_tab_pulse(button);
  }
}

void StartScreen::tab_scale_anim_cb(void* obj, int32_t value) {
  auto* button = static_cast<lv_obj_t*>(obj);
  if (button && lv_obj_is_valid(button)) {
    lv_obj_set_style_transform_scale(button, value, 0);
  }
}

}  // namespace screen
