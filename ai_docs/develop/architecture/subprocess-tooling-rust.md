# Subprocess tooling (Rust-only in Tauri)

**Scope:** External binary tooling invoked from the desktop app — e.g. **radare2**, **objdump**, file-format helpers, and similar CLIs.

## Principle

**All** external tool invocation from the desktop application goes through **Rust** in the Tauri shell (`frontend/src-tauri/`), not through JavaScript (`child_process`, shell wrappers, or ad-hoc `spawn` in Node or the browser runtime).

**Rationale:** Centralizes policy (argv shape, timeouts, cwd, output caps), keeps the web layer free of OS process APIs, and aligns with BMFP: **infrastructure** exposes a thin IPC surface; **domain** expresses intent; **boundary** never shells out directly.

The prior Qt stack used C++ `QProcess`-style orchestration for the disassembler worker; the Tauri migration **must not** reintroduce execution bridges from untyped UI or settings strings into raw process argv.

---

## Security checklist (migrated from Qt audit)

These items preserve hardening called out for the legacy desktop; regressions here are considered **blockers** for subprocess features in Tauri.

1. **No unvalidated user strings in argv** — Every segment passed to `std::process::Command` (or equivalent) is either a fixed literal, an allowlisted token, or passed through a **narrow validation** step (pattern, enum, bounded length). Never concatenate “settings text” or file contents into argv without parsing into a safe structure first.
2. **Allowlist subcommands and flags** — Tools that support many subcommands (e.g. `r2`) are invoked with **explicit, versioned** argument templates; UI may only choose among **predefined** operations, not arbitrary CLI text.
3. **Timeouts** — Each invocation has a wall-clock limit; kill the child (and optionally reap) on expiry; surface timeout as a typed error to the domain layer.
4. **Working directory** — `current_dir` is restricted to the **opened workspace root** (after Rust-side canonicalization and prefix check) or a **dedicated temp** directory created for the session; never `cwd` = user home or arbitrary paths from prefs without the same validation as workspace paths.
5. **radare2 `-c` / pre-commands** — **Never** pass arbitrary r2 `-c` strings from settings or imported config without validation. Legacy audit **[S1]** (`cremniy/ai_docs/develop/audits/2026-04-29-cremniy-full-audit.md`): unvalidated radare2 pre-commands in `-c` were a **critical** arbitrary-execution vector. In Tauri: either remove user-supplied pre-command surfaces, or validate against a **strict allowlist** with no metacharacters / script injection paths; prefer fixed prelude scripts shipped with the app.

**Additional practices:** Prefer argument vectors over shell strings (`shell=False` equivalent). Cap stdout/stderr size. Log command **shape** (names only) in debug builds, not full secrets from env.

---

## Phase 2 — Disassembler worker port (implementation notes)

- **Ownership:** Replace `DisassemblerWorker` / `radare2backend` C++ paths with a Rust module under `src-tauri` that owns **one** long-lived or pooled subprocess policy per workspace session (exact lifecycle TBD; avoid unbounded concurrent r2 children).
- **IPC contract:** Expose Tauri **commands** (or controlled **events** for streamed output) that accept **typed** requests: e.g. binary path (must resolve under workspace), function name or RVA band, **enum** for “disasm at entry” vs “list symbols” — not raw CLI strings from React.
- **Streaming:** If stdout is line-based JSON or plain disasm text, buffer in Rust with backpressure; frontend receives chunks via invoke polling or emit channels — **do not** pipe megabytes into the JS heap unbounded in one shot.
- **Settings:** Any “extra r2 args” or “prelude” from persisted prefs must go through the **same** allowlist/validation as interactive UI; tie to **[S1]** remediation in code review.
- **Parity baseline:** Historical Qt disassembler behavior lived under `src/ToolTabs/Disassembler/` (removed from tree). Use git tag **`pre-qt-removal-2026-05-01`** or history to read expectations (timeouts, error strings, file open contract) but **reimplement** invocation in Rust; do not transliterate `buildPrelude` verbatim without validation.

---

## References

- `cremniy/ai_docs/develop/architecture/tauri-native-bridge.md` — IPC boundaries; Rust owns subprocess policy.
- `cremniy/ai_docs/develop/audits/2026-04-29-cremniy-full-audit.md` — **[S1]** radare2 pre-commands.
