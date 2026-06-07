import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import {
  adminApi,
  collectionsApi,
  apiErrorMessage,
} from "../api/client";
import type { AdminComment, AdminUser, AdminWork, Collection } from "../api/types";
import { useAuth } from "../auth/AuthContext";

type Tab = "works" | "comments" | "collections" | "users";

export function AdminPage() {
  const [tab, setTab] = useState<Tab>("works");
  const [error, setError] = useState("");

  const switchTab = (next: Tab) => {
    setError("");
    setTab(next);
  };

  return (
    <div className="container">
      <h1 className="page-title">Модерация</h1>
      <div className="tabs">
        <button className={tab === "works" ? "active" : ""} onClick={() => switchTab("works")}>
          Произведения
        </button>
        <button className={tab === "comments" ? "active" : ""} onClick={() => switchTab("comments")}>
          Комментарии
        </button>
        <button
          className={tab === "collections" ? "active" : ""}
          onClick={() => switchTab("collections")}
        >
          Подборки
        </button>
        <button className={tab === "users" ? "active" : ""} onClick={() => switchTab("users")}>
          Пользователи
        </button>
      </div>
      {error && <div className="error">{error}</div>}
      {tab === "works" && <WorksTab onError={setError} />}
      {tab === "comments" && <CommentsTab onError={setError} />}
      {tab === "collections" && <CollectionsTab onError={setError} />}
      {tab === "users" && <UsersTab onError={setError} />}
    </div>
  );
}

function WorksTab({ onError }: { onError: (m: string) => void }) {
  const [items, setItems] = useState<AdminWork[]>([]);

  const load = () =>
    adminApi.works().then(setItems).catch((e) => onError(apiErrorMessage(e)));
  useEffect(() => {
    load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const toggle = async (w: AdminWork) => {
    const next = w.status === "published" ? "hidden" : "published";
    await adminApi.setWorkStatus(w.id, next);
    load();
  };

  return (
    <div className="card">
      {items.map((w) => (
        <div className="admin-row" key={w.id}>
          <span className={`status-pill ${w.status}`}>{w.status}</span>
          <Link to={`/works/${w.id}`} style={{ fontWeight: 600 }}>
            {w.title || "Без названия"}
          </Link>
          <span className="muted">{w.author.display_name}</span>
          <button className="btn btn-soft btn-sm" style={{ marginLeft: "auto" }} onClick={() => toggle(w)}>
            {w.status === "published" ? "Скрыть" : "Опубликовать"}
          </button>
        </div>
      ))}
    </div>
  );
}

function CommentsTab({ onError }: { onError: (m: string) => void }) {
  const [items, setItems] = useState<AdminComment[]>([]);

  const load = () =>
    adminApi.comments().then(setItems).catch((e) => onError(apiErrorMessage(e)));
  useEffect(() => {
    load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const toggle = async (c: AdminComment) => {
    const next = c.status === "visible" ? "hidden" : "visible";
    await adminApi.setCommentStatus(c.id, next);
    load();
  };

  return (
    <div className="card">
      {items.map((c) => (
        <div className="admin-row" key={c.id}>
          <span className={`status-pill ${c.status}`}>{c.status}</span>
          <span style={{ flex: 1 }}>{c.body}</span>
          <span className="muted">{c.author.display_name}</span>
          <Link to={`/works/${c.work_id}`} className="muted btn-sm">
            к произв.
          </Link>
          <button className="btn btn-soft btn-sm" onClick={() => toggle(c)}>
            {c.status === "visible" ? "Скрыть" : "Показать"}
          </button>
        </div>
      ))}
    </div>
  );
}

function UsersTab({ onError }: { onError: (m: string) => void }) {
  const { user } = useAuth();
  const [items, setItems] = useState<AdminUser[]>([]);
  const [q, setQ] = useState("");

  const load = (query: string) =>
    adminApi.users(query).then(setItems).catch((e) => onError(apiErrorMessage(e)));
  useEffect(() => {
    load("");
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const search = (e: React.FormEvent) => {
    e.preventDefault();
    load(q.trim());
  };

  const toggleRole = async (u: AdminUser) => {
    const next = u.role === "admin" ? "user" : "admin";
    try {
      await adminApi.setUserRole(u.id, next);
      load(q.trim());
    } catch (err) {
      onError(apiErrorMessage(err));
    }
  };

  return (
    <>
      <form className="toolbar" onSubmit={search}>
        <input
          placeholder="Поиск по имени, @нику или email…"
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
        <button className="btn" type="submit">
          Искать
        </button>
      </form>
      <div className="card">
        {items.map((u) => (
          <div className="admin-row" key={u.id}>
            <span className={`status-pill ${u.role === "admin" ? "published" : "hidden"}`}>
              {u.role === "admin" ? "admin" : "user"}
            </span>
            <Link to={`/users/${u.id}`} style={{ fontWeight: 600 }}>
              {u.display_name}
            </Link>
            <span className="handle">@{u.username}</span>
            <span className="muted">{u.email}</span>
            <button
              className="btn btn-soft btn-sm"
              style={{ marginLeft: "auto" }}
              disabled={user?.id === u.id}
              title={user?.id === u.id ? "Нельзя изменить свою роль" : ""}
              onClick={() => toggleRole(u)}
            >
              {u.role === "admin" ? "Снять админа" : "Сделать админом"}
            </button>
          </div>
        ))}
      </div>
    </>
  );
}

function CollectionsTab({ onError }: { onError: (m: string) => void }) {
  const [items, setItems] = useState<Collection[]>([]);
  const [title, setTitle] = useState("");

  const load = () =>
    collectionsApi.list().then(setItems).catch((e) => onError(apiErrorMessage(e)));
  useEffect(() => {
    load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const create = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!title.trim()) return;
    try {
      await collectionsApi.create({ title: title.trim() });
      setTitle("");
      load();
    } catch (err) {
      onError(apiErrorMessage(err));
    }
  };

  const remove = async (id: number) => {
    if (!confirm("Удалить подборку?")) return;
    await collectionsApi.remove(id);
    load();
  };

  return (
    <>
      <form className="toolbar" onSubmit={create} style={{ marginBottom: 16 }}>
        <input
          placeholder="Название новой подборки"
          value={title}
          onChange={(e) => setTitle(e.target.value)}
        />
        <button className="btn" type="submit">
          Создать
        </button>
      </form>

      <p className="muted">
        Оформление (название, обложка, фон, описание) и наполнение настраиваются на
        странице самой подборки.
      </p>
      <div className="card">
        {items.map((c) => (
          <div className="admin-row" key={c.id}>
            <Link to={`/collections/${c.id}`} style={{ fontWeight: 600 }}>
              {c.title}
            </Link>
            <span className="muted">{c.works_count} произв.</span>
            <button
              className="btn btn-danger btn-sm"
              style={{ marginLeft: "auto" }}
              onClick={() => remove(c.id)}
            >
              Удалить
            </button>
          </div>
        ))}
      </div>
    </>
  );
}
