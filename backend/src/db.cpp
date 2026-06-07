#include "db.hpp"

#include <memory>
#include <utility>

namespace tochka {

PooledConnection::PooledConnection(Database& pool,
                                   std::unique_ptr<pqxx::connection> conn)
    : pool_(&pool), conn_(std::move(conn)) {}

PooledConnection::~PooledConnection() {
  if (pool_ != nullptr && conn_) {
    pool_->release(std::move(conn_));
  }
}

Database::Database(const std::string& conn_string, std::size_t pool_size)
    : conn_string_(conn_string) {
  for (std::size_t i = 0; i < pool_size; ++i) {
    pool_.push(std::make_unique<pqxx::connection>(conn_string_));
  }
}

PooledConnection Database::acquire() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return !pool_.empty(); });
  auto conn = std::move(pool_.front());
  pool_.pop();
  lock.unlock();

  if (!conn->is_open()) {
    conn = std::make_unique<pqxx::connection>(conn_string_);
  }
  return PooledConnection(*this, std::move(conn));
}

void Database::release(std::unique_ptr<pqxx::connection> conn) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push(std::move(conn));
  }
  cv_.notify_one();
}

}  // namespace tochka
