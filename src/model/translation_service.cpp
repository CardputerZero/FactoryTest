/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "translation_service.h"

#include <utility>

#include "serialization.h"

namespace model {
namespace {

const platform::serialization::OutputValue* object_value_for_key(
    const platform::serialization::OutputValue& object,
    const std::string& key) {
  if (object.type != platform::serialization::OutputValue::Type::Object) {
    return nullptr;
  }

  const auto it = object.object_values.find(key);
  return it == object.object_values.end() ? nullptr : &it->second;
}

std::string string_value_or(const platform::serialization::OutputValue* value,
                            const std::string& fallback) {
  if (!value || value->type != platform::serialization::OutputValue::Type::String) {
    return fallback;
  }
  return value->string_value;
}

}  // namespace

TranslationService::TranslationService() {
  languages_.push_back({"en", "English"});
}

bool TranslationService::load_language_file(const std::string& locale,
                                            const std::string& path,
                                            std::string* error_message) {
  if (locale.empty() || path.empty()) {
    if (error_message) {
      *error_message = "empty language locale or path";
    }
    return false;
  }

  auto parsed = platform::serialization::parse_json_file(path);
  if (!parsed.success()) {
    if (error_message) {
      *error_message = parsed.error_message;
    }
    return false;
  }
  if (parsed.value.type != platform::serialization::OutputValue::Type::Object) {
    if (error_message) {
      *error_message = "translation root must be a JSON object";
    }
    return false;
  }

  const auto* messages = object_value_for_key(parsed.value, "messages");
  if (!messages) {
    messages = &parsed.value;
  }
  if (messages->type != platform::serialization::OutputValue::Type::Object) {
    if (error_message) {
      *error_message = "translation messages must be a JSON object";
    }
    return false;
  }

  Catalog catalog;
  for (const auto& item : messages->object_values) {
    if (item.second.type == platform::serialization::OutputValue::Type::String) {
      catalog[item.first] = item.second.string_value;
    }
  }

  catalogs_[locale] = std::move(catalog);
  upsert_language_info_(
      {locale, string_value_or(object_value_for_key(parsed.value, "display_name"), locale)});
  return true;
}

bool TranslationService::set_language(const std::string& locale) {
  if (!has_language(locale)) {
    return false;
  }
  language_ = locale;
  return true;
}

const std::string& TranslationService::language() const {
  return language_;
}

const std::vector<TranslationService::LanguageInfo>& TranslationService::languages() const {
  return languages_;
}

std::string TranslationService::translate(std::string_view msgid) const {
  if (msgid.empty() || language_ == "en") {
    return std::string(msgid);
  }

  const auto catalog_it = catalogs_.find(language_);
  if (catalog_it == catalogs_.end()) {
    return std::string(msgid);
  }

  const auto message_it = catalog_it->second.find(std::string(msgid));
  return message_it == catalog_it->second.end() ? std::string(msgid) : message_it->second;
}

bool TranslationService::has_language(const std::string& locale) const {
  return locale == "en" || catalogs_.find(locale) != catalogs_.end();
}

bool TranslationService::uses_cjk_font() const {
  return language_.rfind("zh", 0) == 0;
}

void TranslationService::upsert_language_info_(LanguageInfo info) {
  for (auto& language : languages_) {
    if (language.locale == info.locale) {
      language = std::move(info);
      return;
    }
  }
  languages_.push_back(std::move(info));
}

}  // namespace model
