# Frontend (BMFP)

This tree follows **BMFP** layering (`Docs/` conventions when `BMFP_ARCHITECTURE.md` is added).

## Toolchain

- **Vite** + **React 18** + **TypeScript** — entry `index.html` → `src/main.tsx` → `src/App.tsx` (re-exports boundary root).
- Path aliases (see `vite.config.ts` / `tsconfig.json`): `@boundary`, `@domain`, `@shared`, `@infrastructure`.

```bash
npm install
npm run dev
npm run build
```

### Desktop (Tauri 2)

Requires a [Rust toolchain](https://www.rust-lang.org/tools/install) (MSVC build tools on Windows) and WebView2 (standard on Windows 10/11).

```bash
npm install
npm run tauri:dev
```

This starts the Vite dev server and opens a native window pointed at `http://localhost:5173`. Production bundles: `npm run tauri:build` (runs `npm run build` first, then packages `dist`).

## Dependency direction

- **architecture doc** (`Docs`, scripts) → may reference anything
- **boundary** (UI, composition, interaction) → **domain**, **shared** only
- **domain** (business meaning, DTOs, enums) → **shared** only
- **infrastructure** (clients, stores, integrations) → **shared** and **domain** contracts/types only — no business rules here

Lower layers must not import higher layers (e.g. domain must not import boundary).

## This phase

**Visual-only:** placeholder shell only; **no backend/API calls.** BMFP folders (`src/boundary`, `src/domain`, `src/shared`, `src/infrastructure`) are retained with README pointers.
