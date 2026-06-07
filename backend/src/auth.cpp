#include "auth.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sodium.h>

#include <array>
#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace tochka::auth {
namespace {

constexpr int kBitsPerByte = 8;
constexpr int kBase64Bits = 6;
constexpr unsigned kBase64Mask = 0x3F;
constexpr long long kSecondsPerHour = 3600;
constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string Base64UrlEncode(const std::string& data) {
  std::string out;
  unsigned buffer = 0;
  int bits = 0;
  for (unsigned char byte : data) {
    buffer = (buffer << kBitsPerByte) | byte;
    bits += kBitsPerByte;
    while (bits >= kBase64Bits) {
      bits -= kBase64Bits;
      out.push_back(kAlphabet[(buffer >> bits) & kBase64Mask]);
    }
  }
  if (bits > 0) {
    out.push_back(kAlphabet[(buffer << (kBase64Bits - bits)) & kBase64Mask]);
  }
  return out;
}

int DecodeChar(char chr) {
  for (int idx = 0; kAlphabet[idx] != '\0'; ++idx) {
    if (kAlphabet[idx] == chr) {
      return idx;
    }
  }
  return -1;
}

std::optional<std::string> Base64UrlDecode(const std::string& input) {
  std::string out;
  unsigned buffer = 0;
  int bits = 0;
  for (char chr : input) {
    int value = DecodeChar(chr);
    if (value < 0) {
      return std::nullopt;
    }
    buffer = (buffer << kBase64Bits) | static_cast<unsigned>(value);
    bits += kBase64Bits;
    if (bits >= kBitsPerByte) {
      bits -= kBitsPerByte;
      out.push_back(static_cast<char>(buffer >> bits));
    }
  }
  return out;
}

std::string HmacSha256(const std::string& key, const std::string& data) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> out{};
  unsigned out_len = 0;
  HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
       reinterpret_cast<const unsigned char*>(data.data()), data.size(),
       out.data(), &out_len);
  return std::string(reinterpret_cast<char*>(out.data()), out_len);
}

bool ConstantTimeEqual(const std::string& lhs, const std::string& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  unsigned char diff = 0;
  for (std::size_t idx = 0; idx < lhs.size(); ++idx) {
    diff |= static_cast<unsigned char>(lhs[idx]) ^
            static_cast<unsigned char>(rhs[idx]);
  }
  return diff == 0;
}

long long NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool IsExpired(const boost::json::object& payload) {
  const auto* entry = payload.find("exp");
  if (entry == payload.end() || !entry->value().is_int64()) {
    return false;
  }
  return entry->value().as_int64() < NowSeconds();
}

}  // namespace

std::string HashPassword(const std::string& password) {
  if (sodium_init() < 0) {
    throw std::runtime_error("libsodium init failed");
  }
  std::string hashed(crypto_pwhash_STRBYTES, '\0');
  if (crypto_pwhash_str(hashed.data(), password.c_str(), password.size(),
                        crypto_pwhash_OPSLIMIT_INTERACTIVE,
                        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
    throw std::runtime_error("password hashing failed");
  }
  hashed.resize(std::char_traits<char>::length(hashed.c_str()));
  return hashed;
}

bool VerifyPassword(const std::string& hash, const std::string& password) {
  if (sodium_init() < 0) {
    return false;
  }
  return crypto_pwhash_str_verify(hash.c_str(), password.c_str(),
                                  password.size()) == 0;
}

std::string MakeJwt(const TokenClaims& claims, const std::string& secret,
                    int ttl_hours) {
  long long now = NowSeconds();
  boost::json::object header{{"alg", "HS256"}, {"typ", "JWT"}};
  boost::json::object payload{{"sub", claims.user_id},
                              {"role", claims.role},
                              {"iat", now},
                              {"exp", now + ttl_hours * kSecondsPerHour}};

  std::string head = Base64UrlEncode(boost::json::serialize(header));
  std::string body = Base64UrlEncode(boost::json::serialize(payload));
  std::string signing_input = head + "." + body;
  std::string signature = Base64UrlEncode(HmacSha256(secret, signing_input));
  return signing_input + "." + signature;
}

std::optional<TokenClaims> VerifyJwt(const std::string& token,
                                     const std::string& secret) {
  auto first = token.find('.');
  auto second = token.find('.', first + 1);
  if (first == std::string::npos || second == std::string::npos) {
    return std::nullopt;
  }

  std::string signing_input = token.substr(0, second);
  std::string expected = Base64UrlEncode(HmacSha256(secret, signing_input));
  if (!ConstantTimeEqual(token.substr(second + 1), expected)) {
    return std::nullopt;
  }

  auto raw = Base64UrlDecode(token.substr(first + 1, second - first - 1));
  if (!raw) {
    return std::nullopt;
  }
  boost::system::error_code error;
  auto parsed = boost::json::parse(*raw, error);
  if (error || !parsed.is_object()) {
    return std::nullopt;
  }

  auto& payload = parsed.as_object();
  auto* sub = payload.find("sub");
  if (IsExpired(payload) || sub == payload.end() || !sub->value().is_int64()) {
    return std::nullopt;
  }

  TokenClaims claims;
  claims.user_id = sub->value().as_int64();
  if (auto* role = payload.find("role");
      role != payload.end() && role->value().is_string()) {
    claims.role = std::string(role->value().as_string().c_str());
  }
  return claims;
}

}  // namespace tochka::auth
