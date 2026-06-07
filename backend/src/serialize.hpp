#pragma once

#include <boost/json.hpp>
#include <pqxx/pqxx>
#include <string>

namespace tochka {

inline boost::json::value FieldOrNull(const pqxx::field& field) {
  if (field.is_null()) {
    return nullptr;
  }
  return boost::json::value(std::string(field.c_str()));
}

inline boost::json::object UserPublic(const pqxx::row& row) {
  boost::json::object obj;
  obj["id"] = row["id"].as<long long>();
  obj["username"] = row["username"].as<std::string>();
  obj["display_name"] = row["display_name"].as<std::string>();
  obj["bio"] =
      row["bio"].is_null() ? std::string() : row["bio"].as<std::string>();
  obj["avatar_url"] = FieldOrNull(row["avatar_url"]);
  obj["role"] = row["role"].as<std::string>();
  obj["created_at"] = row["created_at"].as<std::string>();
  return obj;
}

inline boost::json::object AuthorBrief(const pqxx::row& row) {
  boost::json::object obj;
  obj["id"] = row["author_id"].as<long long>();
  obj["username"] = row["author_username"].as<std::string>();
  obj["display_name"] = row["author_name"].as<std::string>();
  obj["avatar_url"] = FieldOrNull(row["author_avatar"]);
  return obj;
}

inline boost::json::array ParsePgArray(const pqxx::field& field) {
  boost::json::array arr;
  if (field.is_null()) {
    return arr;
  }
  pqxx::array_parser parser(field.view());
  auto elem = parser.get_next();
  while (elem.first != pqxx::array_parser::juncture::done) {
    if (elem.first == pqxx::array_parser::juncture::string_value) {
      arr.push_back(boost::json::value(elem.second));
    }
    elem = parser.get_next();
  }
  return arr;
}

inline boost::json::object WorkRow(const pqxx::row& row) {
  boost::json::object obj;
  obj["id"] = row["id"].as<long long>();
  obj["title"] = row["title"].as<std::string>();
  obj["body"] = row["body"].as<std::string>();
  obj["genre"] = row["genre"].as<std::string>();
  obj["cover_image"] = FieldOrNull(row["cover_image"]);
  obj["status"] = row["status"].as<std::string>();
  obj["created_at"] = row["created_at"].as<std::string>();
  obj["author"] = AuthorBrief(row);
  obj["likes_count"] = row["likes_count"].as<long long>();
  obj["comments_count"] = row["comments_count"].as<long long>();
  obj["liked_by_me"] = row["liked_by_me"].as<bool>();
  obj["tags"] = ParsePgArray(row["tags"]);
  return obj;
}

}  // namespace tochka
