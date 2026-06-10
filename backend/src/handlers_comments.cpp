#include <string>
#include <utility>

#include "handlers.hpp"
#include "json_helpers.hpp"
#include "profanity_filter.hpp"
#include "serialize.hpp"

namespace tochka {
namespace {

constexpr std::size_t kMaxCommentLen = 5000;

long long LikesCount(pqxx::work &txn, long long work_id) {
  return txn
      .exec_params("SELECT count(*) AS c FROM likes WHERE work_id = $1",
                   work_id)[0]["c"]
      .as<long long>();
}

boost::json::object CommentRow(const pqxx::row &row) {
  boost::json::object obj;
  obj["id"] = row["id"].as<long long>();
  obj["body"] = row["body"].as<std::string>();
  obj["created_at"] = row["created_at"].as<std::string>();
  obj["author"] = AuthorBrief(row);
  return obj;
}

JsonResponse ListComments(AppContext &app, RequestContext &ctx) {
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec_params(
      "SELECT c.id, c.body, c.created_at::text AS created_at, "
      "u.id AS author_id, u.username AS author_username, "
      "u.display_name AS author_name, u.avatar_url AS author_avatar "
      "FROM comments c JOIN users u ON u.id = c.author_id "
      "WHERE c.work_id = $1 AND c.status = 'visible' "
      "ORDER BY c.created_at ASC",
      work_id);
  boost::json::array items;
  for (auto row : rows) {
    items.push_back(CommentRow(row));
  }
  return JsonOk(boost::json::object{{"items", std::move(items)}});
}

JsonResponse CreateComment(AppContext &app, RequestContext &ctx) {
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  std::string text = Trim(JsonString(ctx.json_body(), "body"));
  if (text.empty()) {
    return JsonError(http_status::kBadRequest, "Комментарий пустой");
  }
  if (text.size() > kMaxCommentLen) {
    return JsonError(http_status::kBadRequest, "Комментарий слишком длинный");
  }
  if (ContainsProfanity(text)) {
    return JsonError(http_status::kBadRequest, kCommentProfanityErrorMessage);
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  if (txn.exec_params("SELECT 1 FROM works WHERE id = $1", work_id).empty()) {
    return JsonError(http_status::kNotFound, "Произведение не найдено");
  }
  auto rows = txn.exec_params(
      "WITH inserted AS (INSERT INTO comments (work_id, author_id, body) "
      "VALUES ($1, $2, $3) RETURNING id, author_id, body, created_at) "
      "SELECT i.id, i.body, i.created_at::text AS created_at, "
      "u.id AS author_id, u.username AS author_username, "
      "u.display_name AS author_name, u.avatar_url AS author_avatar "
      "FROM inserted i JOIN users u ON u.id = i.author_id",
      work_id, *ctx.user_id, text);
  JsonResponse res =
      JsonOk(boost::json::object{{"comment", CommentRow(rows[0])}},
             http_status::kCreated);
  txn.commit();
  return res;
}

JsonResponse DeleteComment(AppContext &app, RequestContext &ctx) {
  long long comment_id = 0;
  if (!ParsePathId(ctx, "id", comment_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec_params("SELECT author_id FROM comments WHERE id = $1",
                              comment_id);
  if (rows.empty()) {
    return JsonError(http_status::kNotFound, "Комментарий не найден");
  }
  bool owner = rows[0]["author_id"].as<long long>() == *ctx.user_id;
  if (!owner && !ctx.is_admin()) {
    return JsonError(http_status::kForbidden, "Нет прав на удаление");
  }
  txn.exec_params("DELETE FROM comments WHERE id = $1", comment_id);
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}});
}

JsonResponse Like(AppContext &app, RequestContext &ctx) {
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  if (txn.exec_params("SELECT 1 FROM works WHERE id = $1", work_id).empty()) {
    return JsonError(http_status::kNotFound, "Произведение не найдено");
  }
  txn.exec_params("INSERT INTO likes (user_id, work_id) VALUES ($1, $2) "
                  "ON CONFLICT DO NOTHING",
                  *ctx.user_id, work_id);
  long long count = LikesCount(txn, work_id);
  txn.commit();
  return JsonOk(boost::json::object{{"liked", true}, {"likes_count", count}});
}

JsonResponse Unlike(AppContext &app, RequestContext &ctx) {
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  txn.exec_params("DELETE FROM likes WHERE user_id = $1 AND work_id = $2",
                  *ctx.user_id, work_id);
  long long count = LikesCount(txn, work_id);
  txn.commit();
  return JsonOk(boost::json::object{{"liked", false}, {"likes_count", count}});
}

} // namespace

void RegisterCommentRoutes(Router &router, AppContext &app) {
  router.add("GET", "/api/works/:id/comments", AuthLevel::None,
             [&app](RequestContext &ctx) { return ListComments(app, ctx); });
  router.add("POST", "/api/works/:id/comments", AuthLevel::User,
             [&app](RequestContext &ctx) { return CreateComment(app, ctx); });
  router.add("DELETE", "/api/comments/:id", AuthLevel::User,
             [&app](RequestContext &ctx) { return DeleteComment(app, ctx); });
  router.add("POST", "/api/works/:id/like", AuthLevel::User,
             [&app](RequestContext &ctx) { return Like(app, ctx); });
  router.add("DELETE", "/api/works/:id/like", AuthLevel::User,
             [&app](RequestContext &ctx) { return Unlike(app, ctx); });
}

} // namespace tochka
