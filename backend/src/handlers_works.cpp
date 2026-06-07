#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "handlers.hpp"
#include "json_helpers.hpp"
#include "serialize.hpp"
#include "work_query.hpp"

namespace tochka {
namespace {

constexpr int kDefaultLimit = 20;
constexpr int kMaxLimit = 50;
constexpr int kMaxPage = 100000;
constexpr std::size_t kMaxTags = 20;
constexpr std::size_t kMaxTitleLen = 300;
constexpr std::size_t kMaxBodyLen = 100000;
constexpr std::size_t kMaxTagLen = 50;

enum class OwnerCheck { NotFound, Forbidden, Ok };

std::vector<std::string> ParseTags(const boost::json::value& body) {
  std::vector<std::string> tags;
  if (!body.is_object()) {
    return tags;
  }
  const auto* entry = body.as_object().find("tags");
  if (entry == body.as_object().end() || !entry->value().is_array()) {
    return tags;
  }
  for (const auto& element : entry->value().as_array()) {
    if (!element.is_string()) {
      continue;
    }
    std::string tag = Trim(std::string(element.as_string().c_str()));
    if (!tag.empty() && tag.size() <= kMaxTagLen && tags.size() < kMaxTags) {
      tags.push_back(tag);
    }
  }
  return tags;
}

void SyncTags(pqxx::work& txn, long long work_id,
              const std::vector<std::string>& tags) {
  txn.exec_params("DELETE FROM work_tags WHERE work_id = $1", work_id);
  for (const auto& name : tags) {
    auto row = txn.exec_params(
        "INSERT INTO tags (name) VALUES ($1) "
        "ON CONFLICT (name) DO UPDATE SET name = EXCLUDED.name RETURNING id",
        name);
    txn.exec_params(
        "INSERT INTO work_tags (work_id, tag_id) VALUES ($1, $2) "
        "ON CONFLICT DO NOTHING",
        work_id, row[0]["id"].as<long long>());
  }
}

int ClampInt(const std::string& text, int fallback, int low, int high) {
  try {
    return std::clamp(std::stoi(text), low, high);
  } catch (const std::exception&) {
    return fallback;
  }
}

std::string NormalizeGenre(const std::string& genre) {
  return genre == "prose" ? "prose" : "poem";
}

bool TooLong(const std::string& title, const std::string& body) {
  return title.size() > kMaxTitleLen || body.size() > kMaxBodyLen;
}

std::string BuildWhere(pqxx::work& txn, const RequestContext& ctx) {
  std::vector<std::string> where{"w.status = 'published'"};
  if (auto genre = ctx.query_param("genre");
      genre && (*genre == "poem" || *genre == "prose")) {
    where.push_back("w.genre = " + txn.quote(*genre));
  }
  if (auto author = ctx.query_param("author_id"); author && !author->empty()) {
    where.push_back("w.author_id = " + txn.quote(*author));
  }
  if (auto collection = ctx.query_param("collection_id");
      collection && !collection->empty()) {
    where.push_back(
        "EXISTS(SELECT 1 FROM collection_works cw WHERE cw.work_id = w.id "
        "AND cw.collection_id = " +
        txn.quote(*collection) + ")");
  }
  if (auto tag = ctx.query_param("tag"); tag && !tag->empty()) {
    where.push_back(
        "EXISTS(SELECT 1 FROM work_tags wt JOIN tags tg ON tg.id = wt.tag_id "
        "WHERE wt.work_id = w.id AND tg.name = " +
        txn.quote(*tag) + ")");
  }
  if (auto query = ctx.query_param("q"); query && !query->empty()) {
    std::string like = txn.quote("%" + *query + "%");
    where.push_back("(w.title ILIKE " + like + " OR w.body ILIKE " + like +
                    " OR u.display_name ILIKE " + like + ")");
  }
  std::string clause;
  for (std::size_t idx = 0; idx < where.size(); ++idx) {
    clause += (idx == 0 ? "" : " AND ") + where[idx];
  }
  return clause;
}

boost::json::object SelectWork(pqxx::work& txn, long long work_id,
                               long long viewer_id) {
  auto rows = txn.exec(WorkProjection(viewer_id) +
                       "WHERE w.id = " + txn.quote(work_id));
  return WorkRow(rows[0]);
}

OwnerCheck CheckOwner(pqxx::work& txn, long long work_id,
                      const RequestContext& ctx) {
  auto rows =
      txn.exec_params("SELECT author_id FROM works WHERE id = $1", work_id);
  if (rows.empty()) {
    return OwnerCheck::NotFound;
  }
  bool owner =
      ctx.user_id && rows[0]["author_id"].as<long long>() == *ctx.user_id;
  return (owner || ctx.is_admin()) ? OwnerCheck::Ok : OwnerCheck::Forbidden;
}

JsonResponse Feed(AppContext& app, RequestContext& ctx) {
  long long viewer_id = ctx.user_id.value_or(0);
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());

