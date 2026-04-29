# UI parity docs

## Verify audit markdown structure

From the repository root:

```bash
node scripts/verify-ui-audit.mjs
```

Exits `0` if `2026-04-29-qt-ui-audit.md` exists and expected heading themes (Welcome, IDE, Menu, Styles/style) are present. Exits `1` otherwise.
