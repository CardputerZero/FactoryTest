/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace model {

class TranslationService {
 public:
  struct LanguageInfo {
    std::string locale;
    std::string display_name;
  };

  TranslationService();

  bool load_language_file(const std::string& locale,
                          const std::string& path,
                          std::string* error_message = nullptr);
  bool set_language(const std::string& locale);
  const std::string& language() const;
  const std::vector<LanguageInfo>& languages() const;
  std::string translate(std::string_view msgid) const;
  bool has_language(const std::string& locale) const;
  bool uses_cjk_font() const;

 private:
  using Catalog = std::map<std::string, std::string>;

  void upsert_language_info_(LanguageInfo info);

  std::string language_{"en"};
  std::map<std::string, Catalog> catalogs_{};
  std::vector<LanguageInfo> languages_{};
};

}  // namespace model
