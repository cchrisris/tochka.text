#include <string>
#include <utility>

#include "handlers.hpp"
#include "json_helpers.hpp"
#include "serialize.hpp"
#include "work_query.hpp"

namespace tochka {
namespace {

constexpr const char* kCollectionSelect =
    "SELECT c.id, c.slug, c.title, c.description, c.cover_image, c.hero_bg, "
    "c.created_at::text AS created_at, cu.id AS curator_id, "
    "cu.username AS curator_username, "
    "cu.display_name AS curator_name, cu.avatar_url AS curator_avatar, "
    "(SELECT count(*) FROM collection_works cw "
    "WHERE cw.collection_id = c.id) AS works_count "
    "FROM collections c LEFT JOIN users cu ON cu.id = c.created_by ";

boost::json::value Curator(const pqxx::row& row) {
  if (row["curator_id"].is_null()) {
    return nullptr;
  }
  return boost::json::object{
      {"id", row["curator_id"].as<long long>()},
      {"username", row["curator_username"].as<std::string>()},
      {"display_name", row["curator_name"].as<std::string>()},
      {"avatar_url", FieldOrNull(row["curator_avatar"])}};
}

boost::json::object CollectionRow(const pqxx::row& row) {
  boost::json::object obj;
  obj["id"] = row["id"].as<long long>();
  obj["slug"] = FieldOrNull(row["slug"]);
  obj["title"] = row["title"].as<std::string>();
  obj["description"] = row["description"].as<std::string>();
  obj["cover_image"] = FieldOrNull(row["cover_image"]);
  obj["hero_bg"] = FieldOrNull(row["hero_bg"]);
  obj["created_at"] = row["created_at"].as<std::string>();
  obj["works_count"] = row["works_count"].as<long long>();
  obj["curator"] = Curator(row);
  return obj;
}

JsonResponse ListCollections(AppContext& app, RequestContext& ctx) {
  (void)ctx;
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  boost::json::array items;
  for (auto row : txn.exec(std::string(kCollectionSelect) +
                           "ORDER BY c.created_at DESC")) {
    items.push_back(CollectionRow(row));
  }
  return JsonOk(boost::json::object{{"items", std::move(items)}});
}

JsonResponse GetCollection(AppContext& app, RequestContext& ctx) {
  long long target_id = 0;
  if (!ParsePathId(ctx, "id", target_id)) {
    return JsonError(http_status::kBadRequest, "Некорректный идентификатор");
  }
  auto conn = app.db.acquire();
  pqxx::work txn(conn.get());
  auto collections = txn.exec(std::string(kCollectionSelect) +
                              "WHERE c.id = " + txn.quote(target_id));
  if (collections.empty()) {
    return JsonError(http_status::kNotFound, "Подборка не найдена");
  }

  std::string sql =
      WorkProjection(ctx.user_id.value_or(0)) +
      "JOIN collection_works cw ON cw.work_id = w.id WHERE cw.collection_id "
      "= " +
      txn.quote(target_id) +
      " AND w.status = 'published' ORDER BY cw.position ASC, w.created_at DESC";
  boost::json::array works;
  for (auto row : txn.exec(sql)) {
    works.push_back(WorkRow(row));
  }
  return JsonOk(
      boost::json::object{{"collection", CollectionRow(collections[0])},
                          {"works", std::move(works)}});
}

}  // namespace

void RegisterCollectionRoutes(Router& router, AppContext& app) {
  router.add("GET", "/api/collections", AuthLevel::None,
             [&app](RequestContext& ctx) { return ListCollections(app, ctx); });
  router.add("GET", "/api/collections/:id", AuthLevel::None,
             [&app](RequestContext& ctx) { return GetCollection(app, ctx); });
}

}  // namespace tochka
