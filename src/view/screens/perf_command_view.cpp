/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_command_view.h"

#include <algorithm>
#include <sstream>
#include <thread>

#include "bindings.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

constexpr int K_CONTENT_WIDTH  = 292;
constexpr int K_POLL_MS        = 120;
constexpr int K_STDOUT_LINES   = 4;
constexpr int K_STDOUT_HEIGHT  = 42;
constexpr int K_REPORT_HEIGHT  = 82;
constexpr int K_PROGRESS_WIDTH = 250;

void set_label_font(lv_obj_t* label, lv_font_t* preferred, const lv_font_t* fallback) {
  lv_obj_set_style_text_font(label, preferred ? preferred : fallback, 0);
}

std::string truncate_line(std::string line, std::size_t max_size = 92) {
  if (line.size() <= max_size) {
    return line;
  }
  return line.substr(0, max_size - 3) + "...";
}

const char* result_icon(bool passed) {
  return passed ? view::ICON_CHECK_SQUARE : view::ICON_X_SQUARE;
}

}  // namespace

PerfCommandView::PerfCommandView(std::string title,
                                 platform::perf::TestCommand command,
                                 Runner runner)
    : title_(std::move(title)),
      command_(std::move(command)),
      runner_(std::move(runner)) {}

PerfCommandView::~PerfCommandView() {
  if (poll_timer_) {
    lv_timer_delete(poll_timer_);
    poll_timer_ = nullptr;
  }
  report_list_.reset();
}

