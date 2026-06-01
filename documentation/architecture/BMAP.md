# BMAP — Base Multi Application Platform (Cremniy)

**Scope:** This document defines the **top-level** organization of the **Cremniy** repository: how a **single deliverable** (desktop `.exe` / `.app` / installer) is produced from a **monorepo** that combines a web UI, native shell, docs, and tooling.

It sits **above**:

- **BMFP** — frontend layering *inside* [`source/frontend/`](../../source/frontend/README.md) (React / TypeScript).
- **BMSP** — classic *API service* layering (API → Core → Infrastructure). That pattern applies **only when** a separate HTTP (or similar) backend exists. For the current product, the “server-shaped” concerns are folded into **Native shell** (Rust) instead.

---

## Why BMAP exists

Many teams are used to **frontend on one host** and **backend on another**. Here, **one process** (Tauri) hosts the UI in a WebView and exposes **commands/events** from Rust. The repo still benefits from **clear roots** (`source/frontend/`, `source/backend/`, `documentation/`, `tools/`) and **consistent mental models** (BMFP in TS, disciplined Rust modules), without pretending there is always a second deployable called “API”.

---

## Glossary

| Name | Stands for | Where it applies | Role |
|------|------------|------------------|------|
| **BMAP** | Base Multi Application Platform | **Whole repo** | Top-level layout, deliverable boundaries, what “an app” means here. |
| **BMFP** | *(your)* Base Multi **Front** Platform | **`source/frontend/src/**`** | `boundary` / `domain` / `shared` / `infrastructure`; UI + typed shells, not raw `invoke` in widgets. |
| **BMSP** | Base Multi **Service** Platform | **Future** local/remote service **or** mental map for shell | API → Core → Infrastructure **when** you have a real service. Not mandatory for v1 desktop. |

---

## Repository layout (authoritative)

| Path | Purpose |
|------|---------|
| **`source/frontend/`** | **BMFP** web app: Vite + React + TS. **Primary day-one development** for UI. |
| **`source/frontend/src/`** | React source: BMFP layers (`boundary`, `domain`, `shared`, `infrastructure`). See [BMFP (EN)](../EN/bmfp_and_layers.md). |
| **`source/backend/`** | **Native shell:** Tauri **Rust crate** — window, IPC `invoke`, plugins (dialog, fs where allowed). Not “BMSP API”, but **transport + OS integration**. |
| **`documentation/`** | Human-facing product & **contributor** docs (EN/RU), diagrams, release notes. |
| **`ai_docs/`** | ADRs, audits, migration logs, internal plans (can be promoted to `documentation/` when stable). |
| **`tools/`** | Small repo automation (e.g. `tools/scripts/` doc smoke checks). Not app runtime. |
| **`tests/`** | Optional **repo-level** test harness (integration, orchestration). Unit tests stay next to their package. |
| **`.github/workflows/`** | CI: `frontend-ci.yml`, `release.yml` (Tauri). |
| **`screenshots/`** | Marketing / docs assets (if present). |

The name **`source/backend/`** is **not** a separate HTTP service: it is the **Tauri host** crate. If you add a real BMSP-style service later (e.g. local daemon or remote API), BMAP expects something like **`services/<name>/`** or **`packages/api/`** — document the new root in this file when introduced.

---

## Native shell vs BMSP (mapping)

When people say “backend”, map concepts like this **for the current repo**:

| BMSP idea | In Cremniy today | Notes |
|-----------|------------------|-------|
| **API** (HTTP routes) | *N/A* | No public HTTP API in repo. |
| **API** (transport) | **Tauri `invoke` + events** | Thin handlers; validate inputs; no business rules duplicated sloppily in TS. |
| **Core** | **Rust:** pure logic if grown (parsers, safe subprocess orchestration) OR **TS `domain`** for UI-facing DTOs | Prefer **one** source of truth per concern; document which side owns it. |
| **Infrastructure** | **Rust:** `std::fs` only behind scoped helpers; plugins | **TS:** `infrastructure/tauri` wrappers calling `invoke`. |

When/if a **real BMSP service** appears: add it as its own package, reference **both** BMAP (repo roots) and **BMSP_ARCHITECTURE** for that package only.

---

## Testing layout

| Layer | Tests | Location |
|-------|--------|----------|
| React / BMFP | Vitest + Testing Library | `source/frontend/**/*.test.ts(x)` |
| Native shell | `cargo test` (as modules grow) | `source/backend/` |
| Repo-level / E2E (optional) | Playwright or similar | `tests/` at repo root — **not** required for MVP |

---

## Related documents

- [BMFP layers (English)](../EN/bmfp_and_layers.md) · [Russian](../RU/bmfp_and_layers.md)
- [ADR-001: Tauri primary](../../ai_docs/develop/architecture/ADR-001-tauri-desktop-primary.md)
- [ADR-002: Qt removed](../../ai_docs/develop/architecture/ADR-002-qt-sources-removed.md)
- [Tauri native bridge](../../ai_docs/develop/architecture/tauri-native-bridge.md)

---

## Versioning

Introduced **2026-05-02**. Update this file when top-level roots change (new service, moved Tauri crate, etc.). Last layout refresh: **2026-04-29** — `source/frontend` + `source/backend` split.
