import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";
import { apiErrorMessage } from "../api/client";

const USERNAME_RE = /^[a-z0-9_]{3,20}$/;

export function RegisterPage() {
  const { register } = useAuth();
  const navigate = useNavigate();
  const [email, setEmail] = useState("");
  const [username, setUsername] = useState("");
  const [displayName, setDisplayName] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  // Клиентская проверка — чтобы пояснение появлялось сразу, до запроса.
  const validate = (): string | null => {
    if (!displayName.trim()) return "Укажите отображаемое имя";
    if (!USERNAME_RE.test(username)) {
      return "Юзернейм: 3–20 символов, только латиница, цифры и _";
    }
    if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) {
      return "Введите корректный email, например name@example.com";
    }
    if (password.length < 8) return "Пароль должен содержать минимум 8 символов";
    return null;
  };

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    const localError = validate();
    if (localError) {
      setError(localError);
      return;
    }
    setBusy(true);
    setError("");
    try {
      await register(email.trim(), username.trim().toLowerCase(), password, displayName.trim());
      navigate("/feed");
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="container container-narrow" style={{ maxWidth: 420 }}>
      <h1 className="page-title">Регистрация</h1>
      <form className="form" onSubmit={submit} noValidate>
        {error && <div className="error">{error}</div>}
        <div>
          <label>Отображаемое имя</label>
          <input
            value={displayName}
            onChange={(e) => setDisplayName(e.target.value)}
            placeholder="Например: Анна Ахматова"
          />
        </div>
        <div>
          <label>Юзернейм (@)</label>
          <div className="input-prefix">
            <span>@</span>
            <input
              value={username}
              onChange={(e) => setUsername(e.target.value.toLowerCase())}
              placeholder="anna_a"
              autoCapitalize="none"
              autoCorrect="off"
            />
          </div>
          <p className="editor-hint">3–20 символов: латиница, цифры и _</p>
        </div>
        <div>
          <label>Email</label>
          <input
            type="email"
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            placeholder="name@example.com"
          />
        </div>
        <div>
          <label>Пароль</label>
          <input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            placeholder="минимум 8 символов"
          />
        </div>
        <button className="btn" type="submit" disabled={busy}>
          Создать аккаунт
        </button>
      </form>
      <p className="muted" style={{ marginTop: 16 }}>
        Уже есть аккаунт? <Link to="/login">Войти</Link>
      </p>
    </div>
  );
}
