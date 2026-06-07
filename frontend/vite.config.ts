import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    host: true,
    port: 5173,
    // В dev фронт ходит на относительный /api и /uploads — проксируем на бэкенд,
    // чтобы поведение совпадало с продом (единый origin за nginx).
    proxy: {
      "/api": { target: "http://localhost:8080", changeOrigin: true },
      "/uploads": { target: "http://localhost:8080", changeOrigin: true },
    },
  },
});
