import { useState } from "react";
import { Link } from "react-router-dom";
import type { Work } from "../api/types";
import { worksApi } from "../api/client";
import { useAuth } from "../auth/AuthContext";
import { markdownToPlain } from "../lib/markdown";

const GENRE_LABEL: Record<string, string> = { poem: "Стихи", prose: "Проза" };

export function WorkCard({ work }: { work: Work }) {
  const { user } = useAuth();
  const [liked, setLiked] = useState(work.liked_by_me);
  const [likes, setLikes] = useState(work.likes_count);
  const [busy, setBusy] = useState(false);

  const toggleLike = async () => {
    if (!user || busy) return;
    setBusy(true);
    try {
      const res = liked ? await worksApi.unlike(work.id) : await worksApi.like(work.id);
      setLiked(res.liked);
      setLikes(res.likes_count);
    } finally {
      setBusy(false);
    }
  };

  return (
    <article className="card work-card">
      <div className="genre-badge">{GENRE_LABEL[work.genre] ?? work.genre}</div>
      <h2 className="work-title">
        <Link to={`/works/${work.id}`}>{work.title || "Без названия"}</Link>
      </h2>
      <div className="work-meta">
        <Link to={`/users/${work.author.id}`}>{work.author.display_name}</Link>
        <span>·</span>
        <span>{new Date(work.created_at).getFullYear()}</span>
      </div>
      {work.cover_image && (
        <img className="work-cover" src={work.cover_image} alt={work.title} loading="lazy" />
      )}
      {work.body && <p className="work-excerpt">{markdownToPlain(work.body)}</p>}
      <div style={{ marginTop: 14 }}>
        {work.tags.map((t) => (
          <span className="tag" key={t}>
            #{t}
          </span>
        ))}
      </div>
      <div style={{ marginTop: 14, display: "flex", gap: 12, alignItems: "center" }}>
        <button
          className={`like-btn ${liked ? "liked" : ""}`}
          onClick={toggleLike}
          disabled={!user || busy}
          title={user ? "" : "Войдите, чтобы лайкать"}
        >
          ♥ {likes}
        </button>
        <Link to={`/works/${work.id}`} className="muted">
          💬 {work.comments_count}
        </Link>
      </div>
    </article>
  );
}
