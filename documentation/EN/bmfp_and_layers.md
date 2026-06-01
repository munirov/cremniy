# BMFP — frontend layering (Cremniy)

This project follows **BMFP** **inside** the [`source/frontend/`](../../source/frontend/README.md) package only.

## Layers (`source/frontend/src/`)

| Layer | Directory | Responsibility |
|-------|-----------|------------------|
| **boundary** | `boundary/` | Pages, layout, presentational components, composition; **no** direct OS/API calls — use infrastructure abstractions. |
| **domain** | `domain/` | Types, DTOs, enums, pure logic that defines **meaning** for the UI (no Tauri imports). |
| **shared** | `shared/` | Tokens, shared utilities/constants without domain meaning. |
| **infrastructure** | `infrastructure/` | Tauri `invoke` wrappers, persistence drivers — **thin** transport. |

**Dependency direction (ideal):** boundary → domain, shared; domain → shared; infrastructure → shared and **contracts** from domain — boundary must not import infrastructure implementation details all over (wrappers/facades are OK).

## Meta level

For **repository** layout (desktop + docs + CI), see **[BMAP — Base Multi Application Platform](../architecture/BMAP.md)**.

When a separate HTTP **backend** exists in the repo, that package would follow **BMSP**; the desktop app remains BMFP + native shell per BMAP.
