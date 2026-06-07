#!/usr/bin/env bash
# Первичный выпуск сертификата Let's Encrypt для единого nginx.
# Запускать ОДИН раз после того, как DNS домена указывает на этот сервер,
# а порты 80 и 443 открыты. Дальнейшее продление делает сервис certbot сам.
#
#   ./scripts/init-letsencrypt.sh
#
# Параметры берутся из .env (DOMAIN, LETSENCRYPT_EMAIL).
# Для проверки без лимитов Let's Encrypt: STAGING=1 ./scripts/init-letsencrypt.sh
set -euo pipefail

cd "$(dirname "$0")/.."

if ! docker compose version >/dev/null 2>&1; then
  echo "Нужен docker compose (v2)." >&2
  exit 1
fi

if [ ! -f .env ]; then
  echo "Не найден .env — создайте его из .env.example." >&2
  exit 1
fi

DOMAIN=$(grep -E '^DOMAIN=' .env | head -n1 | cut -d= -f2-)
EMAIL=$(grep -E '^LETSENCRYPT_EMAIL=' .env | head -n1 | cut -d= -f2-)
STAGING="${STAGING:-0}"
RSA_KEY_SIZE=4096
CERT_PATH="certbot/conf/live/${DOMAIN}"

if [ -z "${DOMAIN}" ] || [ "${DOMAIN}" = "example.com" ]; then
  echo "Задайте реальный DOMAIN в .env." >&2
  exit 1
fi
if [ -z "${EMAIL}" ] || [ "${EMAIL}" = "you@example.com" ]; then
  echo "Задайте реальный LETSENCRYPT_EMAIL в .env." >&2
  exit 1
fi

echo "### Домен: ${DOMAIN}; email: ${EMAIL}; staging=${STAGING}"
mkdir -p certbot/conf certbot/www

if [ -d "${CERT_PATH}" ]; then
  read -r -p "Сертификат для ${DOMAIN} уже существует. Перевыпустить? (y/N) " ans
  case "${ans}" in [yY]*) ;; *) echo "Отмена."; exit 0;; esac
fi

echo "### Создаю временный самоподписанный сертификат, чтобы nginx смог стартовать…"
mkdir -p "${CERT_PATH}"
docker compose run --rm --entrypoint "\
  openssl req -x509 -nodes -newkey rsa:${RSA_KEY_SIZE} -days 1 \
    -keyout '/etc/letsencrypt/live/${DOMAIN}/privkey.pem' \
    -out '/etc/letsencrypt/live/${DOMAIN}/fullchain.pem' \
    -subj '/CN=localhost'" certbot

echo "### Поднимаю nginx (frontend)…"
docker compose up -d --build frontend

echo "### Удаляю временный сертификат…"
docker compose run --rm --entrypoint "\
  rm -rf /etc/letsencrypt/live/${DOMAIN} \
    /etc/letsencrypt/archive/${DOMAIN} \
    /etc/letsencrypt/renewal/${DOMAIN}.conf" certbot

echo "### Запрашиваю настоящий сертификат у Let's Encrypt…"
staging_arg=""
if [ "${STAGING}" != "0" ]; then staging_arg="--staging"; fi

docker compose run --rm --entrypoint "\
  certbot certonly --webroot -w /var/www/certbot \
    ${staging_arg} \
    --email ${EMAIL} \
    -d ${DOMAIN} \
    --rsa-key-size ${RSA_KEY_SIZE} \
    --agree-tos \
    --no-eff-email \
    --force-renewal" certbot

echo "### Перечитываю конфиг nginx…"
docker compose exec frontend nginx -s reload

echo "### Готово. Сайт доступен по https://${DOMAIN}"
