#pragma once

#include <optional>
#include <string>

namespace tochka::auth {

std::string HashPassword(const std::string& password);
bool VerifyPassword(const std::string& hash, const std::string& password);

struct TokenClaims {
  long long user_id = 0;
  std::string role;
};

std::string MakeJwt(const TokenClaims& claims, const std::string& secret,
                    int ttl_hours);
std::optional<TokenClaims> VerifyJwt(const std::string& token,
                                     const std::string& secret);

}  // namespace tochka::auth
