import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { usersApi, apiErrorMessage } from "../api/client";
import type { User, Work } from "../api/types";
import { WorkCard } from "../components/WorkCard";
import { FloatingWriteButton } from "../components/FloatingWriteButton";
import { ImageUpload } from "../components/ImageUpload";
import { useAuth } from "../auth/AuthContext";

const BIO_MAX = 280;

function EditProfile({
  user,
  onSaved,
  onCancel,
}: {
  user: User;
  onSaved: (u: User) => void;
  onCancel: () => void;
}) {
  const [displayName, setDisplayName] = useState(user.display_name);
  const [avatar, setAvatar] = useState(user.avatar_url ?? "");
  const [bio, setBio] = useState(user.bio);
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  const save = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!displayName.trim()) {
      setError("Укажите отображаемое имя");
      return;
    }
    setBusy(true);
    setError("");
    try {
      const updated = await usersApi.updateMe({
        display_name: displayName.trim(),
        bio,
        avatar_url: avatar.trim() || undefined,
      });
      onSaved(updated);
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setBusy(false);
    }
  };

  return (
    <form className="card form" onSubmit={save} style={{ marginBottom: 24 }}>
      <h3 style={{ margin: 0 }}>Редактировать профиль</h3>
      {error && <div className="error">{error}</div>}
      <div>
        <label>Аватарка</label>
        <ImageUpload value={avatar} onChange={setAvatar} maxSize={256} shape="circle" />
      </div>
      <div>
        <label>Отображаемое имя</label>
        <input value={displayName} onChange={(e) => setDisplayName(e.target.value)} />
      </div>
      <div>
        <label>О себе</label>
        <textarea
          style={{ minHeight: 80 }}
          value={bio}
          maxLength={BIO_MAX}
          onChange={(e) => setBio(e.target.value)}
          placeholder="Короткая цитата о себе…"
        />
        <p className="editor-hint" style={{ textAlign: "right" }}>
          {bio.length}/{BIO_MAX}
        </p>
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

export function ProfilePage() {
  const { id } = useParams();
  const userId = Number(id);
  const { user: me, setCurrentUser } = useAuth();
  const [user, setUser] = useState<User | null>(null);
  const [works, setWorks] = useState<Work[]>([]);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);
  const [editing, setEditing] = useState(false);

  useEffect(() => {
    setLoading(true);
    setEditing(false);
    usersApi
      .profile(userId)
      .then((data) => {
        setUser(data.user);
        setWorks(data.works);
      })
      .catch((err) => setError(apiErrorMessage(err)))
      .finally(() => setLoading(false));
  }, [userId]);

  if (loading) return <div className="container container-narrow">Загрузка…</div>;
  if (!user) return <div className="container container-narrow error">{error || "Не найдено"}</div>;

  const isMe = me?.id === user.id;

  const handleSaved = (updated: User) => {
    setUser(updated);
    setCurrentUser(updated);
    setEditing(false);
  };

  return (
    <div className="container container-narrow">
      <div className="profile-head">
        <span className="avatar">
          {user.avatar_url && <img className="avatar" src={user.avatar_url} alt="" />}
        </span>
        <div style={{ flex: 1 }}>
          <h1 className="page-title" style={{ marginBottom: 2 }}>
            {user.display_name}
          </h1>
          <p className="handle" style={{ margin: "0 0 2px" }}>
            @{user.username}
          </p>
          {user.role === "admin" && (
            <p className="muted" style={{ margin: 0 }}>
              Администратор
            </p>
          )}
        </div>
        {isMe && !editing && (
          <button className="btn btn-soft btn-sm" onClick={() => setEditing(true)}>
            Редактировать
          </button>
        )}
      </div>

      {isMe && editing && (
        <EditProfile user={user} onSaved={handleSaved} onCancel={() => setEditing(false)} />
      )}

      {!editing && user.bio && <blockquote className="profile-bio">{user.bio}</blockquote>}

      <div
        style={{
          display: "flex",
          alignItems: "center",
          justifyContent: "space-between",
          gap: 12,
        }}
      >
        <h2 style={{ margin: 0 }}>Произведения ({works.length})</h2>
        {isMe && (
          <Link className="btn btn-sm" to="/new">
            Написать
          </Link>
        )}
      </div>
      {works.length === 0 ? (
        <p className="muted">Пока нет публикаций.</p>
      ) : (
        works.map((w) => <WorkCard key={w.id} work={w} />)
      )}
      {isMe && <FloatingWriteButton />}
    </div>
  );
}
