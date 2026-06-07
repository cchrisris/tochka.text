#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <random>
#include <string>
#include <system_error>

#include "handlers.hpp"
#include "json_helpers.hpp"

namespace tochka {
namespace {

constexpr std::size_t kMaxUploadBytes = 8ULL * 1024 * 1024;
constexpr int kNameLen = 32;
constexpr int kHexMax = 15;

bool StartsWith(const std::string& data, const char* sig, std::size_t len) {
  return data.size() >= len && std::memcmp(data.data(), sig, len) == 0;
}

// Определяем формат по сигнатуре (magic bytes), а не по Content-Type клиента:
// заголовок легко подделать, а так на диск попадёт только настоящая картинка.
std::string SniffImageExt(const std::string& data) {
  if (StartsWith(data, "\xFF\xD8\xFF", 3)) {
    return ".jpg";
  }
  if (StartsWith(data, "\x89PNG\r\n\x1A\n", 8)) {
    return ".png";
  }
  if (StartsWith(data, "GIF87a", 6) || StartsWith(data, "GIF89a", 6)) {
    return ".gif";
  }
  if (data.size() >= 12 && std::memcmp(data.data(), "RIFF", 4) == 0 &&
      std::memcmp(data.data() + 8, "WEBP", 4) == 0) {
    return ".webp";
  }
  return "";
}

std::string RandomName() {
  static constexpr char kHex[] = "0123456789abcdef";
  std::random_device rng;
  std::uniform_int_distribution<int> dist(0, kHexMax);
  std::string out;
  out.reserve(kNameLen);
  for (int i = 0; i < kNameLen; ++i) {
    out.push_back(kHex[dist(rng)]);
  }
  return out;
}

JsonResponse UploadImage(AppContext& app, RequestContext& ctx) {
  if (ctx.body.empty()) {
    return JsonError(http_status::kBadRequest, "Пустой файл");
  }
  if (ctx.body.size() > kMaxUploadBytes) {
    return JsonError(http_status::kBadRequest, "Файл слишком большой");
  }
  std::string ext = SniffImageExt(ctx.body);
  if (ext.empty()) {
    return JsonError(http_status::kBadRequest,
                     "Поддерживаются только изображения jpeg, png, webp, gif");
  }

  std::error_code ec;
  std::filesystem::create_directories(app.config.uploads_dir, ec);
  std::string name = RandomName() + ext;
  std::filesystem::path full =
      std::filesystem::path(app.config.uploads_dir) / name;

  std::ofstream out(full, std::ios::binary);
  out.write(ctx.body.data(), static_cast<std::streamsize>(ctx.body.size()));
  out.close();
  if (!out) {
    return JsonError(http_status::kServerError, "Не удалось сохранить файл");
  }
  return JsonOk(boost::json::object{{"url", "/uploads/" + name}},
                http_status::kCreated);
}

}  // namespace

void RegisterUploadRoutes(Router& router, AppContext& app) {
  router.add("POST", "/api/uploads", AuthLevel::User,
             [&app](RequestContext& ctx) { return UploadImage(app, ctx); });
}

}  // namespace tochka
