/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl.h"

namespace view::widgets {

class BaseWidgets {
 public:
  explicit BaseWidgets(lv_obj_t* parent)
      : parent_(parent) {}
  virtual ~BaseWidgets() {
    if (theme_observer_) {
      lv_observer_remove(theme_observer_);
      theme_observer_ = nullptr;
    }

    if (core_obj_ && lv_obj_is_valid(core_obj_)) {
      lv_obj_delete(core_obj_);
    }
  }

  BaseWidgets(const BaseWidgets&) = delete;
  BaseWidgets& operator=(const BaseWidgets&) = delete;

  virtual void build() = 0;

  lv_obj_t* root() const { return core_obj_; }

 protected:
  void bind_theme_(lv_subject_t* subject, bool initial_dark_mode) {
    if (!subject || !core_obj_) {
      return;
    }

    if (theme_observer_) {
      lv_observer_remove(theme_observer_);
      theme_observer_ = nullptr;
    }

    theme_observer_ = lv_subject_add_observer_obj(subject, theme_observer_cb, core_obj_, this);
    apply_theme_(initial_dark_mode);
  }

  void apply_theme_(bool dark_mode) { apply_theme(dark_mode); }
  virtual void apply_theme(bool dark_mode) { LV_UNUSED(dark_mode); }

  lv_obj_t* parent_{nullptr};
  lv_obj_t* core_obj_{nullptr};

 private:
  static void theme_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    auto* widget = static_cast<BaseWidgets*>(lv_observer_get_user_data(observer));
    if (!widget) {
      return;
    }

    widget->apply_theme_(lv_subject_get_int(subject) != 0);
  }

  lv_observer_t* theme_observer_{nullptr};
};

}  // namespace view::widgets
