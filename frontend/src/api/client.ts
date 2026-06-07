import axios from "axios";
import type {
  AdminComment,
  AdminUser,
  AdminWork,
  Collection,
  Comment,
  Genre,
  Role,
  User,
  Work,
} from "./types";

// По умолчанию ходим на тот же origin (nginx проксирует /api на бэкенд).
// VITE_API_URL можно задать для локальной разработки без прокси.
const baseURL = import.meta.env.VITE_API_URL ?? "";

// Токен живёт в httpOnly-cookie, недоступной JS, и шлётся браузером сам.
// withCredentials нужен, чтобы cookie прикреплялась и при кросс-origin вызовах.
export const api = axios.create({ baseURL, withCredentials: true });

export interface AuthResponse {
  user: User;
}

export const authApi = {
  register: (data: {
    email: string;
    username: string;
    password: string;
    display_name: string;
  }) => api.post<AuthResponse>("/api/auth/register", data).then((r) => r.data),
  login: (data: { email: string; password: string }) =>
    api.post<AuthResponse>("/api/auth/login", data).then((r) => r.data),
  logout: () => api.post("/api/auth/logout").then((r) => r.data),
  me: () => api.get<{ user: User }>("/api/auth/me").then((r) => r.data.user),
};

export interface FeedParams {
  q?: string;
  genre?: Genre;
  tag?: string;
  author_id?: number;
  collection_id?: number;
  page?: number;
  limit?: number;
}

export const worksApi = {
  feed: (params: FeedParams = {}) =>
    api
      .get<{ items: Work[]; page: number; limit: number }>("/api/works", { params })
      .then((r) => r.data),
  get: (id: number) => api.get<{ work: Work }>(`/api/works/${id}`).then((r) => r.data.work),
  create: (data: {
    title: string;
    body: string;
    genre: Genre;
    cover_image?: string;
    tags?: string[];
  }) => api.post<{ work: Work }>("/api/works", data).then((r) => r.data.work),
  update: (
    id: number,
    data: { title: string; body: string; genre: Genre; cover_image?: string; tags?: string[] },
  ) => api.put<{ work: Work }>(`/api/works/${id}`, data).then((r) => r.data.work),
  remove: (id: number) => api.delete(`/api/works/${id}`).then((r) => r.data),
  like: (id: number) =>
    api
      .post<{ liked: boolean; likes_count: number }>(`/api/works/${id}/like`)
      .then((r) => r.data),
  unlike: (id: number) =>
    api
      .delete<{ liked: boolean; likes_count: number }>(`/api/works/${id}/like`)
      .then((r) => r.data),
};

export const commentsApi = {
  list: (workId: number) =>
    api.get<{ items: Comment[] }>(`/api/works/${workId}/comments`).then((r) => r.data.items),
  create: (workId: number, body: string) =>
    api
      .post<{ comment: Comment }>(`/api/works/${workId}/comments`, { body })
      .then((r) => r.data.comment),
  remove: (commentId: number) =>
    api.delete(`/api/comments/${commentId}`).then((r) => r.data),
};

export const usersApi = {
  search: (q: string) =>
    api.get<{ items: User[] }>("/api/users", { params: { q } }).then((r) => r.data.items),
  profile: (id: number) =>
    api.get<{ user: User; works: Work[] }>(`/api/users/${id}`).then((r) => r.data),
  updateMe: (data: { display_name: string; bio?: string; avatar_url?: string }) =>
    api.patch<{ user: User }>("/api/users/me", data).then((r) => r.data.user),
};

export const uploadsApi = {
  upload: (blob: Blob) =>
    api
      .post<{ url: string }>("/api/uploads", blob, {
        headers: { "Content-Type": blob.type || "application/octet-stream" },
      })
      .then((r) => r.data.url),
};

export const collectionsApi = {
  list: () =>
    api.get<{ items: Collection[] }>("/api/collections").then((r) => r.data.items),
  get: (id: number) =>
    api
      .get<{ collection: Collection; works: Work[] }>(`/api/collections/${id}`)
      .then((r) => r.data),
  create: (data: {
    title: string;
    description?: string;
    slug?: string;
    cover_image?: string;
    hero_bg?: string;
  }) => api.post<{ id: number }>("/api/collections", data).then((r) => r.data),
  update: (
    id: number,
    data: {
      title: string;
      description?: string;
      slug?: string;
      cover_image?: string;
      hero_bg?: string;
    },
  ) => api.put(`/api/collections/${id}`, data).then((r) => r.data),
  remove: (id: number) => api.delete(`/api/collections/${id}`).then((r) => r.data),
  addWork: (id: number, workId: number, position = 0) =>
    api
      .post(`/api/collections/${id}/works`, { work_id: workId, position })
      .then((r) => r.data),
  removeWork: (id: number, workId: number) =>
    api.delete(`/api/collections/${id}/works/${workId}`).then((r) => r.data),
};

export const adminApi = {
  works: () => api.get<{ items: AdminWork[] }>("/api/admin/works").then((r) => r.data.items),
  comments: () =>
    api.get<{ items: AdminComment[] }>("/api/admin/comments").then((r) => r.data.items),
  setWorkStatus: (id: number, status: "published" | "hidden") =>
    api.patch(`/api/admin/works/${id}/status`, { status }).then((r) => r.data),
  setCommentStatus: (id: number, status: "visible" | "hidden") =>
    api.patch(`/api/admin/comments/${id}/status`, { status }).then((r) => r.data),
  users: (q = "") =>
    api.get<{ items: AdminUser[] }>("/api/admin/users", { params: { q } }).then((r) => r.data.items),
  setUserRole: (id: number, role: Role) =>
    api.patch(`/api/admin/users/${id}/role`, { role }).then((r) => r.data),
};

export function apiErrorMessage(err: unknown): string {
  if (axios.isAxiosError(err)) {
    const data = err.response?.data as { error?: string } | undefined;
    return data?.error || err.message;
  }
  return "Неизвестная ошибка";
}
