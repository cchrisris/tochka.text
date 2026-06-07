import { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";

export function UserMenu() {
  const { user, logout } = useAuth();
  const navigate = useNavigate();
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open) return;
    const onClick = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) {
        setOpen(false);
      }
    };
    document.addEventListener("mousedown", onClick);
    return () => document.removeEventListener("mousedown", onClick);
  }, [open]);

  if (!user) return null;

  const go = (path: string) => {
    setOpen(false);
    navigate(path);
  };

  return (
    <div className="user-menu" ref={ref}>
      <button
        className="user-menu-trigger"
        onClick={() => setOpen((v) => !v)}
        aria-haspopup="menu"
        aria-expanded={open}
      >
        <span className="avatar avatar-sm">
          {user.avatar_url && <img className="avatar avatar-sm" src={user.avatar_url} alt="" />}
        </span>
        <span>{user.display_name}</span>
        <span className="caret">▾</span>
      </button>
      {open && (
        <div className="user-menu-dropdown" role="menu">
          <div className="user-menu-head">
            <strong>{user.display_name}</strong>
            <span className="handle">@{user.username}</span>
          </div>
          <button role="menuitem" onClick={() => go(`/users/${user.id}`)}>
            Профиль
          </button>
          <button role="menuitem" onClick={() => go("/new")}>
            Написать
          </button>
          {user.role === "admin" && (
            <button role="menuitem" onClick={() => go("/admin")}>
              Админ-панель
            </button>
          )}
          <div className="user-menu-divider" />
          <button
            role="menuitem"
            className="danger"
            onClick={() => {
              setOpen(false);
              logout();
              navigate("/");
            }}
          >
            Выйти
          </button>
        </div>
      )}
    </div>
  );
}
