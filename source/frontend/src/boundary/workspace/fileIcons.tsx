import type { CSSProperties, ReactNode } from 'react';

/**
 * Inline tree icons. All glyphs share one visual language — a clean rounded
 * outline on a 24px grid, monochrome #d9d9d9 — matching the project icon pack
 * (see folder/file). The chevron is drawn inline in currentColor so the tree
 * CSS can mute it; the type glyphs stay crisp.
 *
 * To add a type: drop its extension into the right row of EXT and, if it needs
 * a new shape, add a glyph to GLYPHS. No per-language colours — one shape per
 * situation.
 */

const ICON_STROKE = '#d9d9d9';

type IconProps = {
  size?: number;
  style?: CSSProperties;
};

/** Shared 24-grid outline frame; children are the glyph paths. */
function Glyph({ size = 16, style, children }: { size?: number; style?: CSSProperties; children: ReactNode }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke={ICON_STROKE}
      strokeWidth={1.8}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
      style={{ flexShrink: 0, ...style }}
    >
      {children}
    </svg>
  );
}

export function ChevronIcon({ open, size = 12 }: { open: boolean; size?: number }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 16 16"
      aria-hidden
      style={{
        flexShrink: 0,
        transform: open ? 'rotate(90deg)' : 'none',
        transition: 'transform 0.1s ease',
      }}
    >
      <path d="M6 4 L10 8 L6 12" stroke="currentColor" strokeWidth="1.6" fill="none" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

export function FolderIcon({ size = 16 }: { open?: boolean; size?: number }) {
  // One folder silhouette — the chevron conveys open/closed.
  return (
    <Glyph size={size}>
      <path d="M3 6.5C3 5.67157 3.67157 5 4.5 5H9L11 7H19.5C20.3284 7 21 7.67157 21 8.5V17.5C21 18.3284 20.3284 19 19.5 19H4.5C3.67157 19 3 18.3284 3 17.5V6.5Z" />
    </Glyph>
  );
}

// ── file type → glyph ────────────────────────────────────────────────────────

type Kind = 'file' | 'code' | 'config' | 'text' | 'binary' | 'image' | 'archive' | 'shell' | 'lock';

/** The icon-pack page — folded top-right corner. Reused by the page-based kinds. */
const PAGE = (
  <>
    <path d="M7 3H13L18 8V20C18 20.5523 17.5523 21 17 21H7C6.44772 21 6 20.5523 6 20V4C6 3.44772 6.44772 3 7 3Z" />
    <path d="M13 3V8H18" />
  </>
);

const GLYPHS: Record<Kind, ReactNode> = {
  file: PAGE,
  text: (
    <>
      {PAGE}
      <path d="M9 13H15" />
      <path d="M9 16H15" />
      <path d="M9 18.5H13" />
    </>
  ),
  code: (
    <>
      {PAGE}
      <path d="M10 13L8.3 15L10 17" />
      <path d="M14 13L15.7 15L14 17" />
    </>
  ),
  config: (
    <>
      <path d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z" />
      <circle cx="12" cy="12" r="3" />
    </>
  ),
  binary: (
    <>
      <rect x="4" y="4" width="16" height="16" rx="2" />
      <rect x="9" y="9" width="6" height="6" rx="1" />
      <path d="M9 2v2" />
      <path d="M15 2v2" />
      <path d="M9 20v2" />
      <path d="M15 20v2" />
      <path d="M2 9h2" />
      <path d="M2 15h2" />
      <path d="M20 9h2" />
      <path d="M20 15h2" />
    </>
  ),
  image: (
    <>
      <rect x="3" y="3" width="18" height="18" rx="2" ry="2" />
      <circle cx="9" cy="9" r="2" />
      <path d="M21 15L17.914 11.914a2 2 0 0 0-2.828 0L6 21" />
    </>
  ),
  archive: (
    <>
      <rect x="2" y="3" width="20" height="5" rx="1" />
      <path d="M4 8v11a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8" />
      <path d="M10 12h4" />
    </>
  ),
  shell: (
    <>
      <path d="M4 17l6-6-6-6" />
      <path d="M12 19h8" />
    </>
  ),
  lock: (
    <>
      <rect x="3" y="11" width="18" height="11" rx="2" ry="2" />
      <path d="M7 11V7a5 5 0 0 1 10 0v4" />
    </>
  ),
};

/** Extension → kind. First match wins; see kindForName for whole-name rules. */
const SETS: Array<[Kind, Set<string>]> = [
  ['code', new Set('ts tsx js jsx mjs cjs cts mts c h cpp cc cxx hpp hxx hh rs go py pyw java kt kts rb php swift asm s cs lua zig v sv scala clj hs ml dart pl r jl nim d f f90 vb pas'.split(' '))],
  ['config', new Set('json jsonc json5 toml yaml yml ini cfg conf config properties plist cremniy gitignore gitattributes gitmodules dockerignore editorconfig npmrc nvmrc babelrc eslintrc prettierrc prettierignore browserslistrc'.split(' '))],
  ['text', new Set('md mdx markdown txt text rst adoc asciidoc org log csv tsv env example nfo'.split(' '))],
  ['binary', new Set('exe dll so dylib bin elf o obj a lib out app wasm class pyc pyd node ko sys rlib rmeta com msi dmg hex dump core'.split(' '))],
  ['image', new Set('png jpg jpeg gif webp ico bmp svg tiff tif avif heic heif psd xcf'.split(' '))],
  ['archive', new Set('zip tar gz tgz bz2 tbz xz txz 7z rar zst lz lzma lz4 cab iso deb rpm jar war ear apk whl nupkg'.split(' '))],
  ['shell', new Set('sh bash zsh fish ksh ps1 psm1 psd1 bat cmd nu'.split(' '))],
];

function kindForName(name: string): Kind {
  const lower = name.toLowerCase();
  if (lower.endsWith('.lock') || lower.endsWith('lock.json') || lower.endsWith('lock.yaml')) return 'lock';

  const dot = lower.lastIndexOf('.');
  // dot===0 → dotfile (.gitignore); the "extension" is the part after the dot.
  const ext = dot > 0 ? lower.slice(dot + 1) : dot === 0 ? lower.slice(1) : '';
  for (const [kind, set] of SETS) {
    if (set.has(ext)) return kind;
  }

  const stem = dot > 0 ? lower.slice(0, dot) : lower;
  if (['readme', 'license', 'licence', 'copying', 'changelog', 'changes', 'authors', 'contributors', 'notice', 'todo', 'news'].includes(stem)) return 'text';
  if (['dockerfile', 'makefile', 'cmakelists', 'justfile', 'rakefile', 'gemfile', 'procfile', 'vagrantfile'].includes(stem)) return 'config';
  return 'file';
}

export function FileIcon({ name = '', size = 16, style }: IconProps & { name?: string }) {
  return (
    <Glyph size={size} style={style}>
      {GLYPHS[kindForName(name)]}
    </Glyph>
  );
}
