#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tochka {

inline constexpr const char *kProfanityErrorMessage =
    "Нельзя публиковать произведения с ненормативной лексикой";

inline constexpr const char *kCommentProfanityErrorMessage =
    "Нельзя отправлять комментарии с ненормативной лексикой";

bool ContainsProfanity(std::string_view text);

bool WorkContainsProfanity(const std::string &title, const std::string &body,
                           const std::vector<std::string> &tags = {});

} // namespace tochka
