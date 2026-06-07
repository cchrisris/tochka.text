#include "router.hpp"

#include <string>
#include <utility>
#include <vector>

namespace tochka {

std::vector<std::string> Router::split_path(const std::string& path) {
  std::vector<std::string> out;
  std::string current;
  for (char chr : path) {
    if (chr == '/') {
      if (!current.empty()) {
        out.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(chr);
    }
  }
  if (!current.empty()) {
    out.push_back(current);
  }
  return out;
}

void Router::add(const std::string& method, const std::string& pattern,
                 AuthLevel auth, Handler handler) {
  routes_.push_back(
      Route{method, split_path(pattern), auth, std::move(handler)});
}

bool Router::match(const Route& route, const std::vector<std::string>& segments,
                   std::unordered_map<std::string, std::string>& params) {
  if (route.segments.size() != segments.size()) {
    return false;
  }
  for (std::size_t i = 0; i < route.segments.size(); ++i) {
    const std::string& seg = route.segments[i];
    if (!seg.empty() && seg[0] == ':') {
      params[seg.substr(1)] = segments[i];
    } else if (seg != segments[i]) {
      return false;
    }
  }
  return true;
}

JsonResponse Router::check_access(const Route& route,
                                  const RequestContext& ctx) {
  if (route.auth == AuthLevel::User && !ctx.authenticated()) {
    return JsonError(http_status::kUnauthorized, "Требуется авторизация");
  }
  if (route.auth == AuthLevel::Admin && !ctx.is_admin()) {
    return JsonError(http_status::kForbidden, "Нужны права администратора");
  }
  return JsonOk(boost::json::object{});
}

JsonResponse Router::dispatch(RequestContext& ctx, bool& matched) const {
  auto segments = split_path(ctx.path);
  bool path_exists = false;

  for (const auto& route : routes_) {
    std::unordered_map<std::string, std::string> params;
    if (!match(route, segments, params)) {
      continue;
    }
    path_exists = true;
    if (route.method != ctx.method) {
      continue;
    }

    matched = true;
    ctx.params = std::move(params);
    JsonResponse access = check_access(route, ctx);
    if (access.status != http_status::kOk) {
      return access;
    }
    return route.handler(ctx);
  }

  matched = false;
  if (path_exists) {
    return JsonError(http_status::kMethodNotAllowed, "Метод не поддерживается");
  }
  return JsonError(http_status::kNotFound, "Не найдено");
}

}  // namespace tochka
