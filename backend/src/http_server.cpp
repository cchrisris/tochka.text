#include "http_server.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "auth.hpp"

namespace tochka {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

constexpr int kHexBase = 16;
constexpr int kHexLen = 2;
constexpr int kDefaultThreads = 4;
// Лимит тела запроса: бинарная загрузка картинок (аватары/обложки).
constexpr std::uint64_t kMaxBodyBytes = 24ULL * 1024 * 1024;

std::string UrlDecode(const std::string& input) {
  std::string out;
  for (std::size_t pos = 0; pos < input.size(); ++pos) {
    if (input[pos] == '%' && pos + kHexLen < input.size()) {
      std::string hex = input.substr(pos + 1, kHexLen);
      char* end = nullptr;
      long value = std::strtol(hex.c_str(), &end, kHexBase);
      if (end != hex.c_str()) {
        out.push_back(static_cast<char>(value));
        pos += kHexLen;
        continue;
      }
    }
    out.push_back(input[pos] == '+' ? ' ' : input[pos]);
  }
  return out;
}

void ParseQuery(const std::string& query_string,
                std::unordered_map<std::string, std::string>& query) {
  std::size_t start = 0;
  while (start < query_string.size()) {
    std::size_t amp = query_string.find('&', start);
    std::string pair = query_string.substr(
        start, amp == std::string::npos ? std::string::npos : amp - start);
    std::size_t sep = pair.find('=');
    if (sep != std::string::npos) {
      query[UrlDecode(pair.substr(0, sep))] = UrlDecode(pair.substr(sep + 1));
    } else if (!pair.empty()) {
      query[UrlDecode(pair)] = "";
    }
    if (amp == std::string::npos) {
      break;
    }
    start = amp + 1;
  }
}

void ParseTarget(const std::string& target, RequestContext& ctx) {
  std::size_t pos = target.find('?');
  if (pos == std::string::npos) {
    ctx.path = target;
    return;
  }
  ctx.path = target.substr(0, pos);
  ParseQuery(target.substr(pos + 1), ctx.query);
}

std::string VerbToString(http::verb verb) {
  switch (verb) {
    case http::verb::get:
      return "GET";
    case http::verb::post:
      return "POST";
    case http::verb::put:
      return "PUT";
    case http::verb::patch:
      return "PATCH";
    case http::verb::delete_:
      return "DELETE";
    default:
      return std::string(http::to_string(verb));
  }
}

// Достаёт значение cookie по имени из заголовка Cookie ("a=1; token=xyz").
std::optional<std::string> ReadCookie(
    const http::request<http::string_body>& req, const std::string& name) {
  auto found = req.find(http::field::cookie);
  if (found == req.end()) {
    return std::nullopt;
  }
  std::string header(found->value());
  std::string key = name + "=";
  std::size_t pos = 0;
  while (pos < header.size()) {
    std::size_t semi = header.find(';', pos);
    std::string part = header.substr(
        pos, semi == std::string::npos ? std::string::npos : semi - pos);
    std::size_t begin = part.find_first_not_of(" \t");
    if (begin != std::string::npos && part.compare(begin, key.size(), key) == 0) {
      return part.substr(begin + key.size());
    }
    if (semi == std::string::npos) {
      break;
    }
    pos = semi + 1;
  }
  return std::nullopt;
}

void ApplyAuth(const http::request<http::string_body>& req,
               const Config& config, RequestContext& ctx) {
  // Приоритет — cookie (основной способ для браузера); запасной — Bearer
  // (для программных клиентов). CSRF-защита нужна только для cookie-варианта.
  if (auto cookie = ReadCookie(req, "token")) {
    if (auto claims = auth::VerifyJwt(*cookie, config.jwt_secret)) {
      ctx.user_id = claims->user_id;
      ctx.role = claims->role;
      ctx.auth_via_cookie = true;
      return;
    }
  }
  auto found = req.find(http::field::authorization);
  if (found == req.end()) {
    return;
  }
  std::string header(found->value());
  std::string prefix = "Bearer ";
  if (header.rfind(prefix, 0) != 0) {
    return;
  }
  auto claims =
      auth::VerifyJwt(header.substr(prefix.size()), config.jwt_secret);
  if (claims) {
    ctx.user_id = claims->user_id;
    ctx.role = claims->role;
  }
}

// За обратным прокси (nginx) реальный IP клиента приходит в X-Forwarded-For.
// Бэкенд недоступен напрямую (порт не опубликован), поэтому заголовку доверяем
// и берём первый адрес из списка. Иначе rate-limit видел бы только IP прокси.
std::string ResolveClientIp(const http::request<http::string_body>& req,
                            const std::string& socket_ip) {
  auto found = req.find("X-Forwarded-For");
  if (found == req.end()) {
    return socket_ip;
  }
  std::string value(found->value());
  std::size_t comma = value.find(',');
  std::string first =
      comma == std::string::npos ? value : value.substr(0, comma);
  std::size_t start = first.find_first_not_of(" \t");
  if (start == std::string::npos) {
    return socket_ip;
  }
  std::size_t end = first.find_last_not_of(" \t");
  return first.substr(start, end - start + 1);
}

RequestContext BuildContext(const http::request<http::string_body>& req,
                            const Config& config, const std::string& client_ip) {
  RequestContext ctx;
  ctx.method = VerbToString(req.method());
  ParseTarget(std::string(req.target()), ctx);
  ctx.body = req.body();
  ctx.content_type = std::string(req[http::field::content_type]);
  ctx.client_ip = ResolveClientIp(req, client_ip);
  ctx.origin = std::string(req[http::field::origin]);
  ctx.host = std::string(req[http::field::host]);
  // За TLS-прокси схему сообщает nginx через X-Forwarded-Proto.
  ctx.is_https = std::string(req["X-Forwarded-Proto"]) == "https";
  ApplyAuth(req, config, ctx);
  return ctx;
}

bool IsMutating(const std::string& method) {
  return method == "POST" || method == "PUT" || method == "PATCH" ||
         method == "DELETE";
}

// CSRF: для cookie-аутентификации на мутирующих запросах требуем, чтобы Origin
// совпадал с Host. SameSite=Lax уже не даёт браузеру слать cookie кросс-сайтом,
// это дополнительный заслон. Запросы с Bearer-токеном не подвержены CSRF.
bool CsrfOk(const RequestContext& ctx) {
  if (!ctx.auth_via_cookie || !IsMutating(ctx.method)) {
    return true;
  }
  if (ctx.origin.empty()) {
    // Браузер всегда шлёт Origin на небезопасных методах; его отсутствие —
    // обычно не-браузерный клиент. Не блокируем, чтобы не ломать API-клиентов.
    return true;
  }
  std::size_t scheme = ctx.origin.find("://");
  std::string origin_host = scheme == std::string::npos
                                ? ctx.origin
                                : ctx.origin.substr(scheme + 3);
  return origin_host == ctx.host;
}

void AddCors(http::response<http::string_body>& res, const Config& config) {
  res.set("Access-Control-Allow-Origin", config.cors_origin);
  if (config.cors_origin != "*") {
    res.set("Vary", "Origin");
  }
  res.set("Access-Control-Allow-Methods",
          "GET, POST, PUT, PATCH, DELETE, OPTIONS");
  res.set("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

http::response<http::string_body> MakeBaseResponse(
    const http::request<http::string_body>& req, const Config& config) {
  http::response<http::string_body> res{http::status::ok, req.version()};
  res.set(http::field::server, "tochka-backend");
  res.set(http::field::content_type, "application/json");
  AddCors(res, config);
  res.keep_alive(req.keep_alive());
  return res;
}

http::response<http::string_body> HandleRequest(
    const http::request<http::string_body>& req, const Config& config,
    const Router& router, const std::string& client_ip) {
  http::response<http::string_body> res = MakeBaseResponse(req, config);
  if (req.method() == http::verb::options) {
    res.result(http::status::no_content);
    return res;
  }

  RequestContext ctx = BuildContext(req, config, client_ip);
  JsonResponse result;
  if (!CsrfOk(ctx)) {
    result = JsonError(http_status::kForbidden, "Запрос отклонён (CSRF)");
  } else {
    bool matched = false;
    try {
      result = router.dispatch(ctx, matched);
    } catch (const std::exception& e) {
      // Детали наружу не отдаём (могут содержать SQL/структуру БД) — только в лог.
      std::cerr << "[error] " << ctx.method << " " << ctx.path << ": "
                << e.what() << std::endl;
      result =
          JsonError(http_status::kServerError, "Внутренняя ошибка сервера");
    }
  }
  res.result(static_cast<unsigned>(result.status));
  for (const auto& [name, value] : result.extra_headers) {
    res.insert(name, value);
  }
  res.body() = boost::json::serialize(result.body);
  return res;
}

void HandleSession(tcp::socket socket, const Config& config,
                   const Router& router) {
  beast::error_code error;
  beast::flat_buffer buffer;
  std::string client_ip;
  {
    beast::error_code ep_ec;
    auto endpoint = socket.remote_endpoint(ep_ec);
    if (!ep_ec) {
      client_ip = endpoint.address().to_string();
    }
  }
  for (;;) {
    http::request_parser<http::string_body> parser;
    parser.body_limit(kMaxBodyBytes);
    http::read(socket, buffer, parser, error);
    if (error) {
      break;
    }
    http::request<http::string_body> req = parser.release();
    http::response<http::string_body> res =
        HandleRequest(req, config, router, client_ip);
    res.prepare_payload();
    http::write(socket, res, error);
    if (error || !res.keep_alive()) {
      break;
    }
  }
  socket.shutdown(tcp::socket::shutdown_send, error);
}

}  // namespace

HttpServer::HttpServer(const Config& config, const Router& router)
    : config_(config), router_(router) {}

void HttpServer::run(int threads) {
  asio::io_context ioc{threads};
  tcp::acceptor acceptor{ioc, tcp::endpoint{tcp::v4(), config_.port}};
  acceptor.set_option(asio::socket_base::reuse_address(true));
  std::cout << "tochka-backend listening on port " << config_.port << std::endl;

  asio::thread_pool pool(threads > 0 ? threads : kDefaultThreads);
  for (;;) {
    beast::error_code error;
    tcp::socket socket{ioc};
    acceptor.accept(socket, error);
    if (error) {
      continue;
    }
    asio::post(pool, [sock = std::move(socket), this]() mutable {
      HandleSession(std::move(sock), config_, router_);
    });
  }
}

}  // namespace tochka
