#include <string>
#include <utility>

#include "handlers.hpp"
#include "json_helpers.hpp"
#include "serialize.hpp"
#include "work_query.hpp"

namespace tochka {
namespace {

constexpr int kSearchLimit = 50;
// Запас по байтам под 280 символов UTF-8 (до 4 байт на символ).
constexpr std::size_t kMaxBioBytes = 1200;
constexpr std::size_t kMaxDisplayNameLen = 80;

JsonResponse SearchUsers(AppContext& app, RequestContext& ctx) {
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  std::string query = ctx.query_param("q").value_or("");
  std::string sql =
      "SELECT id, username, display_name, bio, avatar_url, role, "
      "created_at::text FROM users ";
  if (!query.empty()) {
    std::string like = txn.quote("%" + query + "%");
    sql +=
        "WHERE display_name ILIKE " + like + " OR username ILIKE " + like + " ";
  }
  sql += "ORDER BY created_at DESC LIMIT " + std::to_string(kSearchLimit);

  boost::json::array items;
  for (auto row : txn.exec(sql)) {
    items.push_back(UserPublic(row));
  }
  return JsonOk(boost::json::object{{"items", std::move(items)}});
}

JsonResponse Profile(AppContext& app, RequestContext& ctx) {
  long long user_id = 0;
  if (!ParsePathId(ctx, "id", user_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto users = txn.exec_params(
      "SELECT id, username, display_name, bio, avatar_url, role, "
      "created_at::text FROM users WHERE id = $1",
      user_id);
  if (users.empty()) {
    return JsonError(http_status::kNotFound, "Пользователь не найден");
  }

  bool can_see_hidden =
      ctx.is_admin() || (ctx.user_id && *ctx.user_id == user_id);
  std::string sql = WorkProjection(ctx.user_id.value_or(0)) +
                    "WHERE w.author_id = " + txn.quote(user_id);
  if (!can_see_hidden) {
    sql += " AND w.status = 'published'";
  }
  sql += " ORDER BY w.created_at DESC";

  boost::json::array works;
  for (auto row : txn.exec(sql)) {
    works.push_back(WorkRow(row));
  }
  return JsonOk(boost::json::object{{"user", UserPublic(users[0])},
                                    {"works", std::move(works)}});
}

JsonResponse UpdateMe(AppContext& app, RequestContext& ctx) {
  auto body = ctx.json_body();
  std::string display_name = Trim(JsonString(body, "display_name"));
  if (display_name.empty() || display_name.size() > kMaxDisplayNameLen) {
    return JsonError(http_status::kBadRequest,
                     "Укажите отображаемое имя (до 80 символов)");
  }
  std::string bio = Trim(JsonString(body, "bio"));
  if (bio.size() > kMaxBioBytes) {
    return JsonError(http_status::kBadRequest,
                     "Слишком длинное описание «о себе»");
  }
  auto avatar_url = OptText(Trim(JsonString(body, "avatar_url")));

  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec_params(
      "UPDATE users SET display_name = $1, bio = $2, avatar_url = $3 "
      "WHERE id = $4 "
      "RETURNING id, username, display_name, bio, avatar_url, role, "
      "created_at::text",
      display_name, bio, avatar_url, *ctx.user_id);
  if (rows.empty()) {
    return JsonError(http_status::kNotFound, "Пользователь не найден");
  }
  txn.commit();
  return JsonOk(boost::json::object{{"user", UserPublic(rows[0])}});
}

}  // namespace

void RegisterUserRoutes(Router& router, AppContext& app) {
  router.add("GET", "/api/users", AuthLevel::None,
             [&app](RequestContext& ctx) { return SearchUsers(app, ctx); });
  router.add("PATCH", "/api/users/me", AuthLevel::User,
             [&app](RequestContext& ctx) { return UpdateMe(app, ctx); });
  router.add("GET", "/api/users/:id", AuthLevel::None,
             [&app](RequestContext& ctx) { return Profile(app, ctx); });
}

}  // namespace tochka
