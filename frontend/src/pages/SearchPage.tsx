import { useEffect, useState } from "react";
import { Link, useSearchParams } from "react-router-dom";
import { usersApi, worksApi, apiErrorMessage } from "../api/client";
import type { User, Work } from "../api/types";
import { WorkCard } from "../components/WorkCard";

type Mode = "works" | "users";

export function SearchPage() {
  const [params, setParams] = useSearchParams();
  const tag = params.get("tag") ?? "";
  const [mode, setMode] = useState<Mode>("works");
  const [q, setQ] = useState(params.get("q") ?? "");
  const [works, setWorks] = useState<Work[]>([]);
  const [users, setUsers] = useState<User[]>([]);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const runSearch = async (mode: Mode, q: string, tag: string) => {
    setLoading(true);
    setError("");
    try {
      if (mode === "works") {
        const res = await worksApi.feed({ q: q || undefined, tag: tag || undefined });
        setWorks(res.items);
      } else {
        setUsers(await usersApi.search(q));
      }
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    runSearch(mode, q, tag);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mode, tag]);

  const onSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const next = new URLSearchParams(params);
    if (q) next.set("q", q);
    else next.delete("q");
    next.delete("tag");
    setParams(next);
    runSearch(mode, q, "");
  };

  return (
    <div className="container container-narrow">
      <h1 className="page-title">Поиск</h1>

      <div className="tabs">
        <button className={mode === "works" ? "active" : ""} onClick={() => setMode("works")}>
          Произведения
        </button>
        <button className={mode === "users" ? "active" : ""} onClick={() => setMode("users")}>
          Авторы
        </button>
      </div>

      <form className="toolbar" onSubmit={onSubmit}>
        <input
          placeholder={mode === "works" ? "Название, текст, автор…" : "Имя или @ник автора…"}
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
        <button className="btn" type="submit">
          Искать
        </button>
      </form>

      {tag && <p className="muted">Тег: #{tag}</p>}
      {error && <div className="error">{error}</div>}
      {loading && <p className="muted">Загрузка…</p>}

      {!loading && mode === "works" &&
        (works.length === 0 ? (
          <p className="muted">Ничего не найдено.</p>
        ) : (
          works.map((w) => <WorkCard key={w.id} work={w} />)
        ))}

      {!loading && mode === "users" &&
        (users.length === 0 ? (
          <p className="muted">Ничего не найдено.</p>
        ) : (
          users.map((u) => (
            <div className="card" key={u.id} style={{ marginBottom: 12 }}>
              <Link to={`/users/${u.id}`} style={{ fontWeight: 600 }}>
                {u.display_name}
              </Link>{" "}
              <span className="handle">@{u.username}</span>
              {u.bio && <p className="muted" style={{ margin: "6px 0 0" }}>{u.bio}</p>}
            </div>
          ))
        ))}
    </div>
  );
}
