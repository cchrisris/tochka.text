// Перенос статического альманаха (content/sections.json) в базу форума.
// Разделы -> подборки, авторы -> пользователи (placeholder, без входа), pieces -> произведения.
// Запуск: docker compose run --rm seed
import { readFileSync } from "node:fs";
import crypto from "node:crypto";
import pg from "pg";

const { Pool } = pg;

const DATABASE_URL =
  process.env.DATABASE_URL ||
  "postgresql://tochka:tochka_dev_password@db:5432/tochka";
const CONTENT_PATH = process.env.CONTENT_PATH || "/content/sections.json";
const ADMIN_EMAIL = process.env.ADMIN_EMAIL || "";

// Заглушка вместо хеша пароля: argon2-проверка на бэкенде её отвергнет,
// поэтому в импортированные аккаунты войти нельзя (как и задумано).
const PLACEHOLDER_HASH = "imported-account-no-login";

const pool = new Pool({ connectionString: DATABASE_URL });

function emailForAuthor(name) {
  const hash = crypto.createHash("sha1").update(name).digest("hex").slice(0, 16);
  return `author.${hash}@imported.tochka.local`;
}

const TRANSLIT = {
  а: "a", б: "b", в: "v", г: "g", д: "d", е: "e", ё: "e", ж: "zh", з: "z",
  и: "i", й: "i", к: "k", л: "l", м: "m", н: "n", о: "o", п: "p", р: "r",
  с: "s", т: "t", у: "u", ф: "f", х: "h", ц: "c", ч: "ch", ш: "sh", щ: "sch",
  ъ: "", ы: "y", ь: "", э: "e", ю: "yu", я: "ya",
};

function transliterate(text) {
  let out = "";
  for (const ch of text.toLowerCase()) {
    out += ch in TRANSLIT ? TRANSLIT[ch] : ch;
  }
  return out;
}

// Базовый @ник из имени: транслит, только [a-z0-9_], длина 3–20.
function baseUsername(name) {
  let slug = transliterate(name)
    .replace(/[^a-z0-9_]+/g, "_")
    .replace(/^_+|_+$/g, "");
  if (slug.length < 3) slug = (slug + "_poet").replace(/^_+/, "");
  slug = slug.slice(0, 20).replace(/_+$/, "");
  if (slug.length < 3) slug = "poet";
  return slug;
}

// Гарантирует уникальность @ника в рамках прогона и базы.
async function uniqueUsername(client, name, used) {
  const base = baseUsername(name);
  let candidate = base;
  let n = 1;
  for (;;) {
    const taken =
      used.has(candidate) ||
      (await client.query("SELECT 1 FROM users WHERE username = $1", [
        candidate,
      ])).rowCount > 0;
    if (!taken) {
      used.add(candidate);
      return candidate;
    }
    const suffix = String(++n);
    candidate = base.slice(0, 20 - suffix.length) + suffix;
  }
}

