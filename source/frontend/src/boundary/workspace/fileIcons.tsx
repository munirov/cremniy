import type { CSSProperties } from 'react';

/**
 * Tiny inline file/folder icons in the spirit of VSCode/Cursor's seti theme.
 * No icon-font dependency — each glyph is a single SVG so the bundle stays
 * small and the colours can pick up our theme tokens.
 *
 * Folder, dot-files and unknown files render in `currentColor`, so they inherit
 * the row's muted/active state (see the tree's row opacity). Only files with a
 * recognised extension carry a quiet language tint.
 */

type IconProps = {
  size?: number;
  style?: CSSProperties;
};

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

export function FolderIcon({ size = 14 }: { open?: boolean; size?: number }) {
  // One neutral folder silhouette — the chevron already conveys open/closed.
  // currentColor lets it dim with the row and brighten on the active file.
  return (
    <svg width={size} height={size} viewBox="0 0 16 16" aria-hidden style={{ flexShrink: 0 }}>
      <path d="M2 5.4 H6.4 L7.6 6.6 H14 V12.6 H2 Z" fill="currentColor" fillOpacity="0.85" />
    </svg>
  );
}

/**
 * Recognised file extension → quiet language tint. Returns null for anything we
 * don't have a colour for, which the caller renders as a neutral glyph.
 */
function knownAccent(name: string): string | null {
  const lower = name.toLowerCase();
  if (lower.endsWith('.ts') || lower.endsWith('.tsx')) return '#3178c6';
  if (lower.endsWith('.js') || lower.endsWith('.jsx') || lower.endsWith('.mjs') || lower.endsWith('.cjs')) return '#f0db4f';
  if (lower.endsWith('.py')) return '#3776ab';
  if (lower.endsWith('.rs')) return '#dea584';
  if (lower.endsWith('.go')) return '#00add8';
  if (lower.endsWith('.c') || lower.endsWith('.h')) return '#a8b9cc';
  if (lower.endsWith('.cpp') || lower.endsWith('.cc') || lower.endsWith('.cxx') || lower.endsWith('.hpp')) return '#9c033a';
  if (lower.endsWith('.json') || lower.endsWith('.jsonc')) return '#cbcb41';
  if (lower.endsWith('.yml') || lower.endsWith('.yaml')) return '#cb171e';
  if (lower.endsWith('.toml')) return '#9c4221';
  if (lower.endsWith('.md') || lower.endsWith('.mdx')) return '#519aba';
  if (lower.endsWith('.html') || lower.endsWith('.htm')) return '#e34c26';
  if (lower.endsWith('.css') || lower.endsWith('.scss') || lower.endsWith('.sass') || lower.endsWith('.less')) return '#563d7c';
  if (lower.endsWith('.svg') || lower.endsWith('.png') || lower.endsWith('.jpg') || lower.endsWith('.jpeg') || lower.endsWith('.gif') || lower.endsWith('.webp') || lower.endsWith('.ico')) return '#a074c4';
  if (lower.endsWith('.sh') || lower.endsWith('.bash') || lower.endsWith('.zsh') || lower.endsWith('.ps1')) return '#4eaa25';
  if (lower.endsWith('.lock')) return '#bbbbbb';
  if (lower.endsWith('.exe') || lower.endsWith('.dll') || lower.endsWith('.so') || lower.endsWith('.dylib') || lower.endsWith('.bin') || lower.endsWith('.elf')) return '#7a7a7a';
  return null;
}

/**
 * Folded-corner document. `lines` draws faint text rules inside — we use 0 for
 * dot-files / typed files and a couple for unknown files so they read as "some
 * text we don't recognise".
 */
function PageGlyph({ color, lines, size, style }: { color: string; lines: number; size: number; style?: CSSProperties }) {
  return (
    <svg width={size} height={size} viewBox="0 0 16 16" aria-hidden style={{ flexShrink: 0, ...style }}>
      <path
        d="M3 1.5 L10 1.5 L13 4.5 L13 14.5 L3 14.5 Z"
        fill="none"
        stroke={color}
        strokeOpacity="0.85"
        strokeWidth="1.2"
        strokeLinejoin="round"
      />
      <path
        d="M10 1.5 L10 4.5 L13 4.5"
        fill="none"
        stroke={color}
        strokeOpacity="0.7"
        strokeWidth="1.2"
        strokeLinejoin="round"
      />
      {lines >= 1 && <line x1="5.2" y1="8.2" x2="10.8" y2="8.2" stroke={color} strokeOpacity="0.6" strokeWidth="1.1" strokeLinecap="round" />}
      {lines >= 2 && <line x1="5.2" y1="10.6" x2="10.8" y2="10.6" stroke={color} strokeOpacity="0.6" strokeWidth="1.1" strokeLinecap="round" />}
      {lines >= 3 && <line x1="5.2" y1="13" x2="8.8" y2="13" stroke={color} strokeOpacity="0.6" strokeWidth="1.1" strokeLinecap="round" />}
    </svg>
  );
}

export function FileIcon({ name, size = 14, style }: IconProps & { name: string }) {
  // Dot-files (.cremniy, .gitignore, .env) — quiet neutral page, no rules.
  if (name.startsWith('.')) {
    return <PageGlyph color="currentColor" lines={0} size={size} style={style} />;
  }
  const accent = knownAccent(name);
  // Unknown / no extension — neutral page with text rules.
  if (accent === null) {
    return <PageGlyph color="currentColor" lines={2} size={size} style={style} />;
  }
  // Recognised type — same page in a quiet language tint.
  return <PageGlyph color={accent} lines={0} size={size} style={style} />;
}
