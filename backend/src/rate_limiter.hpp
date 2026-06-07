#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace tochka {

// Простой потокобезопасный rate-limiter со скользящим окном.
// Хранит метки времени последних попыток по ключу (например, IP или email)
// и отклоняет, если за окно их накопилось больше лимита.
class RateLimiter {
 public:
  RateLimiter(std::size_t max_hits, std::chrono::seconds window)
      : max_hits_(max_hits), window_(window) {}

  // Регистрирует попытку и возвращает true, если она в пределах лимита.
  bool allow(const std::string& key) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);

    if (++since_sweep_ > kSweepEvery) {
      sweep(now);
      since_sweep_ = 0;
    }

    auto& hits = buckets_[key];
    while (!hits.empty() && now - hits.front() > window_) {
      hits.pop_front();
    }
    if (hits.size() >= max_hits_) {
      return false;
    }
    hits.push_back(now);
    return true;
  }

 private:
  using Clock = std::chrono::steady_clock;

  static constexpr int kSweepEvery = 256;

  // Убирает протухшие ключи, чтобы карта не росла бесконечно.
  void sweep(Clock::time_point now) {
    for (auto it = buckets_.begin(); it != buckets_.end();) {
      auto& hits = it->second;
      while (!hits.empty() && now - hits.front() > window_) {
        hits.pop_front();
      }
      it = hits.empty() ? buckets_.erase(it) : std::next(it);
    }
  }

  std::size_t max_hits_;
  std::chrono::seconds window_;
  std::mutex mutex_;
  std::unordered_map<std::string, std::deque<Clock::time_point>> buckets_;
  int since_sweep_ = 0;
};

}  // namespace tochka
