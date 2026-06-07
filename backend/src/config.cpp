#include "config.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace tochka {
namespace {

constexpr int kDefaultTtlHours = 72;
constexpr unsigned short kDefaultPort = 8080;

std::string EnvOr(const char* key, const std::string& fallback) {
  const char* value = std::getenv(key);
  return value != nullptr ? std::string(value) : fallback;
}

int ParseIntOr(const std::string& text, int fallback) {
  try {
    return std::stoi(text);
  } catch (const std::exception&) {
    return fallback;
  }
}

}  // namespace

Config Config::from_env() {
  Config cfg;
  cfg.database_url = EnvOr(
      "DATABASE_URL", "postgresql://tochka:tochka_dev_password@db:5432/tochka");
  cfg.jwt_secret = EnvOr("JWT_SECRET", "change_me_in_production");
  cfg.uploads_dir = EnvOr("UPLOADS_DIR", "/data/uploads");
  cfg.cors_origin = EnvOr("CORS_ORIGIN", "*");
  cfg.jwt_ttl_hours =
      ParseIntOr(EnvOr("JWT_TTL_HOURS", std::to_string(kDefaultTtlHours)),
                 kDefaultTtlHours);
  cfg.port = static_cast<unsigned short>(ParseIntOr(
      EnvOr("BACKEND_PORT", std::to_string(kDefaultPort)), kDefaultPort));
  return cfg;
}

}  // namespace tochka
