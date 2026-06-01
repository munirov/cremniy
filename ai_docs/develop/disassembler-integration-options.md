# Disassembler backend options (React tool)

Short reference for wiring `DisassemblerToolPanel` to real listings later.

| Approach | Idea | Pros | Cons |
|----------|------|------|------|
| **Native sidecar** | Spawn **radare2** / **rizin** / similar CLI from Tauri (`Command`), pipe JSON or parse listing text | Full ISA coverage, mature tooling | Packaging binaries per OS, process lifecycle, security review |
| **WASM decoder** | **Capstone** (or similar) compiled to WASM in the web layer | No separate process; predictable sandbox | Larger bundle, arch/feature limits vs native stacks |
| **Hybrid** | WASM for hot paths + optional sidecar for heavy analysis | Flexibility | Two integration surfaces |

**IPC shape (typical):** Tauri commands accept `workspaceRoot` + file path or byte slice handles and return structured rows `{ offset, rawBytes, mnemonic, operands, … }`. Until that exists, the UI stays an explicit stub with an empty listing grid.

Qt reference tree for parity: `cremniy-main/src/ToolTabs/Disassembler/` (tag `pre-qt-removal-2026-05-01`).
