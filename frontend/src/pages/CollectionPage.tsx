import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { collectionsApi, worksApi, apiErrorMessage } from "../api/client";
import type { Collection, Work } from "../api/types";
import { Markdown } from "../components/Markdown";
import { ImageUpload } from "../components/ImageUpload";
import { useAuth } from "../auth/AuthContext";

function Piece({
  work,
  canManage,
  onRemove,
}: {
  work: Work;
  canManage: boolean;
  onRemove: (id: number) => void;
}) {
  return (
    <article className="piece">
      <div className="piece-main">
        <h2 className="piece-title">
          <Link to={`/works/${work.id}`}>{work.title || "Без названия"}</Link>
        </h2>
        {work.cover_image && (
          <img className="piece-cover" src={work.cover_image} alt={work.title} loading="lazy" />
        )}
        {work.body && <Markdown className="piece-body">{work.body}</Markdown>}
      </div>
      <div className="piece-side">
        <Link className="piece-side-author" to={`/users/${work.author.id}`}>
          {work.author.display_name}
        </Link>
        <span className="handle">@{work.author.username}</span>
        <div className="piece-side-meta">
          <span>{new Date(work.created_at).getFullYear()}</span>
          <span>·</span>
          <span>♥ {work.likes_count}</span>
        </div>
        {canManage && (
          <button className="btn btn-danger btn-sm" onClick={() => onRemove(work.id)}>
            Убрать из подборки
          </button>
        )}
      </div>
    </article>
  );
}

function EditCollectionPanel({
  collection,
  onSaved,
  onCancel,
}: {
  collection: Collection;
  onSaved: () => void;
  onCancel: () => void;
}) {
  const [title, setTitle] = useState(collection.title);
  const [description, setDescription] = useState(collection.description);
  const [cover, setCover] = useState(collection.cover_image ?? "");
  const [heroBg, setHeroBg] = useState(collection.hero_bg ?? "");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  const save = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!title.trim()) {
      setError("Укажите название");
      return;
    }
    setBusy(true);
    setError("");
    try {
      await collectionsApi.update(collection.id, {
        title: title.trim(),
        description: description || undefined,
        slug: collection.slug ?? undefined,
        cover_image: cover.trim() || undefined,
        hero_bg: heroBg.trim() || undefined,
      });
      onSaved();
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setBusy(false);
    }
  };

  return (
    <form className="card form" onSubmit={save} style={{ marginBottom: 24 }}>
      <h3 style={{ marginTop: 0 }}>Настройки подборки</h3>
      {error && <div className="error">{error}</div>}
      <div>
        <label>Название</label>
        <input value={title} onChange={(e) => setTitle(e.target.value)} />
      </div>
      <div>
        <label>Описание</label>
        <textarea
          style={{ minHeight: 80 }}
          value={description}
          onChange={(e) => setDescription(e.target.value)}
        />
      </div>
      <div>
        <label>Обложка</label>
        <ImageUpload value={cover} onChange={setCover} maxSize={1280} />
      </div>
      <div>
        <label>Фон шапки (CSS-цвет или градиент)</label>
        <div style={{ display: "flex", gap: 10, alignItems: "center" }}>
          <span
            title="Превью фона"
            style={{
              width: 34,
              height: 34,
              flex: "0 0 auto",
              borderRadius: 8,
              border: "1px solid var(--border)",
              background: heroBg.trim() || "var(--hero-bg)",
            }}
          />
          <input
            value={heroBg}
            onChange={(e) => setHeroBg(e.target.value)}
            placeholder="#ffe9d6 · rgb(...) · linear-gradient(...)"
          />
        </div>
      </div>
      <div style={{ display: "flex", gap: 10 }}>
        <button className="btn" type="submit" disabled={busy}>
          Сохранить
        </button>
        <button className="btn btn-soft" type="button" onClick={onCancel}>
          Отмена
        </button>
      </div>
    </form>
  );
}

