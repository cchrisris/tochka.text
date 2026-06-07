-- Tochka.Text — схема БД форума для поэтов
-- Выполняется автоматически при первой инициализации контейнера postgres.

CREATE EXTENSION IF NOT EXISTS pgcrypto;

-- Пользователи
CREATE TABLE IF NOT EXISTS users (
    id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email         TEXT NOT NULL UNIQUE,
    username      TEXT NOT NULL UNIQUE
                  CHECK (username ~ '^[a-z0-9_]{3,20}$'),
    password_hash TEXT NOT NULL,
    display_name  TEXT NOT NULL,
    bio           TEXT NOT NULL DEFAULT '',
    avatar_url    TEXT,
    role          TEXT NOT NULL DEFAULT 'user' CHECK (role IN ('user', 'admin')),
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Произведения (стихи/проза)
CREATE TABLE IF NOT EXISTS works (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    author_id   BIGINT NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    title       TEXT NOT NULL DEFAULT '',
    body        TEXT NOT NULL DEFAULT '',
    genre       TEXT NOT NULL DEFAULT 'poem' CHECK (genre IN ('poem', 'prose')),
    cover_image TEXT,
    status      TEXT NOT NULL DEFAULT 'published' CHECK (status IN ('published', 'hidden')),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_works_author ON works (author_id);
CREATE INDEX IF NOT EXISTS idx_works_created ON works (created_at DESC);

-- Теги
CREATE TABLE IF NOT EXISTS tags (
    id   BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS work_tags (
    work_id BIGINT NOT NULL REFERENCES works (id) ON DELETE CASCADE,
    tag_id  BIGINT NOT NULL REFERENCES tags (id) ON DELETE CASCADE,
    PRIMARY KEY (work_id, tag_id)
);

-- Кураторские подборки (бывшие «разделы»)
CREATE TABLE IF NOT EXISTS collections (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    slug        TEXT UNIQUE,
    title       TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    cover_image TEXT,
    hero_bg     TEXT,
    created_by  BIGINT REFERENCES users (id) ON DELETE SET NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS collection_works (
    collection_id BIGINT NOT NULL REFERENCES collections (id) ON DELETE CASCADE,
    work_id       BIGINT NOT NULL REFERENCES works (id) ON DELETE CASCADE,
    position      INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (collection_id, work_id)
);

-- Лайки
CREATE TABLE IF NOT EXISTS likes (
    user_id    BIGINT NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    work_id    BIGINT NOT NULL REFERENCES works (id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, work_id)
);

-- Комментарии
CREATE TABLE IF NOT EXISTS comments (
    id         BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    work_id    BIGINT NOT NULL REFERENCES works (id) ON DELETE CASCADE,
    author_id  BIGINT NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    body       TEXT NOT NULL,
    status     TEXT NOT NULL DEFAULT 'visible' CHECK (status IN ('visible', 'hidden')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_comments_work ON comments (work_id);

-- Полнотекстовый поиск по произведениям (простая версия через trigram)
CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE INDEX IF NOT EXISTS idx_works_title_trgm ON works USING gin (title gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_users_name_trgm ON users USING gin (display_name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_users_username_trgm ON users USING gin (username gin_trgm_ops);
