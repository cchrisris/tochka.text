import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { collectionsApi, apiErrorMessage } from "../api/client";
import type { Collection } from "../api/types";

export function CollectionsPage() {
  const [items, setItems] = useState<Collection[]>([]);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    collectionsApi
      .list()
      .then(setItems)
      .catch((err) => setError(apiErrorMessage(err)))
      .finally(() => setLoading(false));
  }, []);

  return (
    <div className="container">
      <h1 className="page-title">Подборки</h1>
      <p className="page-subtitle">Кураторские собрания произведений</p>
      {error && <div className="error">{error}</div>}
      {loading ? (
        <p className="muted">Загрузка…</p>
      ) : items.length === 0 ? (
        <p className="muted">Подборок пока нет.</p>
      ) : (
        <div className="grid">
          {items.map((c) => (
            <Link key={c.id} to={`/collections/${c.id}`} className="card collection-card">
              {c.cover_image ? (
                <img
                  className="collection-cover"
                  src={c.cover_image}
                  alt={c.title}
                  style={c.hero_bg ? { background: c.hero_bg } : undefined}
                />
              ) : (
                c.hero_bg && (
                  <div className="collection-cover" style={{ background: c.hero_bg }} />
                )
              )}
              <div className="collection-body">
                <h3>{c.title}</h3>
                <p className="muted" style={{ margin: 0 }}>
                  {c.works_count} произведений
                  {c.curator ? ` · куратор ${c.curator.display_name}` : ""}
                </p>
                {c.description && <p style={{ marginBottom: 0 }}>{c.description}</p>}
              </div>
            </Link>
          ))}
        </div>
      )}
    </div>
  );
}
