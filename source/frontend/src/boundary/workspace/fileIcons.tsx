import type { CSSProperties } from 'react';

/**
 * Tiny inline file/folder icons in the spirit of VSCode/Cursor's seti theme.
 * No icon-font dependency — each glyph is a single SVG so the bundle stays
 * small and the colours can pick up our theme tokens.
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

export function FolderIcon({ open, size = 14 }: { open: boolean; size?: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 16 16" aria-hidden style={{ flexShrink: 0 }}>
      {open ? (
        <path
          d="M1.5 4 L1.5 13 L14.5 13 L14.5 6.5 L7.5 6.5 L6 5 L4 5 L4 4 Z"
          fill="#c5a572"
          fillOpacity="0.85"
          stroke="none"
        />
      ) : (
        <path
          d="M1.5 4 L1.5 13 L14.5 13 L14.5 5.5 L7 5.5 L5.5 4 Z"
          fill="#c5a572"
          fillOpacity="0.85"
          stroke="none"
        />
      )}
    </svg>
  );
}

/**
 * File extension → muted accent colour. We render a generic document glyph and
 * tint it based on the file kind, which matches Cursor's quiet, low-contrast
 * style better than per-language full-colour icons.
 */
function colorForName(name: string): string {
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
  if (lower === '.gitignore' || lower === '.gitattributes' || lower === '.gitmodules') return '#e85d3a';
  if (lower === '.dockerignore' || lower === 'dockerfile' || lower.startsWith('docker-compose')) return '#0db7ed';
  if (lower.endsWith('.exe') || lower.endsWith('.dll') || lower.endsWith('.so') || lower.endsWith('.dylib') || lower.endsWith('.bin') || lower.endsWith('.elf')) return '#7a7a7a';
  return '#9da5b4';
}

export function FileIcon({ name, size = 14, style }: IconProps & { name: string }) {
  const color = colorForName(name);
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
    </svg>
  );
}
