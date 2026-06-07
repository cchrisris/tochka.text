#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "http_types.hpp"

namespace tochka {

inline std::string JsonString(const boost::json::value& value,
                              const char* key) {
  if (!value.is_object()) {
    return "";
  }
  const auto* entry = value.as_object().find(key);
  if (entry == value.as_object().end() || !entry->value().is_string()) {
    return "";
  }
  return std::string(entry->value().as_string().c_str());
}

inline std::optional<long long> JsonInt(const boost::json::value& value,
                                        const char* key) {
  if (!value.is_object()) {
    return std::nullopt;
  }
  const auto* entry = value.as_object().find(key);
  if (entry == value.as_object().end()) {
    return std::nullopt;
  }
  if (entry->value().is_int64()) {
    return entry->value().as_int64();
  }
  if (entry->value().is_string()) {
    try {
      return std::stoll(entry->value().as_string().c_str());
    } catch (const std::exception&) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

inline std::optional<std::string> OptText(const std::string& text) {
  if (text.empty()) {
    return std::nullopt;
  }
  return text;
}

inline std::string Trim(const std::string& text) {
  auto begin = text.find_first_not_of(" \t\n\r");
  if (begin == std::string::npos) {
    return "";
  }
  auto end = text.find_last_not_of(" \t\n\r");
  return text.substr(begin, end - begin + 1);
}

inline bool ParsePathId(const RequestContext& ctx, const char* key,
                        long long& out) {
  auto found = ctx.params.find(key);
  if (found == ctx.params.end()) {
    return false;
  }
  try {
    out = std::stoll(found->second);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace tochka
