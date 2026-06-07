#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>

#include "auth.hpp"
#include "handlers.hpp"
#include "json_helpers.hpp"
#include "rate_limiter.hpp"
#include "serialize.hpp"

namespace tochka {
namespace {

constexpr std::size_t kMinPasswordLen = 8;
constexpr std::size_t kMaxPasswordLen = 256;
constexpr std::size_t kMinUsernameLen = 3;
constexpr std::size_t kMaxUsernameLen = 20;
constexpr std::size_t kMaxDisplayNameLen = 80;

// Анти-брутфорс: не более 10 попыток входа за 5 минут с одного ключа
// (IP и, отдельно, целевой email), и не более 5 регистраций за час с IP.
RateLimiter& LoginLimiter() {
  static RateLimiter limiter(10, std::chrono::seconds(300));
  return limiter;
}

RateLimiter& RegisterLimiter() {
  static RateLimiter limiter(5, std::chrono::seconds(3600));
  return limiter;
}

std::string NormalizeEmail(std::string email) {
  std::transform(email.begin(), email.end(), email.begin(),
                 [](unsigned char chr) { return std::tolower(chr); });
  return email;
}

bool LooksLikeEmail(const std::string& email) {
  auto at_pos = email.find('@');
  if (at_pos == std::string::npos || at_pos == 0) {
    return false;
  }
  auto dot = email.find('.', at_pos);
  return dot != std::string::npos && dot + 1 < email.size();
}

// Приводит @ник к нижнему регистру и срезает ведущий «@», если его ввели.
std::string NormalizeUsername(std::string username) {
  username = Trim(username);
  if (!username.empty() && username.front() == '@') {
    username.erase(username.begin());
  }
  std::transform(username.begin(), username.end(), username.begin(),
                 [](unsigned char chr) { return std::tolower(chr); });
  return username;
}

// Допустимы латиница, цифры и подчёркивание, длиной 3–20 символов.
bool ValidUsername(const std::string& username) {
  if (username.size() < kMinUsernameLen || username.size() > kMaxUsernameLen) {
    return false;
  }
  return std::all_of(username.begin(), username.end(), [](unsigned char chr) {
    return (chr >= 'a' && chr <= 'z') || (chr >= '0' && chr <= '9') ||
           chr == '_';
  });
}

// Формирует значение заголовка Set-Cookie с JWT.
// HttpOnly — недоступна JS (защита от кражи через XSS);
// Secure — только по HTTPS (за TLS-прокси); SameSite=Lax — заслон от CSRF.
std::string AuthCookie(const std::string& token, int ttl_hours, bool secure) {
  std::string cookie = "token=" + token +
                       "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" +
                       std::to_string(ttl_hours * 3600);
  if (secure) {
    cookie += "; Secure";
  }
  return cookie;
}

std::string ClearCookie(bool secure) {
  std::string cookie = "token=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0";
  if (secure) {
    cookie += "; Secure";
  }
  return cookie;
}

JsonResponse IssueToken(const AppContext& app, const RequestContext& ctx,
                        const pqxx::row& row) {
  auth::TokenClaims claims;
  claims.user_id = row["id"].as<long long>();
  claims.role = row["role"].as<std::string>();
  std::string token =
      auth::MakeJwt(claims, app.config.jwt_secret, app.config.jwt_ttl_hours);
  JsonResponse res = JsonOk(boost::json::object{{"user", UserPublic(row)}});
  res.extra_headers.emplace_back(
      "Set-Cookie",
      AuthCookie(token, app.config.jwt_ttl_hours, ctx.is_https));
  return res;
}

JsonResponse Register(AppContext& app, RequestContext& ctx) {
  if (!RegisterLimiter().allow(ctx.client_ip)) {
    return JsonError(http_status::kTooManyRequests,
                     "Слишком много регистраций. Попробуйте позже");
  }
  auto body = ctx.json_body();
  std::string email = NormalizeEmail(JsonString(body, "email"));
  std::string username = NormalizeUsername(JsonString(body, "username"));
  std::string password = JsonString(body, "password");
  std::string display_name = Trim(JsonString(body, "display_name"));

  if (!LooksLikeEmail(email)) {
    return JsonError(http_status::kBadRequest,
                     "Введите корректный email, например name@example.com");
  }
  if (!ValidUsername(username)) {
    return JsonError(http_status::kBadRequest,
                     "Юзернейм должен быть 3–20 символов: латиница, цифры и _");
  }
  if (password.size() < kMinPasswordLen || password.size() > kMaxPasswordLen) {
    return JsonError(http_status::kBadRequest,
                     "Пароль должен содержать от 8 до 256 символов");
  }
  if (display_name.empty() || display_name.size() > kMaxDisplayNameLen) {
    return JsonError(http_status::kBadRequest,
                     "Укажите отображаемое имя (до 80 символов)");
  }

  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  if (!txn.exec_params("SELECT 1 FROM users WHERE email = $1", email).empty()) {
    return JsonError(http_status::kConflict, "Этот email уже зарегистрирован");
  }
  if (!txn.exec_params("SELECT 1 FROM users WHERE username = $1", username)
           .empty()) {
    return JsonError(http_status::kConflict,
                     "Юзернейм @" + username + " уже занят");
  }
  auto inserted = txn.exec_params(
      "INSERT INTO users (email, username, password_hash, display_name) "
      "VALUES ($1, $2, $3, $4) "
      "RETURNING id, username, display_name, bio, avatar_url, role, "
      "created_at::text",
      email, username, auth::HashPassword(password), display_name);
  JsonResponse res = IssueToken(app, ctx, inserted[0]);
  txn.commit();
  res.status = http_status::kCreated;
  return res;
}

JsonResponse Login(AppContext& app, RequestContext& ctx) {
  auto body = ctx.json_body();
  std::string email = NormalizeEmail(JsonString(body, "email"));
  std::string password = JsonString(body, "password");

  // Лимитируем и по IP, и по целевому email, чтобы осложнить перебор.
  if (!LoginLimiter().allow("ip:" + ctx.client_ip) ||
      !LoginLimiter().allow("email:" + email)) {
    return JsonError(http_status::kTooManyRequests,
                     "Слишком много попыток входа. Попробуйте позже");
  }

  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec_params(
      "SELECT id, username, password_hash, display_name, bio, avatar_url, "
      "role, created_at::text FROM users WHERE email = $1",
      email);
  if (rows.empty() ||
      !auth::VerifyPassword(rows[0]["password_hash"].as<std::string>(),
                            password)) {
    return JsonError(http_status::kUnauthorized, "Неверный email или пароль");
  }
  return IssueToken(app, ctx, rows[0]);
}

JsonResponse Logout(AppContext& app, RequestContext& ctx) {
  (void)app;
  JsonResponse res = JsonOk(boost::json::object{{"ok", true}});
  res.extra_headers.emplace_back("Set-Cookie", ClearCookie(ctx.is_https));
  return res;
}

JsonResponse Me(AppContext& app, RequestContext& ctx) {
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec_params(
      "SELECT id, username, display_name, bio, avatar_url, role, "
      "created_at::text FROM users WHERE id = $1",
      *ctx.user_id);
  if (rows.empty()) {
    return JsonError(http_status::kNotFound, "Пользователь не найден");
  }
  return JsonOk(boost::json::object{{"user", UserPublic(rows[0])}});
}

}  // namespace

void RegisterAuthRoutes(Router& router, AppContext& app) {
  router.add("POST", "/api/auth/register", AuthLevel::None,
             [&app](RequestContext& ctx) { return Register(app, ctx); });
  router.add("POST", "/api/auth/login", AuthLevel::None,
             [&app](RequestContext& ctx) { return Login(app, ctx); });
  router.add("POST", "/api/auth/logout", AuthLevel::None,
             [&app](RequestContext& ctx) { return Logout(app, ctx); });
  router.add("GET", "/api/auth/me", AuthLevel::User,
             [&app](RequestContext& ctx) { return Me(app, ctx); });
}

}  // namespace tochka
