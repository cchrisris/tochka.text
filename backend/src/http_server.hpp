#pragma once

#include "config.hpp"
#include "router.hpp"

namespace tochka {

class HttpServer {
 public:
  HttpServer(const Config& config, const Router& router);

  void run(int threads);

 private:
  const Config& config_;
  const Router& router_;
};

}  // namespace tochka