function rewriteImage(src) {
  if (!src) return null;
  // "../pix/foo.png" -> "/pix/foo.png"
  const m = src.match(/pix\/[^"'\s]+/);
  return m ? "/" + m[0] : null;
}

// Импортированные стихи/проза не должны интерпретироваться как Markdown-списки.
// Экранируем нумерацию в начале строки: "1. Синее" -> "1\. Синее"
// (иначе четверостишие получает отступ как пункт списка).
function escapeListMarkers(line) {
  return line.replace(/^(\s*)(\d+)\.(\s)/, "$1$2\\.$3");
}

// Встраивает inlineImages в текст как Markdown-картинки сразу после строки,
// указанной в поле "after". Если совпадение не найдено — добавляет в конец.
function buildBody(piece) {
  const text = piece.text || "";
  const rawLines = text.split("\n");
  const lines = rawLines.map(escapeListMarkers);
  const inline = Array.isArray(piece.inlineImages) ? piece.inlineImages : [];
  if (inline.length === 0) return lines.join("\n");

  const trailing = [];
  for (const img of inline) {
    const src = rewriteImage(img.src);
    if (!src) continue;
    const md = `\n![${img.alt || ""}](${src})\n`;
    const anchor = (img.after || "").trim();
    const idx = anchor ? rawLines.findIndex((l) => l.trim() === anchor) : -1;
    if (idx >= 0) {
      lines[idx] = lines[idx] + "\n" + md;
    } else {
      trailing.push(md);
    }
  }

  return [lines.join("\n"), ...trailing].join("\n");
}

function guessGenre(text) {
  const lines = text.split("\n").filter((l) => l.trim().length > 0);
  if (lines.length >= 4) {
    const avg = lines.reduce((s, l) => s + l.length, 0) / lines.length;
    if (avg < 70) return "poem";
  }
  return "prose";
}

async function ensureAuthor(client, name, cache, usedUsernames) {
  const display = name && name.trim() ? name.trim() : "Tochka.Text";
  if (cache.has(display)) return cache.get(display);

  const email = emailForAuthor(display);
  const username = await uniqueUsername(client, display, usedUsernames);
  const res = await client.query(
    `INSERT INTO users (email, username, password_hash, display_name, bio)
     VALUES ($1, $2, $3, $4, $5)
     ON CONFLICT (email) DO UPDATE SET display_name = EXCLUDED.display_name
     RETURNING id`,
    [email, username, PLACEHOLDER_HASH, display, ""],
  );
  const id = res.rows[0].id;
  cache.set(display, id);
  return id;
}

async function main() {
  const raw = JSON.parse(readFileSync(CONTENT_PATH, "utf8"));
  const sections = raw.sections || {};

  const client = await pool.connect();
  const authorCache = new Map();
  const usedUsernames = new Set();
  let createdWorks = 0;
  let createdCollections = 0;

  try {
    for (const [slug, section] of Object.entries(sections)) {
      if (section.layout === "credits") continue;

      const existing = await client.query(
        "SELECT id FROM collections WHERE slug = $1",
        [slug],
      );
      if (existing.rowCount > 0) {
        console.log(`Подборка "${slug}" уже существует, пропускаю.`);
        continue;
      }

      const colRes = await client.query(
        `INSERT INTO collections (slug, title, description, cover_image)
         VALUES ($1, $2, $3, $4) RETURNING id`,
        [
          slug,
          section.sectionName || slug,
          "",
          rewriteImage(section.heroImage),
        ],
      );
      const collectionId = colRes.rows[0].id;
      createdCollections += 1;

      let position = 0;
      for (const piece of section.pieces || []) {
        const text = buildBody(piece);
        const authorId = await ensureAuthor(
          client,
          piece.author,
          authorCache,
          usedUsernames,
        );
        const cover = rewriteImage(piece.image);
        const genre = guessGenre(piece.text || "");

        const workRes = await client.query(
          `INSERT INTO works (author_id, title, body, genre, cover_image)
           VALUES ($1, $2, $3, $4, $5) RETURNING id`,
          [authorId, piece.title || "", text, genre, cover],
        );
        const workId = workRes.rows[0].id;
        createdWorks += 1;

        await client.query(
          `INSERT INTO collection_works (collection_id, work_id, position)
           VALUES ($1, $2, $3) ON CONFLICT DO NOTHING`,
          [collectionId, workId, position++],
        );
      }
    }

    if (ADMIN_EMAIL) {
      const res = await client.query(
        "UPDATE users SET role = 'admin' WHERE email = $1 RETURNING id",
        [ADMIN_EMAIL.toLowerCase()],
      );
      if (res.rowCount > 0) {
        console.log(`Пользователь ${ADMIN_EMAIL} назначен администратором.`);
      } else {
        console.log(`Не найден пользователь ${ADMIN_EMAIL} для назначения админом.`);
      }
    }

    console.log(
      `Готово: создано подборок ${createdCollections}, произведений ${createdWorks}, авторов ${authorCache.size}.`,
    );
  } finally {
    client.release();
    await pool.end();
  }
}

main().catch((err) => {
  console.error("Ошибка seed:", err);
  process.exit(1);
});