void PerfCommandView::build(lv_obj_t* parent,
                            viewmodel::AppViewModel& app_view_model,
                            app::AssetManager& assets) {
  app_view_model_   = &app_view_model;
  assets_           = &assets;
  const auto colors = view::palette(app_view_model.is_dark_mode());

  auto* group = lv_obj_create(parent);
  lv_obj_remove_style_all(group);
  lv_obj_set_width(group, K_CONTENT_WIDTH);
  lv_obj_set_height(group, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(group, 5, 0);
  lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(group, LV_ALIGN_TOP_MID, 0, 4);

  auto* title_font  = assets.load_font("inter-semibold.ttf", 14);
  auto* body_font   = assets.load_font("inter-medium.ttf", 11);
  auto* detail_font = assets.load_font("inter-medium.ttf", 10);

  status_label_ = lv_label_create(group);
  lv_obj_set_width(status_label_, K_CONTENT_WIDTH);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  set_label_font(status_label_, title_font, &lv_font_montserrat_14);
  reactive::bind_theme(status_label_,
                       app_view_model.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);

  progress_bar_ = lv_bar_create(group);
  lv_obj_set_size(progress_bar_, K_PROGRESS_WIDTH, 8);
  lv_bar_set_range(progress_bar_, 0, 100);
  lv_bar_set_value(progress_bar_, 0, LV_ANIM_OFF);
  lv_obj_set_style_radius(progress_bar_, 4, 0);
  lv_obj_set_style_bg_color(progress_bar_, lv_color_mix(colors.primary, colors.surface, 36), 0);
  lv_obj_set_style_bg_color(progress_bar_, colors.primary, LV_PART_INDICATOR);
  lv_obj_set_style_radius(progress_bar_, 4, LV_PART_INDICATOR);

  command_label_ = lv_label_create(group);
  lv_obj_set_width(command_label_, K_CONTENT_WIDTH);
  lv_label_set_long_mode(command_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_align(command_label_, LV_TEXT_ALIGN_LEFT, 0);
  set_label_font(command_label_, body_font, &lv_font_montserrat_12);
  reactive::bind_theme(command_label_,
                       app_view_model.dark_mode_subject(),
                       reactive::ThemeRole::TEXT);
  lv_label_set_text(command_label_, platform::perf::command_to_string(command_).c_str());

  stdout_card_ = lv_obj_create(group);
  lv_obj_remove_style_all(stdout_card_);
  lv_obj_set_size(stdout_card_, K_CONTENT_WIDTH, K_STDOUT_HEIGHT);
  lv_obj_set_style_radius(stdout_card_, 6, 0);
  lv_obj_set_style_pad_all(stdout_card_, 5, 0);
  lv_obj_set_style_bg_opa(stdout_card_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(stdout_card_, lv_color_mix(colors.info, colors.surface, 18), 0);
  lv_obj_set_style_border_width(stdout_card_, 1, 0);
  lv_obj_set_style_border_color(stdout_card_, lv_color_mix(colors.info, colors.border, 38), 0);
  lv_obj_clear_flag(stdout_card_, LV_OBJ_FLAG_SCROLLABLE);

  stdout_label_ = lv_label_create(stdout_card_);
  lv_obj_set_width(stdout_label_, K_CONTENT_WIDTH - 10);
  lv_label_set_long_mode(stdout_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(stdout_label_, LV_TEXT_ALIGN_LEFT, 0);
  set_label_font(stdout_label_, detail_font, &lv_font_montserrat_12);
  lv_obj_set_style_text_color(stdout_label_, colors.info, 0);
  lv_label_set_text(stdout_label_, "waiting for command output...");

  report_host_ = lv_obj_create(group);
  lv_obj_remove_style_all(report_host_);
  lv_obj_set_size(report_host_, K_CONTENT_WIDTH, K_REPORT_HEIGHT);
  lv_obj_add_flag(report_host_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(report_host_, LV_OBJ_FLAG_SCROLLABLE);

  lv_label_set_text(status_label_, (title_ + " 0%").c_str());

  start_();
  poll_timer_ = lv_timer_create(timer_cb, K_POLL_MS, this);
}

void PerfCommandView::start_() {
  if (!runner_ || state_) {
    return;
  }

  state_      = std::make_shared<JobState>();
  auto state  = state_;
  auto runner = runner_;
  std::thread([state, runner]() {
    auto result = runner([state](const platform::perf::TestProgress& progress) {
      state->percent.store(progress.percent);
      if (!progress.line.empty()) {
        std::lock_guard<std::mutex> lock(state->mutex);
        auto line = progress.stderr_line ? std::string("err: ") + progress.line : progress.line;
        state->lines.push_back(truncate_line(std::move(line)));
        while (state->lines.size() > K_STDOUT_LINES) {
          state->lines.pop_front();
        }
        state->line_revision.fetch_add(1);
      }
    });
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->result = std::move(result);
    }
    state->percent.store(100);
    state->done.store(true);
  }).detach();
}

void PerfCommandView::update_() {
  if (!state_ || !status_label_ || !stdout_label_) {
    return;
  }

  update_progress_(state_->percent.load());
  update_stdout_buffer_();

  if (!state_->done.load() || report_shown_) {
    return;
  }

  platform::perf::TestResult result;
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    result = state_->result;
  }
  show_report_(result);
  report_shown_ = true;
  if (poll_timer_) {
    lv_timer_pause(poll_timer_);
  }
}

void PerfCommandView::update_progress_(int percent) {
  percent = std::max(0, std::min(100, percent));
  if (progress_bar_) {
    lv_bar_set_value(progress_bar_, percent, LV_ANIM_ON);
  }
  if (status_label_ && !report_shown_) {
    lv_label_set_text(status_label_, (title_ + " " + std::to_string(percent) + "%").c_str());
  }
}

void PerfCommandView::update_stdout_buffer_() {
  const auto revision = state_ ? state_->line_revision.load() : 0;
  if (revision == last_line_revision_) {
    return;
  }
  last_line_revision_ = revision;
  const auto text     = stdout_text_();
  lv_label_set_text(stdout_label_, text.empty() ? "..." : text.c_str());

  lv_obj_set_y(stdout_label_, 5);
  lv_obj_set_style_opa(stdout_label_, LV_OPA_60, 0);

  lv_anim_t y_anim;
  lv_anim_init(&y_anim);
  lv_anim_set_var(&y_anim, stdout_label_);
  lv_anim_set_values(&y_anim, 5, 0);
  lv_anim_set_time(&y_anim, 180);
  lv_anim_set_exec_cb(&y_anim, [](void* obj, int32_t value) {
    lv_obj_set_y(static_cast<lv_obj_t*>(obj), value);
  });
  lv_anim_start(&y_anim);

  lv_anim_t fade_anim;
  lv_anim_init(&fade_anim);
  lv_anim_set_var(&fade_anim, stdout_label_);
  lv_anim_set_values(&fade_anim, LV_OPA_60, LV_OPA_COVER);
  lv_anim_set_time(&fade_anim, 180);
  lv_anim_set_exec_cb(&fade_anim, [](void* obj, int32_t value) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(value), 0);
  });
  lv_anim_start(&fade_anim);
}

void PerfCommandView::show_report_(const platform::perf::TestResult& result) {
  const auto colors = view::palette(app_view_model_ && app_view_model_->is_dark_mode());
  if (status_label_) {
    lv_label_set_text(status_label_, (title_ + (result.passed ? " PASS" : " FAILED")).c_str());
    lv_obj_set_style_text_color(status_label_, result.passed ? colors.success : colors.error, 0);
  }
  if (progress_bar_) {
    lv_bar_set_value(progress_bar_, 100, LV_ANIM_ON);
    lv_obj_set_style_bg_color(progress_bar_,
                              result.passed ? colors.success : colors.error,
                              LV_PART_INDICATOR);
  }
  if (!report_host_ || !app_view_model_ || !assets_) {
    return;
  }

  report_titles_ = report_lines_(result);
  report_items_.clear();
  report_items_.reserve(report_titles_.size());
  for (std::size_t i = 0; i < report_titles_.size(); ++i) {
    report_items_.push_back(
        {i == 0 ? result_icon(result.passed) : view::ICON_INFO, report_titles_[i].c_str()});
  }

  auto* text_font = assets_->load_font("inter-medium.ttf", 11);
  auto* icon_font = assets_->load_font("Phosphor-Fill.ttf", 12);
  report_list_ =
      std::make_unique<view::widgets::IconList>(report_host_,
                                                *app_view_model_,
                                                report_items_,
                                                text_font ? text_font : &lv_font_montserrat_12,
                                                icon_font ? icon_font : &lv_font_montserrat_12,
                                                K_CONTENT_WIDTH,
                                                K_REPORT_HEIGHT,
                                                report_item_clicked,
                                                this);
  report_list_->build();
  report_list_->set_focused(false);
  lv_obj_remove_flag(report_host_, LV_OBJ_FLAG_HIDDEN);
}

std::vector<std::string> PerfCommandView::report_lines_(
    const platform::perf::TestResult& result) const {
  std::vector<std::string> lines;
  lines.push_back(result.summary.empty() ? result.name : result.summary);

  const std::vector<std::string> preferred_keys = {
      "events_per_second",
      "latency_95th",
      "bogo_ops_per_sec",
      "wall_time",
      "write_bw",
      "write_iops",
      "write_clat_95th",
  };
  for (const auto& key : preferred_keys) {
    const auto it = result.metrics.find(key);
    if (it != result.metrics.end()) {
      lines.push_back(key + ": " + it->second);
    }
    if (lines.size() >= 4) {
      break;
    }
  }

  if (!result.passed) {
    if (!result.structured_output.error_message.empty()) {
      lines.push_back("parse: " + result.structured_output.error_message);
    } else if (!result.process.stderr_text.empty()) {
      lines.push_back("stderr: " + truncate_line(result.process.stderr_text, 72));
    } else if (!result.process.error_message.empty()) {
      lines.push_back("error: " + result.process.error_message);
    }
  }

  while (lines.size() > 4) {
    lines.pop_back();
  }
  return lines;
}

std::string PerfCommandView::stdout_text_() const {
  if (!state_) {
    return {};
  }
  std::lock_guard<std::mutex> lock(state_->mutex);
  std::ostringstream out;
  bool first = true;
  for (const auto& line : state_->lines) {
    if (!first) {
      out << '\n';
    }
    first = false;
    out << line;
  }
  return out.str();
}

void PerfCommandView::timer_cb(lv_timer_t* timer) {
  auto* view = static_cast<PerfCommandView*>(lv_timer_get_user_data(timer));
  if (view) {
    view->update_();
  }
}

void PerfCommandView::report_item_clicked(std::size_t index, void* user_data) {
  LV_UNUSED(index);
  LV_UNUSED(user_data);
}

}  // namespace screen