  int limit = ClampInt(ctx.query_param("limit").value_or(""), kDefaultLimit, 1,
                       kMaxLimit);
  int page = ClampInt(ctx.query_param("page").value_or(""), 1, 1, kMaxPage);
  std::string sql =
      WorkProjection(viewer_id) + "WHERE " + BuildWhere(txn, ctx) +
      " ORDER BY w.created_at DESC LIMIT " + std::to_string(limit) +
      " OFFSET " + std::to_string((page - 1) * limit);

  boost::json::array items;
  for (auto row : txn.exec(sql)) {
    items.push_back(WorkRow(row));
  }
  return JsonOk(boost::json::object{
      {"items", std::move(items)}, {"page", page}, {"limit", limit}});
}

bool VisibleTo(const pqxx::row& row, const RequestContext& ctx) {
  if (row["status"].as<std::string>() != "hidden") {
    return true;
  }
  return ctx.is_admin() ||
         (ctx.user_id && row["author_id"].as<long long>() == *ctx.user_id);
}

JsonResponse GetWork(AppContext& app, RequestContext& ctx) {
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  long long viewer_id = ctx.user_id.value_or(0);
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto rows = txn.exec(WorkProjection(viewer_id) +
                       "WHERE w.id = " + txn.quote(work_id));
  if (rows.empty() || !VisibleTo(rows[0], ctx)) {
    return JsonError(http_status::kNotFound, "Произведение не найдено");
  }
  return JsonOk(boost::json::object{{"work", WorkRow(rows[0])}});
}

JsonResponse CreateWork(AppContext& app, RequestContext& ctx) {
  auto body = ctx.json_body();
  std::string title = JsonString(body, "title");
  std::string text = JsonString(body, "body");
  if (title.empty() && text.empty()) {
    return JsonError(http_status::kBadRequest, "Произведение пустое");
  }
  if (TooLong(title, text)) {
    return JsonError(http_status::kBadRequest, "Произведение слишком длинное");
  }

  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto inserted = txn.exec_params(
      "INSERT INTO works (author_id, title, body, genre, cover_image) "
      "VALUES ($1, $2, $3, $4, $5) RETURNING id",
      *ctx.user_id, title, text, NormalizeGenre(JsonString(body, "genre")),
      OptText(JsonString(body, "cover_image")));
  long long work_id = inserted[0]["id"].as<long long>();
  SyncTags(txn, work_id, ParseTags(body));
  JsonResponse res = JsonOk(
      boost::json::object{{"work", SelectWork(txn, work_id, *ctx.user_id)}},
      http_status::kCreated);
  txn.commit();
  return res;
}

JsonResponse UpdateWork(AppContext& app, RequestContext& ctx) {
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto body = ctx.json_body();
  if (TooLong(JsonString(body, "title"), JsonString(body, "body"))) {
    return JsonError(http_status::kBadRequest, "Произведение слишком длинное");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());

  OwnerCheck check = CheckOwner(txn, work_id, ctx);
  if (check == OwnerCheck::NotFound) {
    return JsonError(http_status::kNotFound, "Произведение не найдено");
  }
  if (check == OwnerCheck::Forbidden) {
    return JsonError(http_status::kForbidden, "Нет прав на редактирование");
  }

  txn.exec_params(
      "UPDATE works SET title = $1, body = $2, genre = $3, cover_image = $4, "
      "updated_at = now() WHERE id = $5",
      JsonString(body, "title"), JsonString(body, "body"),
      NormalizeGenre(JsonString(body, "genre")),
      OptText(JsonString(body, "cover_image")), work_id);
  SyncTags(txn, work_id, ParseTags(body));
  JsonResponse res = JsonOk(
      boost::json::object{{"work", SelectWork(txn, work_id, *ctx.user_id)}});
  txn.commit();
  return res;
}

JsonResponse DeleteWork(AppContext& app, RequestContext& ctx) {
  long long work_id = 0;
  if (!ParsePathId(ctx, "id", work_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  OwnerCheck check = CheckOwner(txn, work_id, ctx);
  if (check == OwnerCheck::NotFound) {
    return JsonError(http_status::kNotFound, "Произведение не найдено");
  }
  if (check == OwnerCheck::Forbidden) {
    return JsonError(http_status::kForbidden, "Нет прав на удаление");
  }
  txn.exec_params("DELETE FROM works WHERE id = $1", work_id);
  txn.commit();
  return JsonOk(boost::json::object{{"ok", true}});
}

}  // namespace

void RegisterWorksRoutes(Router& router, AppContext& app) {
  router.add("GET", "/api/works", AuthLevel::None,
             [&app](RequestContext& ctx) { return Feed(app, ctx); });
  router.add("GET", "/api/works/:id", AuthLevel::None,
             [&app](RequestContext& ctx) { return GetWork(app, ctx); });
  router.add("POST", "/api/works", AuthLevel::User,
             [&app](RequestContext& ctx) { return CreateWork(app, ctx); });
  router.add("PUT", "/api/works/:id", AuthLevel::User,
             [&app](RequestContext& ctx) { return UpdateWork(app, ctx); });
  router.add("DELETE", "/api/works/:id", AuthLevel::User,
             [&app](RequestContext& ctx) { return DeleteWork(app, ctx); });
}

}  // namespace tochka
