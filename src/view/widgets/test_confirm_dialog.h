/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "app_viewmodel.h"
#include "asset_manager.h"
#include "base_widget.h"
#include "lvgl.h"

namespace view::widgets {

struct TestConfirmDialogConfig {
  std::string title{};
  std::size_t current{0};
  std::size_t total{0};
};

struct TestConfirmDialogCallbacks {
  std::function<void()> pass_action{};
  std::function<void()> skip_action{};
  std::function<void()> fail_action{};
};

class TestConfirmDialog : public BaseWidgets {
 public:
  TestConfirmDialog(lv_obj_t* parent,
                    viewmodel::AppViewModel& app_view_model,
                    app::AssetManager& assets,
                    TestConfirmDialogConfig config,
                    TestConfirmDialogCallbacks callbacks = {});
  ~TestConfirmDialog() override;

  void build() override;
  bool visible() const;
  bool handle_key(uint32_t key, const char* key_name = nullptr);
  void close();

 private:
  enum class ButtonRole {
    FAIL,
    SKIP,
    PASS,
  };

  struct ButtonEntry {
    lv_obj_t* button{nullptr};
    lv_obj_t* label{nullptr};
    ButtonRole role{ButtonRole::PASS};
  };

  const lv_font_t* ui_font_(const char* latin_font_name, uint32_t size);
  void add_title_();
  void add_body_();
  void add_buttons_();
  void add_button_(const char* label, ButtonRole role);
  void apply_theme(bool dark_mode) override;
  void apply_button_style_(ButtonEntry& entry, bool focused);
  void move_focus_(int32_t direction);
  void trigger_focused_button_();
  lv_color_t role_color_(ButtonRole role) const;
  bool is_previous_key_(uint32_t key, const char* key_name) const;
  bool is_next_key_(uint32_t key, const char* key_name) const;
  bool is_enter_key_(uint32_t key, const char* key_name) const;
  bool is_cancel_key_(uint32_t key, const char* key_name) const;
  static bool key_name_is_one_of_(const char* key_name,
                                  const char* a,
                                  const char* b,
                                  const char* c);
  static void button_cb(lv_event_t* event);

  viewmodel::AppViewModel& app_view_model_;
  app::AssetManager& assets_;
  TestConfirmDialogConfig config_{};
  TestConfirmDialogCallbacks callbacks_{};
  lv_obj_t* title_label_{nullptr};
  lv_obj_t* body_label_{nullptr};
  lv_obj_t* button_row_{nullptr};
  ButtonEntry buttons_[3]{};
  std::size_t button_count_{0};
  std::size_t focused_button_index_{0};
  bool action_triggered_{false};
};

}  // namespace view::widgets
