# Tochka.Text — форум для поэтов

Полноценный форум для публикации стихов и прозы: аккаунты, публикации, лайки,
комментарии, поиск, кураторские подборки и пост-модерация.

## Стек

- Бэкенд: C++17, Boost.Beast (HTTP) + Boost.Asio, libpqxx, libsodium, OpenSSL
- База данных: PostgreSQL 16.
- Фронтенд: React + Vite + TypeScript, React Router, axios.
- Прокси/TLS: Nginx (reverse proxy, статика, HTTPS) + Let's Encrypt (Certbot).
- Мониторинг: Prometheus, Grafana, Loki + Promtail, cAdvisor, node-exporter.
- Запуск: Docker Compose.

## Структура

```
backend/    C++ Boost.Beast REST API
frontend/   React SPA + Nginx-шаблоны (default.conf.template — прод, local.conf.template — локально)
db/
  migrations/  SQL-схема (применяется при первом старте Postgres)
  seed/        перенос старого альманаха (sections.json) в БД
content/    исходный sections.json (источник seed-данных)
pix/        иллюстрации (копируются в frontend/public/pix)
monitoring/ конфиги Prometheus, Loki, Promtail и провижининг Grafana
scripts/    init-letsencrypt.sh — первичный выпуск TLS-сертификатов
docker-compose.yml            прод: db + backend + frontend(nginx+TLS) + certbot
docker-compose.local.yml      оверлей для локального запуска по HTTP (порт 8080)
docker-compose.monitoring.yml отдельный стек мониторинга (проект tochka-monitoring)
```

## Локальный запуск (HTTP, без TLS)

```bash
cp .env.example .env
docker compose -f docker-compose.yml -f docker-compose.local.yml up -d --build db backend frontend_local
```

- Сайт: http://localhost:8080
- API проксируется тем же Nginx на `/api`

## Запуск в проде (HTTPS)

1. Заполните `.env`: укажите `DOMAIN`, `LETSENCRYPT_EMAIL`, `CORS_ORIGIN=https://<domain>`,
   смените `JWT_SECRET` и `POSTGRES_PASSWORD` (бэкенд не стартует с дефолтным секретом).
2. Выпустите сертификаты (DNS домена должен указывать на сервер, открыты порты 80/443):

```bash
./scripts/init-letsencrypt.sh
```

3. Поднимите стек:

```bash
docker compose up -d --build
```

Nginx терминирует TLS, отдаёт статику и проксирует `/api` и `/uploads` на бэкенд;
Certbot автоматически продлевает сертификаты.

## Перенос базового альманаха и назначение нулевого администратора

1. Зарегистрируйте аккаунт через интерфейс (например, `admin@tochka.local`).
2. Запустите перенос контента и назначьте себя админом:

```bash
ADMIN_EMAIL=admin@tochka.local docker compose run --rm seed
```

Скрипт создаёт подборки из разделов, импортирует произведения и их авторов
(импортированные авторы — placeholder-аккаунты без возможности входа), а также
повышает указанный email до роли `admin`.

## Основные эндпоинты API

| Метод | Путь | Назначение |
|------|------|-----------|
| POST | `/api/auth/register` `/api/auth/login` | регистрация / вход |
| POST | `/api/auth/logout` | выход (сброс cookie) |
| GET | `/api/auth/me` | текущий пользователь |
| GET | `/api/works` | лента (`q`, `genre`, `tag`, `author_id`, `collection_id`, `page`, `limit`) |
| GET/POST | `/api/works`, `/api/works/:id` | чтение / создание / правка / удаление |
| POST/DELETE | `/api/works/:id/like` | лайк |
| GET/POST | `/api/works/:id/comments` | комментарии |
| DELETE | `/api/comments/:id` | удаление комментария |
| GET | `/api/users`, `/api/users/:id` | поиск авторов и профиль |
| GET | `/api/collections`, `/api/collections/:id` | подборки |
| * | `/api/collections...`, `/api/admin/...` | управление подборками и модерация (только admin) |

## Модерация

Контент публикуется сразу. Администратор в разделе «Модерация» может скрывать
произведения и комментарии (пост-модерация), а также собирать кураторские подборки.

## Страница «О проекте»

Описание функционала и ссылка на репозиторий — на маршруте `/docs`
(ссылка есть в шапке и на лендинге).

## Аутентификация и безопасность

- JWT (HS256) хранится в httpOnly + Secure + SameSite=Lax cookie — недоступен из
  JavaScript. Пароли хешируются argon2id (libsodium).
- Защита от CSRF: проверка заголовка `Origin`/`Host` для мутаций при cookie-авторизации.
- Rate limiting на вход/регистрацию (по IP и email).
- Загрузка изображений проверяется по magic bytes, а не по `Content-Type`.
- Валидация длины текстовых полей; безопасная проверка значения `hero_bg` для предотвращения инъекций.
- Бэкенд не стартует с дефолтным/слабым `JWT_SECRET` (fail-fast).

## Мониторинг

Отдельный стек в `docker-compose.monitoring.yml` (изолированный проект
`tochka-monitoring`), не зависит от основного:

```bash
docker compose -f docker-compose.monitoring.yml up -d
```

- **Grafana** — порт `3000`, доступ только с `127.0.0.1`. Логин/пароль из `GRAFANA_ADMIN_USER`/
  `GRAFANA_ADMIN_PASSWORD`.

  Доступ к Grafana на проде — через SSH-туннель с локальной машины:

  ```bash
  ssh -L 3000:localhost:3000 <user>@<ip-сервера>
  ```
- **Prometheus** — метрики; **cAdvisor** — по контейнерам, **node-exporter** — по хосту.
- **Loki + Promtail** — логи всех контейнеров.
- Логи можно смотреть в Grafana → Explore (источник Loki), например
  `{container="tochkatext-backend-1"}`.

Остановить: `docker compose -f docker-compose.monitoring.yml down`.
