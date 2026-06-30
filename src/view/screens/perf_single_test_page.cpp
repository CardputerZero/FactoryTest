/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "perf_single_test_page.h"

#include "linux_input.h"
#include "ui_const.h"

namespace screen {

PerfSingleTestPage::PerfSingleTestPage(viewmodel::AppViewModel& app_view_model,
                                       app::AssetManager& assets,
                                       model::AppPage page)
    : BaseScreen(app_view_model, assets),
      page_(page) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::CLICK);
  update_nav_actions_();
  init();
  platform::set_key_listener(key_listener, this);
}

PerfSingleTestPage::~PerfSingleTestPage() {
  platform::clear_key_listener(key_listener, this);
  sd_view_.reset();
  mem_view_.reset();
  cpu_view_.reset();
}

void PerfSingleTestPage::build_content(lv_obj_t* content) {
  switch (page_) {
    case model::AppPage::CPU_BENCHMARK:
      cpu_view_ = std::make_unique<PerfCpuView>();
      cpu_view_->build(content, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::MEM_STRESS_TEST:
      mem_view_ = std::make_unique<PerfMemView>();
      mem_view_->build(content, app_view_model_ref_(), assets_ref_());
      break;
    case model::AppPage::SD_CARD_TEST:
      sd_view_ = std::make_unique<PerfSdView>();
      sd_view_->build(content, app_view_model_ref_(), assets_ref_());
      break;
    default:
      break;
  }
}

void PerfSingleTestPage::update_nav_actions_() {
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_('4', view::ICON_ARROW_U_UP_LEFT, [this]() {
    app_view_model_ref_().request_back_or_quit();
  });
  set_nav_action_('8', view::ICON_CHECK_SQUARE, [this]() { show_test_result_dialog_(); });
}

void PerfSingleTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<PerfSingleTestPage*>(user_data);
  if (page) {
    page->handle_test_result_dialog_key_(key, key_name);
  }
}

}  // namespace screen
