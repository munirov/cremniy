# Repository-level tests

This folder is for **cross-cutting** checks: integration tests, shared fixtures, and orchestration that are not tied to a single package.

- **Frontend (Vitest):** run from `source/frontend` (`npm test`).
- **Tauri shell (Rust):** run from `source/backend` (`cargo test`).
- **Doc smoke checks:** e.g. `node tools/scripts/verify-ui-audit.mjs` from the repo root.

Add Playwright or other E2E suites here when the project needs them.
