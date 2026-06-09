import type { PluginManifest } from '@shared/plugins/contributions';

import { disassemblerToolService } from '@infrastructure/disassembly/disassemblerToolService';

import { BinaryToolPanel } from './BinaryToolPanel';
import { DisassemblerToolPanel } from './DisassemblerToolPanel';
import { FunctionListToolPanel } from './FunctionListToolPanel';
import { MemoryMapToolPanel } from './MemoryMapToolPanel';
import { PatchesToolPanel } from './PatchesToolPanel';
import { ResourcesToolPanel } from './ResourcesToolPanel';
import { StringsToolPanel } from './StringsToolPanel';
import { SymbolTableToolPanel } from './SymbolTableToolPanel';

/**
 * Binary Tools — the reverse-engineering rail. It contributes, purely through
 * the `toolTabs` contribution, the 8 tools on the right-edge ToolRail (binary /
 * disassembler / strings / symbols / memory map / functions / patches /
 * resources). Each entry carries its rail icon + label and renders its panel in
 * the tool dock; nothing here is wired into core (ToolRail / IdeToolDock read
 * the registry). The non-rail `codeEditor` split stays in core.
 *
 * Rail icons: inline SVG `<path d="…">` on a 24×24 viewBox, drawn by ToolRail
 * with `stroke="currentColor" stroke-width="1.5" fill="none"` — Lucide-style
 * strokes readable at 16px (2×2 bytes, a listing, quoted T, hashes/indices,
 * stacked sections, f-in-braces, a wrench, a framed picture).
 */
// 2×2 bytes glyph (same as the Binary/hex rail icon) for the Extensions row.
const TOOLS_GLYPH = 'M4 4h7v7H4z M13 4h7v7h-7z M4 13h7v7H4z M13 13h7v7h-7z';

const TOOLS_README = `# Binary Tools

The reverse-engineering rail: a set of read-only inspectors for binaries, on the
right-edge **ToolRail**. Open a file, pick a tool, and read its structure.

## Tools

- **Binary / hex** — raw bytes in a hex viewer.
- **Disassembler** — instruction listing.
- **Strings** — printable strings pulled from the file.
- **Symbols** — imports / exports / symbol table.
- **Memory map** — sections and their layout.
- **Function list** — discovered functions.
- **Patches** — byte-level edits.
- **Resources** — embedded PE resources.

## How to use

1. Open a binary file in the editor.
2. Click a tool on the **ToolRail** (right edge) — it opens in the tool dock and reads the active file.
3. Tools refresh automatically when the file changes on disk.

Binary Tools ships **bundled** with Cremniy and is always on.
`;

const tools: PluginManifest = {
  id: 'tools',
  name: 'Binary Tools',
  description: 'Reverse-engineering rail: hex, disassembler, strings, symbols, sections, functions, patches, resources.',
  delivery: 'bundled',
  icon: TOOLS_GLYPH,
  version: '0.1.0',
  author: 'Cremniy',
  categories: ['Reverse Engineering', 'Binary'],
  links: [
    {
      label: 'Documentation',
      url: 'https://github.com/munirov/cremniy/blob/HEAD/documentation/architecture/PLUGINS.md',
    },
    { label: 'Repository', url: 'https://github.com/munirov/cremniy' },
  ],
  readme: TOOLS_README,
  toolTabs: [
    {
      id: 'binary',
      label: 'Binary / hex',
      railIconPath: 'M4 4h7v7H4zM13 4h7v7h-7zM4 13h7v7H4zM13 13h7v7h-7z',
      render: () => <BinaryToolPanel />,
    },
    {
      id: 'disassembler',
      label: 'Disassembler',
      railIconPath: 'M4 7h6M4 12h12M4 17h9M14 7h6',
      render: () => <DisassemblerToolPanel disassembleFile={disassemblerToolService} />,
    },
    {
      id: 'strings',
      label: 'Strings',
      railIconPath: 'M6 4h12M12 4v16M7 20h10',
      render: () => <StringsToolPanel />,
    },
    {
      id: 'symbols',
      label: 'Symbols / imports / exports',
      railIconPath: 'M9 3L7 21M17 3l-2 18M4 9h17M3 15h17',
      render: () => <SymbolTableToolPanel />,
    },
    {
      id: 'memoryMap',
      label: 'Memory map (sections)',
      railIconPath: 'M3 5h18v4H3zM3 11h18v4H3zM3 17h18v4H3z',
      render: () => <MemoryMapToolPanel />,
    },
    {
      id: 'functions',
      label: 'Function list',
      railIconPath:
        'M8 4c-2 0-3 1-3 4v2c0 1.5-.5 2-2 2 1.5 0 2 .5 2 2v2c0 3 1 4 3 4M16 4c2 0 3 1 3 4v2c0 1.5.5 2 2 2-1.5 0-2 .5-2 2v2c0 3-1 4-3 4M10 12h4',
      render: () => <FunctionListToolPanel />,
    },
    {
      id: 'patches',
      label: 'Patches',
      railIconPath: 'M14 5l5 5l-9 9l-5 1l1-5zM12 7l5 5',
      render: () => <PatchesToolPanel />,
    },
    {
      id: 'resources',
      label: 'Resources (PE)',
      railIconPath:
        'M3 5h18v14H3zM3 15l5-5l4 4l3-3l6 6M8 10a1.5 1.5 0 1 0 0-3a1.5 1.5 0 0 0 0 3z',
      render: () => <ResourcesToolPanel />,
    },
  ],
};

export default tools;
