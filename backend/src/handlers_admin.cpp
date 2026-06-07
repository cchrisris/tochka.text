#include <algorithm>
#include <string>
#include <utility>

#include "handlers.hpp"
#include "json_helpers.hpp"
#include "serialize.hpp"

namespace tochka {
namespace {

constexpr int kAdminFeedLimit = 100;
constexpr std::size_t kMaxTitleLen = 200;
constexpr std::size_t kMaxDescriptionLen = 4000;
constexpr std::size_t kMaxCssLen = 500;

// hero_bg уходит в инлайновый CSS `background` на фронте. Поле admin-only,
// но дополнительно режем символы, которыми можно вырваться из значения
// свойства или подтянуть постороннюю логику.
bool SafeCssValue(const std::string& value) {
  if (value.size() > kMaxCssLen) {
    return false;
  }
  if (value.find_first_of("<>;{}\\\"") != std::string::npos) {
    return false;
  }
  std::string lower;
  lower.reserve(value.size());
  std::transform(value.begin(), value.end(), std::back_inserter(lower),
                 [](unsigned char c) { return std::tolower(c); });
  static const char* kBanned[] = {"javascript:", "expression(", "@import",
                                  "</", "<script"};
  for (const char* needle : kBanned) {
    if (lower.find(needle) != std::string::npos) {
      return false;
    }
  }
  return true;
}

JsonResponse CreateCollection(AppContext& app, RequestContext& ctx) {
  auto body = ctx.json_body();
  std::string title = Trim(JsonString(body, "title"));
  if (title.empty() || title.size() > kMaxTitleLen) {
    return JsonError(http_status::kBadRequest,
                     "Название должно быть от 1 до 200 символов");
  }
  if (JsonString(body, "description").size() > kMaxDescriptionLen) {
    return JsonError(http_status::kBadRequest, "Слишком длинное описание");
  }
  std::string hero_bg = JsonString(body, "hero_bg");
  if (!hero_bg.empty() && !SafeCssValue(hero_bg)) {
    return JsonError(http_status::kBadRequest, "Недопустимое значение фона");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto row = txn.exec_params(
      "INSERT INTO collections (slug, title, description, cover_image, "
      "hero_bg, created_by) VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
      OptText(JsonString(body, "slug")), title, JsonString(body, "description"),
      OptText(JsonString(body, "cover_image")),
      OptText(JsonString(body, "hero_bg")), *ctx.user_id);
  JsonResponse res =
      JsonOk(boost::json::object{{"id", row[0]["id"].as<long long>()}},
             http_status::kCreated);
  txn.commit();
  return res;
}

JsonResponse UpdateCollection(AppContext& app, RequestContext& ctx) {
  long long target_id = 0;
  if (!ParsePathId(ctx, "id", target_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto body = ctx.json_body();
  std::string title = Trim(JsonString(body, "title"));
  if (title.empty() || title.size() > kMaxTitleLen) {
    return JsonError(http_status::kBadRequest,
                     "Название должно быть от 1 до 200 символов");
  }
  if (JsonString(body, "description").size() > kMaxDescriptionLen) {
    return JsonError(http_status::kBadRequest, "Слишком длинное описание");
  }
  std::string hero_bg = JsonString(body, "hero_bg");
  if (!hero_bg.empty() && !SafeCssValue(hero_bg)) {
    return JsonError(http_status::kBadRequest, "Недопустимое значение фона");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto row = txn.exec_params(
      "UPDATE collections SET slug = $1, title = $2, description = $3, "
      "cover_image = $4, hero_bg = $5 WHERE id = $6 RETURNING id",
      OptText(JsonString(body, "slug")), title, JsonString(body, "description"),
      OptText(JsonString(body, "cover_image")),
      OptText(JsonString(body, "hero_bg")), target_id);
  if (row.empty()) {
    return JsonError(http_status::kNotFound, "Подборка не найдена");
  }
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}});
}

JsonResponse DeleteCollection(AppContext& app, RequestContext& ctx) {
  long long target_id = 0;
  if (!ParsePathId(ctx, "id", target_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  txn.exec_params("DELETE FROM collections WHERE id = $1", target_id);
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}});
}

JsonResponse AddWork(AppContext& app, RequestContext& ctx) {
  long long collection_id = 0;
  if (!ParsePathId(ctx, "id", collection_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto work_id = JsonInt(ctx.json_body(), "work_id");
  if (!work_id) {
    return JsonError(http_status::kBadRequest, "Укажите work_id");
  }
  long long position = JsonInt(ctx.json_body(), "position").value_or(0);
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  txn.exec_params(
      "INSERT INTO collection_works (collection_id, work_id, position) "
      "VALUES ($1, $2, $3) "
      "ON CONFLICT (collection_id, work_id) DO UPDATE SET position = $3",
      collection_id, *work_id, position);
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}});
}

JsonResponse RemoveWork(AppContext& app, RequestContext& ctx) {
  long long collection_id = 0;
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", collection_id) ||
      !ParsePathId(ctx, "workId", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  txn.exec_params(
      "DELETE FROM collection_works WHERE collection_id = $1 AND work_id = $2",
      collection_id, work_id);
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}});
}

