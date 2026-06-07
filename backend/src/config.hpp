#pragma once

#include <string>

namespace tochka {

struct Config {
  std::string database_url;
  std::string jwt_secret;
  std::string uploads_dir = "/data/uploads";
  std::string cors_origin = "*";
  int jwt_ttl_hours = 72;
  unsigned short port = 8080;

  static Config from_env();
};

}  // namespace tochka
