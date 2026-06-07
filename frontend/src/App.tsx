import { Navigate, Route, Routes } from "react-router-dom";
import { Layout } from "./components/Layout";
import { useAuth } from "./auth/AuthContext";
import { LandingPage } from "./pages/LandingPage";
import { FeedPage } from "./pages/FeedPage";
import { WorkPage } from "./pages/WorkPage";
import { LoginPage } from "./pages/LoginPage";
import { RegisterPage } from "./pages/RegisterPage";
import { ProfilePage } from "./pages/ProfilePage";
import { EditorPage } from "./pages/EditorPage";
import { CollectionsPage } from "./pages/CollectionsPage";
import { CollectionPage } from "./pages/CollectionPage";
import { SearchPage } from "./pages/SearchPage";
import { AdminPage } from "./pages/AdminPage";
import type { ReactNode } from "react";

function RequireAuth({ children }: { children: ReactNode }) {
  const { user, loading } = useAuth();
  if (loading) return <div className="container">Загрузка…</div>;
  if (!user) return <Navigate to="/login" replace />;
  return <>{children}</>;
}

function RequireAdmin({ children }: { children: ReactNode }) {
  const { user, loading } = useAuth();
  if (loading) return <div className="container">Загрузка…</div>;
  if (!user || user.role !== "admin") return <Navigate to="/" replace />;
  return <>{children}</>;
}

export default function App() {
  return (
    <Layout>
      <Routes>
        <Route path="/" element={<LandingPage />} />
        <Route path="/feed" element={<FeedPage />} />
        <Route path="/works/:id" element={<WorkPage />} />
        <Route path="/login" element={<LoginPage />} />
        <Route path="/register" element={<RegisterPage />} />
        <Route path="/users/:id" element={<ProfilePage />} />
        <Route path="/search" element={<SearchPage />} />
        <Route path="/collections" element={<CollectionsPage />} />
        <Route path="/collections/:id" element={<CollectionPage />} />
        <Route
          path="/new"
          element={
            <RequireAuth>
              <EditorPage />
            </RequireAuth>
          }
        />
        <Route
          path="/works/:id/edit"
          element={
            <RequireAuth>
              <EditorPage />
            </RequireAuth>
          }
        />
        <Route
          path="/admin"
          element={
            <RequireAdmin>
              <AdminPage />
            </RequireAdmin>
          }
        />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </Layout>
  );
}
