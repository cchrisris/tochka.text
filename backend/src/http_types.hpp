#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tochka {

enum class AuthLevel { None, User, Admin };

namespace http_status {
constexpr int kOk = 200;
constexpr int kCreated = 201;
constexpr int kNoContent = 204;
constexpr int kBadRequest = 400;
constexpr int kUnauthorized = 401;
constexpr int kForbidden = 403;
constexpr int kNotFound = 404;
constexpr int kMethodNotAllowed = 405;
constexpr int kConflict = 409;
constexpr int kTooManyRequests = 429;
constexpr int kServerError = 500;
}  // namespace http_status

struct RequestContext {
  std::string method;
  std::string path;
  std::unordered_map<std::string, std::string> params;
  std::unordered_map<std::string, std::string> query;
  std::string body;
  std::string content_type;
  std::string client_ip;
  std::string origin;
  std::string host;
  bool is_https = false;

  std::optional<long long> user_id;
  std::string role;
  // true, если авторизация пришла через cookie (а не Authorization: Bearer).
  // Только для cookie-аутентификации нужна защита от CSRF.
  bool auth_via_cookie = false;

  bool authenticated() const { return user_id.has_value(); }
  bool is_admin() const { return role == "admin"; }

  boost::json::value json_body() const {
    if (body.empty()) {
      return boost::json::object{};
    }
    boost::system::error_code ec;
    auto value = boost::json::parse(body, ec);
    if (ec) {
      return boost::json::object{};
    }
    return value;
  }

  std::optional<std::string> query_param(const std::string& key) const {
    auto it = query.find(key);
    if (it == query.end()) {
      return std::nullopt;
    }
    return it->second;
  }
};

struct JsonResponse {
  int status = http_status::kOk;
  boost::json::value body = boost::json::object{};
  // Дополнительные заголовки ответа (например, Set-Cookie).
  std::vector<std::pair<std::string, std::string>> extra_headers;
};

inline JsonResponse JsonOk(boost::json::value body,
                           int status = http_status::kOk) {
  return JsonResponse{status, std::move(body)};
}

inline JsonResponse JsonError(int status, const std::string& message) {
  return JsonResponse{status, boost::json::object{{"error", message}}};
}

}  // namespace tochka
