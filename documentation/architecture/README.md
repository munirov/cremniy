# Архитектура

| Документ | О чём |
|---|---|
| [BMAP](./BMAP.md) | Cremniy целиком: корни репо, нативная оболочка, связь фронта и оболочки. |
| [BMFP](./BMFP.md) | Слои фронта: `boundary / domain / infrastructure / shared`. |
| [BMBP](./BMBP.md) | Слои бэка: `api / core / infrastructure / shared`. В Cremniy — ментальная карта для нативной оболочки. |
| [AGENT_CONTROL](./AGENT_CONTROL.md) | Правило «способность = команда», API `window.cremniy`, карта кода. |

## Что брать с чего

- Делаешь UI → [BMFP](./BMFP.md).
- Добавляешь способность (кнопка/хоткей/инструмент) → [AGENT_CONTROL](./AGENT_CONTROL.md).
- Трогаешь Tauri-крейт или меняешь корни репо → [BMAP](./BMAP.md).

ADR и аудиты — в [`ai_docs/develop/architecture/`](../../ai_docs/develop/architecture/).
