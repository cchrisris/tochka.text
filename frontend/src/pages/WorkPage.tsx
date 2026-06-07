import { useEffect, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import { commentsApi, worksApi, apiErrorMessage } from "../api/client";
import type { Comment, Work } from "../api/types";
import { useAuth } from "../auth/AuthContext";
import { Markdown } from "../components/Markdown";

const GENRE_LABEL: Record<string, string> = { poem: "Стихи", prose: "Проза" };

export function WorkPage() {
  const { id } = useParams();
  const workId = Number(id);
  const navigate = useNavigate();
  const { user } = useAuth();

  const [work, setWork] = useState<Work | null>(null);
  const [comments, setComments] = useState<Comment[]>([]);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);
  const [liked, setLiked] = useState(false);
  const [likes, setLikes] = useState(0);
  const [commentText, setCommentText] = useState("");
  const [posting, setPosting] = useState(false);

  useEffect(() => {
    setLoading(true);
    Promise.all([worksApi.get(workId), commentsApi.list(workId)])
      .then(([w, c]) => {
        setWork(w);
        setLiked(w.liked_by_me);
        setLikes(w.likes_count);
        setComments(c);
      })
      .catch((err) => setError(apiErrorMessage(err)))
      .finally(() => setLoading(false));
  }, [workId]);

  const toggleLike = async () => {
    if (!user) return;
    const res = liked ? await worksApi.unlike(workId) : await worksApi.like(workId);
    setLiked(res.liked);
    setLikes(res.likes_count);
  };

  const submitComment = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!commentText.trim()) return;
    setPosting(true);
    try {
      const c = await commentsApi.create(workId, commentText.trim());
      setComments((prev) => [...prev, c]);
      setCommentText("");
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setPosting(false);
    }
  };

  const deleteComment = async (cid: number) => {
    await commentsApi.remove(cid);
    setComments((prev) => prev.filter((c) => c.id !== cid));
  };

  const deleteWork = async () => {
    if (!confirm("Удалить произведение?")) return;
    await worksApi.remove(workId);
    navigate("/feed");
  };

  if (loading) return <div className="container container-narrow">Загрузка…</div>;
  if (!work) return <div className="container container-narrow error">{error || "Не найдено"}</div>;

  const canEdit = user && (user.id === work.author.id || user.role === "admin");

  return (
    <div className="container container-narrow">
      <div className="genre-badge">{GENRE_LABEL[work.genre] ?? work.genre}</div>
      <h1 className="page-title">{work.title || "Без названия"}</h1>
      <div className="work-meta">
        <Link to={`/users/${work.author.id}`}>{work.author.display_name}</Link>
        <span className="handle">@{work.author.username}</span>
        <span>·</span>
        <span>{new Date(work.created_at).getFullYear()}</span>
        {work.status === "hidden" && <span className="status-pill hidden">скрыто</span>}
      </div>

      {work.cover_image && <img className="work-cover" src={work.cover_image} alt={work.title} />}
      {work.body && <Markdown className="work-body">{work.body}</Markdown>}

      <div style={{ marginTop: 18 }}>
        {work.tags.map((t) => (
          <Link className="tag" key={t} to={`/search?tag=${encodeURIComponent(t)}`}>
            #{t}
          </Link>
        ))}
      </div>

      <div style={{ marginTop: 20, display: "flex", gap: 12, alignItems: "center" }}>
        <button
          className={`like-btn ${liked ? "liked" : ""}`}
          onClick={toggleLike}
          disabled={!user}
        >
          ♥ {likes}
        </button>
        {canEdit && (
          <>
            <Link className="btn btn-soft btn-sm" to={`/works/${work.id}/edit`}>
              Редактировать
            </Link>
            <button className="btn btn-danger btn-sm" onClick={deleteWork}>
              Удалить
            </button>
          </>
        )}
      </div>

      <section style={{ marginTop: 40 }}>
        <h2>Комментарии ({comments.length})</h2>
        {user ? (
          <form className="form" onSubmit={submitComment} style={{ marginBottom: 10 }}>
            <textarea
              style={{ minHeight: 90 }}
              placeholder="Оставьте комментарий…"
              value={commentText}
              onChange={(e) => setCommentText(e.target.value)}
            />
            <div>
              <button className="btn" type="submit" disabled={posting}>
                Отправить
              </button>
            </div>
          </form>
        ) : (
          <p className="muted">
            <Link to="/login">Войдите</Link>, чтобы комментировать.
          </p>
        )}

        {comments.map((c) => (
          <div className="comment" key={c.id}>
            <div className="comment-meta">
              <Link to={`/users/${c.author.id}`}>{c.author.display_name}</Link>
              <span>·</span>
              <span>{new Date(c.created_at).toLocaleString("ru-RU")}</span>
              {user && (user.id === c.author.id || user.role === "admin") && (
                <button
                  className="btn btn-danger btn-sm"
                  style={{ marginLeft: "auto" }}
                  onClick={() => deleteComment(c.id)}
                >
                  Удалить
                </button>
              )}
            </div>
            <p style={{ whiteSpace: "pre-wrap", margin: "6px 0 0" }}>{c.body}</p>
          </div>
        ))}
      </section>
    </div>
  );
}
