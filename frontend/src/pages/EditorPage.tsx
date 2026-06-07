import { useEffect, useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import { worksApi, apiErrorMessage } from "../api/client";
import type { Genre } from "../api/types";
import { Markdown } from "../components/Markdown";
import { ImageUpload } from "../components/ImageUpload";

export function EditorPage() {
  const { id } = useParams();
  const editing = Boolean(id);
  const navigate = useNavigate();

  const [title, setTitle] = useState("");
  const [body, setBody] = useState("");
  const [genre, setGenre] = useState<Genre>("poem");
  const [cover, setCover] = useState("");
  const [tags, setTags] = useState("");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(editing);
  const [showPreview, setShowPreview] = useState(false);

  useEffect(() => {
    if (!editing) return;
    worksApi
      .get(Number(id))
      .then((w) => {
        setTitle(w.title);
        setBody(w.body);
        setGenre(w.genre);
        setCover(w.cover_image ?? "");
        setTags(w.tags.join(", "));
      })
      .catch((err) => setError(apiErrorMessage(err)))
      .finally(() => setLoading(false));
  }, [id, editing]);

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setBusy(true);
    setError("");
    const payload = {
      title,
      body,
      genre,
      cover_image: cover || undefined,
      tags: tags
        .split(",")
        .map((t) => t.trim())
        .filter(Boolean),
    };
    try {
      const work = editing
        ? await worksApi.update(Number(id), payload)
        : await worksApi.create(payload);
      navigate(`/works/${work.id}`);
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setBusy(false);
    }
  };

  if (loading) return <div className="container container-narrow">Загрузка…</div>;

  return (
    <div className="container container-narrow">
      <h1 className="page-title">{editing ? "Редактирование" : "Новое произведение"}</h1>
      <form className="form" onSubmit={submit}>
        {error && <div className="error">{error}</div>}
        <div>
          <label>Название</label>
          <input value={title} onChange={(e) => setTitle(e.target.value)} />
        </div>
        <div>
          <label>Жанр</label>
          <select value={genre} onChange={(e) => setGenre(e.target.value as Genre)}>
            <option value="poem">Стихи</option>
            <option value="prose">Проза</option>
          </select>
        </div>
        <div>
          <label>Текст (Markdown)</label>
          <div className="editor-md-toolbar">
            <button
              type="button"
              className="btn btn-soft btn-sm"
              onClick={() => setShowPreview((v) => !v)}
            >
              {showPreview ? "Скрыть предпросмотр" : "Предпросмотр"}
            </button>
          </div>
          <textarea value={body} onChange={(e) => setBody(e.target.value)} />
          <p className="editor-hint">
            Поддерживается Markdown: **жирный**, *курсив*, заголовки (#), списки. Картинку
            можно вставить в любое место текста синтаксисом
            <code> ![подпись](ссылка)</code>.
          </p>
          {showPreview && (
            <div className="card" style={{ marginTop: 10 }}>
              {body.trim() ? (
                <Markdown className="work-md">{body}</Markdown>
              ) : (
                <p className="muted" style={{ margin: 0 }}>
                  Здесь появится предпросмотр.
                </p>
              )}
            </div>
          )}
        </div>
        <div>
          <label>Обложка (необязательно)</label>
          <ImageUpload value={cover} onChange={setCover} maxSize={1280} />
        </div>
        <div>
          <label>Теги (через запятую)</label>
          <input value={tags} onChange={(e) => setTags(e.target.value)} />
        </div>
        <div>
          <button className="btn" type="submit" disabled={busy}>
            {editing ? "Сохранить" : "Опубликовать"}
          </button>
        </div>
      </form>
    </div>
  );
}
