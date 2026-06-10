#include "profanity_filter.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>

namespace tochka {
namespace {

constexpr std::array<std::string_view, 20> kBannedRoots = {
    "бляд", "блят", "пизд", "хуй",  "хуя",  "хуе",  "хую",
    "хуи",  "ебал", "ебан", "ебат", "ебаш", "ебну", "ебет",
    "ебля", "blya", "pizd", "huy",  "ebat", "fuck",
};

constexpr unsigned char kUtf8CyrillicLead1 = 0xD0;
constexpr unsigned char kUtf8CyrillicLead2 = 0xD1;
constexpr unsigned char kUtf8CyrillicUpperStart = 0x90;  // А
constexpr unsigned char kUtf8CyrillicLowerEnd = 0xBF;    // я (в lead1)
constexpr unsigned char kUtf8UpperYoTrail = 0x81;        // Ё
constexpr unsigned char kUtf8LowerYoTrail = 0x91;        // ё
constexpr unsigned char kUtf8CyrillicLead2TrailStart = 0x80;
constexpr unsigned char kUtf8CyrillicLead2TrailEnd = 0x8F;
constexpr std::size_t kUtf8CyrillicBytes = 2;

constexpr char32_t kCyrillicUpperA = 0x0410;
constexpr char32_t kCyrillicUpperYa = 0x042F;
constexpr char32_t kCyrillicUpperYo = 0x0401;
constexpr char32_t kCyrillicLowerYo = 0x0451;
constexpr char32_t kUtf8CaseOffset = 0x20;
constexpr unsigned char kUtf8LeadPayloadMask = 0x1F;
constexpr unsigned char kUtf8TrailPayloadMask = 0x3F;
constexpr unsigned char kUtf8LeadPrefix = 0xC0;
constexpr unsigned char kUtf8TrailPrefix = 0x80;
constexpr unsigned char kUtf8PayloadBits = 6;

constexpr const char* kNormalizedYo = "е";

char32_t DecodeUtf8Pair(unsigned char lead, unsigned char trail) {
  return (static_cast<char32_t>(lead & kUtf8LeadPayloadMask)
          << kUtf8PayloadBits) |
         static_cast<char32_t>(trail & kUtf8TrailPayloadMask);
}

void AppendUtf8(char32_t codepoint, std::string& output) {
  output.push_back(
      static_cast<char>((codepoint >> kUtf8PayloadBits) | kUtf8LeadPrefix));
  output.push_back(static_cast<char>((codepoint & kUtf8TrailPayloadMask) |
                                     kUtf8TrailPrefix));
}

bool IsCyrillicLetter(unsigned char lead, unsigned char trail) {
  if (lead == kUtf8CyrillicLead1) {
    return (trail >= kUtf8CyrillicUpperStart &&
            trail <= kUtf8CyrillicLowerEnd) ||
           trail == kUtf8UpperYoTrail;
  }
  if (lead == kUtf8CyrillicLead2) {
    return trail >= kUtf8CyrillicLead2TrailStart &&
           trail <= kUtf8CyrillicLead2TrailEnd;
  }
  return false;
}

bool IsLetterByte(unsigned char byte) { return std::isalpha(byte) != 0; }

bool IsHomoglyph(char character) {
  return character == '@' || character == '4' || character == '0' ||
         character == '3' || character == '6' || character == '$';
}

void AppendLowerUtf8(unsigned char lead, unsigned char trail,
                     std::string& output) {
  const char32_t kCodepoint = DecodeUtf8Pair(lead, trail);
  if (kCodepoint == kCyrillicUpperYo || kCodepoint == kCyrillicLowerYo) {
    output.append(kNormalizedYo);
    return;
  }
  if (kCodepoint >= kCyrillicUpperA && kCodepoint <= kCyrillicUpperYa) {
    AppendUtf8(kCodepoint + kUtf8CaseOffset, output);
    return;
  }
  output.push_back(static_cast<char>(lead));
  output.push_back(static_cast<char>(trail));
}

void AppendHomoglyph(char character, std::string& output) {
  switch (character) {
    case '@':
      output.append("а");
      return;
    case '4':
      output.append("ч");
      return;
    case '0':
      output.append("о");
      return;
    case '3':
      output.append("з");
      return;
    case '6':
      output.append("б");
      return;
    case '$':
      output.push_back('s');
      return;
    default:
      output.push_back(character);
  }
}

std::string NormalizeForScan(std::string_view text) {
  std::string normalized;
  normalized.reserve(text.size());
  for (std::size_t index = 0; index < text.size();) {
    unsigned char lead = static_cast<unsigned char>(text[index]);
    if (index + 1 < text.size() &&
        IsCyrillicLetter(lead, static_cast<unsigned char>(text[index + 1]))) {
      AppendLowerUtf8(lead, static_cast<unsigned char>(text[index + 1]),
                      normalized);
      index += kUtf8CyrillicBytes;
      continue;
    }
    char mapped = static_cast<char>(std::tolower(lead));
    if (IsHomoglyph(mapped)) {
      AppendHomoglyph(mapped, normalized);
    } else if (IsLetterByte(static_cast<unsigned char>(mapped))) {
      normalized.push_back(mapped);
    }
    ++index;
  }
  return normalized;
}

bool HasBannedRoot(const std::string& normalized) {
  // NormalizeForScan уже оставляет только буквы; искать нужно во всей
  // UTF-8 строке — std::isalpha не понимает кириллицу побайтно.
  return std::any_of(kBannedRoots.begin(), kBannedRoots.end(),
                     [&normalized](std::string_view root) {
                       return normalized.find(root) != std::string::npos;
                     });
}

}  // namespace

bool ContainsProfanity(std::string_view text) {
  if (text.empty()) {
    return false;
  }
  return HasBannedRoot(NormalizeForScan(text));
}

bool WorkContainsProfanity(const std::string& title, const std::string& body,
                           const std::vector<std::string>& tags) {
  if (ContainsProfanity(title) || ContainsProfanity(body)) {
    return true;
  }
  return std::any_of(tags.begin(), tags.end(), [](const std::string& tag) {
    return ContainsProfanity(tag);
  });
}

}  // namespace tochka
