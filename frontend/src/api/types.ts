export type Role = "user" | "admin";
export type Genre = "poem" | "prose";

export interface User {
  id: number;
  username: string;
  display_name: string;
  bio: string;
  avatar_url: string | null;
  role: Role;
  created_at: string;
}

export interface AuthorBrief {
  id: number;
  username: string;
  display_name: string;
  avatar_url: string | null;
}

export interface Work {
  id: number;
  title: string;
  body: string;
  genre: Genre;
  cover_image: string | null;
  status: "published" | "hidden";
  created_at: string;
  author: AuthorBrief;
  likes_count: number;
  comments_count: number;
  liked_by_me: boolean;
  tags: string[];
}

export interface Comment {
  id: number;
  body: string;
  created_at: string;
  author: AuthorBrief;
}

export interface Collection {
  id: number;
  slug: string | null;
  title: string;
  description: string;
  cover_image: string | null;
  hero_bg: string | null;
  created_at: string;
  works_count: number;
  curator: AuthorBrief | null;
}

export interface AdminUser {
  id: number;
  email: string;
  username: string;
  display_name: string;
  avatar_url: string | null;
  role: Role;
  created_at: string;
}

export interface AdminWork {
  id: number;
  title: string;
  genre: Genre;
  status: "published" | "hidden";
  created_at: string;
  author: AuthorBrief;
}

export interface AdminComment {
  id: number;
  body: string;
  status: "visible" | "hidden";
  created_at: string;
  work_id: number;
  author: AuthorBrief;
}
