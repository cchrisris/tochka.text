#pragma once

#include "config.hpp"
#include "db.hpp"
#include "router.hpp"

namespace tochka {

struct AppContext {
  Database& db;
  const Config& config;
};

void RegisterAuthRoutes(Router& router, AppContext& app);
void RegisterWorksRoutes(Router& router, AppContext& app);
void RegisterCommentRoutes(Router& router, AppContext& app);
void RegisterCollectionRoutes(Router& router, AppContext& app);
void RegisterUserRoutes(Router& router, AppContext& app);
void RegisterAdminRoutes(Router& router, AppContext& app);
void RegisterUploadRoutes(Router& router, AppContext& app);

}  // namespace tochka
