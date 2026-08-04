/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"
#include "logger.h"

int main() {
  app::Application application;
  const int result = application.run();
  LOG_INFO("application exiting: code={}", result);
  logger::Logger::shutdown();
  return result;
}
