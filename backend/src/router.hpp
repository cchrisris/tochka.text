#pragma once

#include <functional>
#include <string>
#include <vector>

#include "http_types.hpp"

namespace tochka {

using Handler = std::function<JsonResponse(RequestContext&)>;

class Router {
 public:
  void add(const std::string& method, const std::string& pattern,
           AuthLevel auth, Handler handler);

  JsonResponse dispatch(RequestContext& ctx, bool& matched) const;

  static std::vector<std::string> split_path(const std::string& path);

 private:
  struct Route {
    std::string method;
    std::vector<std::string> segments;
    AuthLevel auth;
    Handler handler;
  };

  static bool match(const Route& route,
                    const std::vector<std::string>& segments,
                    std::unordered_map<std::string, std::string>& params);
  static JsonResponse check_access(const Route& route,
                                   const RequestContext& ctx);

  std::vector<Route> routes_;
};

}  // namespace tochka
