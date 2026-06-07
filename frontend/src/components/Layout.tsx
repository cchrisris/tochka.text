import { NavLink, useLocation } from "react-router-dom";
import { type ReactNode } from "react";
import { useAuth } from "../auth/AuthContext";
import { UserMenu } from "./UserMenu";

export function Layout({ children }: { children: ReactNode }) {
  const { user } = useAuth();
  const location = useLocation();
  const isLanding = location.pathname === "/";

  if (isLanding) {
    return <div className="app-shell">{children}</div>;
  }

  return (
    <div className="app-shell">
      <header className="site-header">
        <div className="site-header-inner">
          <NavLink to="/" className="brand">
            Tochka.Text
          </NavLink>
          <nav className="nav">
            <NavLink to="/feed">Лента</NavLink>
            <NavLink to="/collections">Подборки</NavLink>
            <NavLink to="/search">Поиск</NavLink>
            {user ? (
              <UserMenu />
            ) : (
              <>
                <NavLink to="/login">Войти</NavLink>
                <NavLink to="/register" className="btn btn-sm">
                  Регистрация
                </NavLink>
              </>
            )}
          </nav>
        </div>
      </header>
      <main>{children}</main>
    </div>
  );
}
