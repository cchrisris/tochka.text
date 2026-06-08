import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";
import { FloatingWriteButton } from "../components/FloatingWriteButton";
import { UserMenu } from "../components/UserMenu";

export function LandingPage() {
  const navigate = useNavigate();
  const { user } = useAuth();
  const [query, setQuery] = useState("");

  const onSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const trimmed = query.trim();
    navigate(trimmed ? `/feed?q=${encodeURIComponent(trimmed)}` : "/feed");
  };

  return (
    <div className="landing">
      <header className="landing-top">
        <Link to="/" aria-label="На главную">
          <img className="landing-logo" src="/pix/rht_logo.png" alt="Логотип РШТ" />
        </Link>
        <nav className="nav">
          {user ? <UserMenu /> : <Link to="/login">Войти</Link>}
        </nav>
      </header>

      <section className="search-shell" aria-label="Поиск произведений">
        <h1 className="landing-brand">Tochka.Text</h1>
        <form className="search-form" onSubmit={onSubmit} autoComplete="off">
          <input
            className="search-input"
            type="text"
            placeholder="Найти стихи, прозу или автора…"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            aria-label="Поисковая строка"
          />
          <button className="search-submit" type="submit">
            читать
          </button>
        </form>
        <div className="landing-links">
          <Link to="/collections">Подборки</Link>
          <Link to="/feed" className="landing-link-feed">
            Лента
          </Link>
          <Link to="/docs">О проекте</Link>
        </div>
      </section>
      <FloatingWriteButton />
    </div>
  );
}
