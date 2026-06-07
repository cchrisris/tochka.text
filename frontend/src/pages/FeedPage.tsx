import { useEffect, useState } from "react";
import { useSearchParams } from "react-router-dom";
import { worksApi, apiErrorMessage } from "../api/client";
import type { Genre, Work } from "../api/types";
import { WorkCard } from "../components/WorkCard";
import { FloatingWriteButton } from "../components/FloatingWriteButton";

export function FeedPage() {
  const [searchParams, setSearchParams] = useSearchParams();
  const [works, setWorks] = useState<Work[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [q, setQ] = useState(searchParams.get("q") ?? "");
  const [genre, setGenre] = useState<"" | Genre>("");

  const load = async (query: string, currentGenre: "" | Genre) => {
    setLoading(true);
    setError("");
    try {
      const res = await worksApi.feed({
        q: query || undefined,
        genre: currentGenre || undefined,
      });
      setWorks(res.items);
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    const urlQuery = searchParams.get("q") ?? "";
    setQ(urlQuery);
    load(urlQuery, genre);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [searchParams]);

  const onSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    setSearchParams(q.trim() ? { q: q.trim() } : {});
    load(q.trim(), genre);
  };

  return (
    <div className="container container-narrow">
      <h1 className="page-title">Лента произведений</h1>
      <p className="page-subtitle">Стихи и проза сообщества Tochka.Text</p>

      <form className="toolbar" onSubmit={onSubmit}>
        <input
          placeholder="Поиск по тексту, названию, автору…"
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
        <select value={genre} onChange={(e) => setGenre(e.target.value as "" | Genre)}>
          <option value="">Все жанры</option>
          <option value="poem">Стихи</option>
          <option value="prose">Проза</option>
        </select>
        <button className="btn" type="submit">
          Искать
        </button>
      </form>

      {error && <div className="error">{error}</div>}
      {loading ? (
        <p className="muted">Загрузка…</p>
      ) : works.length === 0 ? (
        <p className="muted">Пока ничего нет. Будьте первым — опубликуйте произведение!</p>
      ) : (
        works.map((w) => <WorkCard key={w.id} work={w} />)
      )}
      <FloatingWriteButton />
    </div>
  );
}
