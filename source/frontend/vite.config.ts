import path from 'node:path';
import { defineConfig } from 'vitest/config';
import react from '@vitejs/plugin-react';

// Repo root (where `source/` and `plugins/` live, two levels up from frontend).
const repoRoot = path.resolve(__dirname, '..', '..');

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@boundary': path.resolve(__dirname, 'src/boundary'),
      '@domain': path.resolve(__dirname, 'src/domain'),
      '@shared': path.resolve(__dirname, 'src/shared'),
      '@infrastructure': path.resolve(__dirname, 'src/infrastructure'),
      // Top-level plugins folder (sibling of source/) — see PLUGINS.md.
      '@plugins': path.resolve(repoRoot, 'plugins'),
    },
    // Plugins live outside the frontend root, so resolve React (and its JSX
    // runtime) from the frontend's node_modules — and only once, to avoid a
    // second React copy breaking hooks.
    dedupe: ['react', 'react-dom'],
  },
  server: {
    // Allow Vite to serve the plugins/ folder, which sits outside the frontend root.
    fs: { allow: [repoRoot] },
  },
  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: ['./src/setupTests.ts'],
    include: ['src/**/*.{test,spec}.{ts,tsx}'],
  },
});
