#include <sodium.h>

#include <iostream>
#include <thread>

#include "config.hpp"
#include "db.hpp"
#include "handlers.hpp"
#include "http_server.hpp"
#include "router.hpp"

namespace {

constexpr int kFallbackThreads = 4;
constexpr std::size_t kExtraConnections = 2;

}  // namespace

int main() {
  using namespace tochka;

  if (sodium_init() < 0) {
    std::cerr << "libsodium init failed" << std::endl;
    return 1;
  }

  Config config = Config::from_env();
  unsigned int cores = std::thread::hardware_concurrency();
  int threads = cores > 0 ? static_cast<int>(cores) : kFallbackThreads;

  Database database(config.database_url,
                    static_cast<std::size_t>(threads) + kExtraConnections);
  AppContext app{database, config};

  Router router;
  router.add("GET", "/api/health", AuthLevel::None, [](RequestContext&) {
    return JsonOk(boost::json::object{{"status", "ok"}});
  });
  RegisterAuthRoutes(router, app);
  RegisterWorksRoutes(router, app);
  RegisterCommentRoutes(router, app);
  RegisterCollectionRoutes(router, app);
  RegisterUserRoutes(router, app);
  RegisterAdminRoutes(router, app);
  RegisterUploadRoutes(router, app);

  HttpServer server(config, router);
  server.run(threads);
  return 0;
}
