#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <queue>
#include <string>

namespace tochka {

class Database;

class PooledConnection {
 public:
  PooledConnection(Database& pool, std::unique_ptr<pqxx::connection> conn);
  ~PooledConnection();

  PooledConnection(const PooledConnection&) = delete;
  PooledConnection& operator=(const PooledConnection&) = delete;
  PooledConnection(PooledConnection&&) = default;
  PooledConnection& operator=(PooledConnection&&) = default;

  pqxx::connection& get() { return *conn_; }
  pqxx::connection* operator->() { return conn_.get(); }

 private:
  Database* pool_;
  std::unique_ptr<pqxx::connection> conn_;
};

class Database {
 public:
  Database(const std::string& conn_string, std::size_t pool_size);

  PooledConnection acquire();

 private:
  friend class PooledConnection;
  void release(std::unique_ptr<pqxx::connection> conn);

  std::string conn_string_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::unique_ptr<pqxx::connection>> pool_;
};

}  // namespace tochka