function ManagePanel({
  collectionId,
  onChanged,
}: {
  collectionId: number;
  onChanged: () => void;
}) {
  const [q, setQ] = useState("");
  const [results, setResults] = useState<Work[]>([]);
  const [searching, setSearching] = useState(false);
  const [manualId, setManualId] = useState("");
  const [error, setError] = useState("");

  const search = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!q.trim()) return;
    setSearching(true);
    setError("");
    try {
      const res = await worksApi.feed({ q: q.trim(), limit: 20 });
      setResults(res.items);
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setSearching(false);
    }
  };

  const add = async (workId: number) => {
    setError("");
    try {
      await collectionsApi.addWork(collectionId, workId);
      onChanged();
    } catch (err) {
      setError(apiErrorMessage(err));
    }
  };

  const addManual = async () => {
    const id = Number(manualId);
    if (!id) return;
    await add(id);
    setManualId("");
  };

  return (
    <div className="card" style={{ marginBottom: 24 }}>
      <h3 style={{ marginTop: 0 }}>Добавить произведение</h3>

      <form className="toolbar" onSubmit={search} style={{ marginBottom: 12 }}>
        <input
          placeholder="Поиск по названию, тексту или автору…"
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
        <button className="btn" type="submit" disabled={searching}>
          Искать
        </button>
      </form>

      {error && <div className="error" style={{ marginBottom: 12 }}>{error}</div>}

      {results.map((w) => (
        <div className="admin-row" key={w.id}>
          <Link to={`/works/${w.id}`} style={{ fontWeight: 600 }}>
            {w.title || "Без названия"}
          </Link>
          <span className="muted">{w.author.display_name}</span>
          <span className="handle">#{w.id}</span>
          <button
            className="btn btn-soft btn-sm"
            style={{ marginLeft: "auto" }}
            onClick={() => add(w.id)}
          >
            Добавить
          </button>
        </div>
      ))}

      <div className="toolbar" style={{ marginTop: 14, marginBottom: 0 }}>
        <input
          style={{ flex: "0 0 160px" }}
          placeholder="…или по ID работы"
          value={manualId}
          onChange={(e) => setManualId(e.target.value)}
        />
        <button className="btn btn-soft" type="button" onClick={addManual}>
          Добавить по ID
        </button>
      </div>
    </div>
  );
}

export function CollectionPage() {
  const { id } = useParams();
  const { user } = useAuth();
  const [collection, setCollection] = useState<Collection | null>(null);
  const [works, setWorks] = useState<Work[]>([]);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);
  const [editing, setEditing] = useState(false);

  const canManage = user?.role === "admin";

  const load = () =>
    collectionsApi
      .get(Number(id))
      .then((data) => {
        setCollection(data.collection);
        setWorks(data.works);
      })
      .catch((err) => setError(apiErrorMessage(err)))
      .finally(() => setLoading(false));

  useEffect(() => {
    load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [id]);

  const removeWork = async (workId: number) => {
    if (!confirm("Убрать произведение из подборки?")) return;
    try {
      await collectionsApi.removeWork(Number(id), workId);
      load();
    } catch (err) {
      setError(apiErrorMessage(err));
    }
  };

  if (loading) return <div className="container">Загрузка…</div>;
  if (!collection)
    return <div className="container error">{error || "Не найдено"}</div>;

  return (
    <div className="container">
      <section
        className="section-hero"
        style={collection.hero_bg ? { background: collection.hero_bg } : undefined}
      >
        <h1 className="hero-title">{collection.title}</h1>
        {collection.description && <p className="hero-desc">{collection.description}</p>}
        {collection.curator && (
          <p className="hero-curator">
            Куратор:{" "}
            <Link to={`/users/${collection.curator.id}`}>
              {collection.curator.display_name}
            </Link>
          </p>
        )}
        {collection.cover_image && (
          <img className="hero-image" src={collection.cover_image} alt={collection.title} />
        )}
      </section>

      {canManage && !editing && (
        <div style={{ display: "flex", justifyContent: "flex-end", marginBottom: 16 }}>
          <button className="btn btn-soft btn-sm" onClick={() => setEditing(true)}>
            Редактировать подборку
          </button>
        </div>
      )}

      {canManage && editing && (
        <EditCollectionPanel
          collection={collection}
          onSaved={() => {
            setEditing(false);
            load();
          }}
          onCancel={() => setEditing(false)}
        />
      )}

      {canManage && <ManagePanel collectionId={collection.id} onChanged={load} />}

      {works.length === 0 ? (
        <p className="muted center">В подборке пока нет произведений.</p>
      ) : (
        <div className="feed">
          {works.map((w) => (
            <Piece key={w.id} work={w} canManage={canManage} onRemove={removeWork} />
          ))}
        </div>
      )}
    </div>
  );
}