JsonResponse SetStatus(AppContext& app, RequestContext& ctx, const char* table,
                       const std::string& first, const std::string& second) {
  long long target_id = 0;
  if (!ParsePathId(ctx, "id", target_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  std::string value = JsonString(ctx.json_body(), "status");
  if (value != first && value != second) {
    return JsonError(http_status::kBadRequest, "Недопустимый статус");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto row = txn.exec_params("UPDATE " + std::string(table) +
                                 " SET status = $1 WHERE id = $2 "
                                 "RETURNING id",
                             value, target_id);
  if (row.empty()) {
    return JsonError(http_status::kNotFound, "Не найдено");
  }
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}, {"status", value}});
}

JsonResponse AdminWorks(AppContext& app, RequestContext& ctx) {
  (void)ctx;
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec(
      "SELECT w.id, w.title, w.genre, w.status, w.created_at::text AS "
      "created_at, u.id AS author_id, u.username AS author_username, "
      "u.display_name AS author_name, "
      "u.avatar_url AS author_avatar FROM works w JOIN users u "
      "ON u.id = w.author_id ORDER BY w.created_at DESC LIMIT " +
      std::to_string(kAdminFeedLimit));
  boost::json::array items;
  for (auto row : rows) {
    items.push_back(
        boost::json::object{{"id", row["id"].as<long long>()},
                            {"title", row["title"].as<std::string>()},
                            {"genre", row["genre"].as<std::string>()},
                            {"status", row["status"].as<std::string>()},
                            {"created_at", row["created_at"].as<std::string>()},
                            {"author", AuthorBrief(row)}});
  }
  return JsonOk(boost::json::object{{"items", std::move(items)}});
}

JsonResponse AdminUsers(AppContext& app, RequestContext& ctx) {
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  std::string query = ctx.query_param("q").value_or("");
  std::string sql =
      "SELECT id, email, username, display_name, avatar_url, role, "
      "created_at::text AS created_at FROM users ";
  if (!query.empty()) {
    std::string like = txn.quote("%" + query + "%");
    sql += "WHERE display_name ILIKE " + like + " OR email ILIKE " + like +
           " OR username ILIKE " + like + " ";
  }
  sql += "ORDER BY role ASC, display_name ASC LIMIT " +
         std::to_string(kAdminFeedLimit);

  boost::json::array items;
  for (auto row : txn.exec(sql)) {
    items.push_back(boost::json::object{
        {"id", row["id"].as<long long>()},
        {"email", row["email"].as<std::string>()},
        {"username", row["username"].as<std::string>()},
        {"display_name", row["display_name"].as<std::string>()},
        {"avatar_url", FieldOrNull(row["avatar_url"])},
        {"role", row["role"].as<std::string>()},
        {"created_at", row["created_at"].as<std::string>()}});
  }
  return JsonOk(boost::json::object{{"items", std::move(items)}});
}

