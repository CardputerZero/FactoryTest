/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "base_widget.h"
#include "lvgl.h"

namespace view::widgets {

enum class DialogButtonTone {
  DEFAULT,
  SUCCESS,
  WARNING,
  ERROR,
};

struct DialogConfig {
  int32_t width{286};
  int32_t height{150};
  lv_align_t align{LV_ALIGN_CENTER};
  int32_t offset_x{0};
  int32_t offset_y{0};
  bool scroll_y{false};
  bool show_title{true};
  std::string title{};
  bool show_shortcuts{true};
  std::string shortcut_text{"ESC: Cancel  OK: Confirm"};
  bool show_ok_button{true};
  bool show_skip_button{false};
  bool show_cancel_button{true};
  std::string ok_button_label{"OK"};
  std::string skip_button_label{"Skip"};
  std::string cancel_button_label{"Cancel"};
  int32_t pad_all{8};
  int32_t pad_row{4};
  int32_t body_offset_y{0};
  int32_t shortcut_width{0};
  int32_t button_width{82};
  int32_t button_height{26};
  int32_t button_row_width{214};
  int32_t button_bottom_pad{4};
  int32_t body_font_size{12};
  DialogButtonTone ok_button_tone{DialogButtonTone::DEFAULT};
  DialogButtonTone skip_button_tone{DialogButtonTone::DEFAULT};
  DialogButtonTone cancel_button_tone{DialogButtonTone::DEFAULT};
  bool use_nav_action_keys{false};
  bool focus_button_navigation{false};
};

struct DialogCallbacks {
  std::function<void()> ok_action{};
  std::function<void()> skip_action{};
  std::function<void()> cancel_action{};
};

struct DialogTextareaOptions {
  const char* accepted_chars{nullptr};
  int32_t width{246};
  int32_t height{24};
  bool one_line{true};
  bool enter_confirms{true};
};

class Dialog : public BaseWidgets {
 public:
  Dialog(lv_obj_t* parent,
         viewmodel::AppViewModel& app_view_model,
         app::AssetManager& assets,
         DialogConfig config,
         DialogCallbacks callbacks = {});
  ~Dialog() override;

  void build() override;
  bool visible() const;
  bool handle_key(uint32_t key, const char* key_name = nullptr);
  void set_ok_action(std::function<void()> action);
  void set_skip_action(std::function<void()> action);
  void set_cancel_action(std::function<void()> action);
  void trigger_ok();
  void trigger_skip();
  void trigger_cancel();
  void close();

  lv_obj_t* content() const;
  lv_obj_t* add_label(const char* text,
                      int32_t width         = 246,
                      lv_text_align_t align = LV_TEXT_ALIGN_LEFT);
  lv_obj_t* add_textarea(const char* text, const DialogTextareaOptions& options = {});
  lv_obj_t* add_dropdown(const char* options, uint32_t selected, int32_t width = 196);
  lv_obj_t* add_display_field(const char* text, int32_t width = 246, int32_t height = 26);
  lv_obj_t* ok_button() const;
  lv_obj_t* skip_button() const;
  lv_obj_t* cancel_button() const;

 private:
  struct TextareaKeyState {
    Dialog* dialog{nullptr};
    bool enter_confirms{true};
  };
  enum class ButtonRole {
    CANCEL,
    SKIP,
    OK,
  };

  struct ButtonEntry {
    lv_obj_t* button{nullptr};
    lv_obj_t* label{nullptr};
    DialogButtonTone tone{DialogButtonTone::DEFAULT};
    ButtonRole role{ButtonRole::OK};
  };

  lv_obj_t* add_button_(lv_obj_t* parent,
                        const char* text,
                        DialogButtonTone tone,
                        lv_event_cb_t callback);
  void apply_button_tone_(lv_obj_t* button, lv_obj_t* label, DialogButtonTone tone);
	  lv_color_t tone_color_(DialogButtonTone tone) const;
	  void update_button_focus_();
	  void apply_dialog_theme_(bool dark_mode);
	  void apply_theme(bool dark_mode) override;
	  void move_button_focus_(int32_t direction);
  void trigger_focused_button_();
  bool is_focus_previous_key_(uint32_t key, const char* key_name) const;
  bool is_focus_next_key_(uint32_t key, const char* key_name) const;
  void add_title_row_();
  void add_button_row_();
  bool is_ok_key_(uint32_t key, const char* key_name) const;
  bool is_cancel_key_(uint32_t key, const char* key_name) const;
  static bool key_name_is_one_of_(const char* key_name,
                                  const char* a,
                                  const char* b,
                                  const char* c);
  static void ok_button_cb(lv_event_t* event);
  static void skip_button_cb(lv_event_t* event);
  static void cancel_button_cb(lv_event_t* event);
  static void textarea_key_cb(lv_event_t* event);
  static void textarea_ready_cb(lv_event_t* event);

  viewmodel::AppViewModel& app_view_model_;
  app::AssetManager& assets_;
  DialogConfig config_{};
  DialogCallbacks callbacks_{};
  lv_obj_t* content_{nullptr};
  lv_obj_t* button_row_{nullptr};
  lv_obj_t* ok_button_{nullptr};
  lv_obj_t* skip_button_{nullptr};
  lv_obj_t* cancel_button_{nullptr};
  std::vector<ButtonEntry> button_entries_{};
  std::size_t focused_button_index_{0};
  bool action_triggered_{false};
};

}  // namespace view::widgets