JsonResponse SetUserRole(AppContext& app, RequestContext& ctx) {
  long long target_id = 0;
  if (!ParsePathId(ctx, "id", target_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  std::string role = JsonString(ctx.json_body(), "role");
  if (role != "user" && role != "admin") {
    return JsonError(http_status::kBadRequest, "Недопустимая роль");
  }
  if (target_id == *ctx.user_id && role != "admin") {
    return JsonError(http_status::kForbidden, "Нельзя снять админа с себя");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto row = txn.exec_params(
      "UPDATE users SET role = $1 WHERE id = $2 RETURNING id", role, target_id);
  if (row.empty()) {
    return JsonError(http_status::kNotFound, "Пользователь не найден");
  }
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}, {"role", role}});
}

JsonResponse AdminComments(AppContext& app, RequestContext& ctx) {
  (void)ctx;
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec(
      "SELECT c.id, c.body, c.status, c.created_at::text AS created_at, "
      "c.work_id, u.id AS author_id, u.username AS author_username, "
      "u.display_name AS author_name, "
      "u.avatar_url AS author_avatar FROM comments c JOIN users u "
      "ON u.id = c.author_id ORDER BY c.created_at DESC LIMIT " +
      std::to_string(kAdminFeedLimit));
  boost::json::array items;
  for (auto row : rows) {
    items.push_back(
        boost::json::object{{"id", row["id"].as<long long>()},
                            {"body", row["body"].as<std::string>()},
                            {"status", row["status"].as<std::string>()},
                            {"created_at", row["created_at"].as<std::string>()},
                            {"work_id", row["work_id"].as<long long>()},
                            {"author", AuthorBrief(row)}});
  }
  return JsonOk(boost::json::object{{"items", std::move(items)}});
}

}  // namespace

void RegisterAdminRoutes(Router& router, AppContext& app) {
  router.add(
      "POST", "/api/collections", AuthLevel::Admin,
      [&app](RequestContext& ctx) { return CreateCollection(app, ctx); });
  router.add(
      "PUT", "/api/collections/:id", AuthLevel::Admin,
      [&app](RequestContext& ctx) { return UpdateCollection(app, ctx); });
  router.add(
      "DELETE", "/api/collections/:id", AuthLevel::Admin,
      [&app](RequestContext& ctx) { return DeleteCollection(app, ctx); });
  router.add("POST", "/api/collections/:id/works", AuthLevel::Admin,
             [&app](RequestContext& ctx) { return AddWork(app, ctx); });
  router.add("DELETE", "/api/collections/:id/works/:workId", AuthLevel::Admin,
             [&app](RequestContext& ctx) { return RemoveWork(app, ctx); });
  router.add("PATCH", "/api/admin/works/:id/status", AuthLevel::Admin,
             [&app](RequestContext& ctx) {
               return SetStatus(app, ctx, "works", "published", "hidden");
             });
  router.add("PATCH", "/api/admin/comments/:id/status", AuthLevel::Admin,
             [&app](RequestContext& ctx) {
               return SetStatus(app, ctx, "comments", "visible", "hidden");
             });
  router.add("GET", "/api/admin/works", AuthLevel::Admin,
             [&app](RequestContext& ctx) { return AdminWorks(app, ctx); });
  router.add("GET", "/api/admin/comments", AuthLevel::Admin,
             [&app](RequestContext& ctx) { return AdminComments(app, ctx); });
  router.add("GET", "/api/admin/users", AuthLevel::Admin,
             [&app](RequestContext& ctx) { return AdminUsers(app, ctx); });
  router.add("PATCH", "/api/admin/users/:id/role", AuthLevel::Admin,
             [&app](RequestContext& ctx) { return SetUserRole(app, ctx); });
}

}  // namespace tochka
