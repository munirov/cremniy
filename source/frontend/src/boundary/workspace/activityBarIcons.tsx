import type { ReactNode } from 'react';

/**
 * Activity-bar + views-menu glyphs — monochrome currentColor outlines on a 24px
 * grid, same line language as the file icons. currentColor lets the bar mute
 * inactive views and brighten the active one.
 */

function UiIcon({ size = 16, children }: { size?: number; children: ReactNode }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.7}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
      style={{ flexShrink: 0 }}
    >
      {children}
    </svg>
  );
}

export function ExplorerIcon({ size = 16 }: { size?: number }) {
  // Two stacked documents — the canonical "files" / Explorer glyph.
  return (
    <UiIcon size={size}>
      <path d="M9 18a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h7l4 4v10a2 2 0 0 1-2 2Z" />
      <path d="M16 2v4a2 2 0 0 0 2 2h2" />
      <path d="M3 7.6v12.8A1.6 1.6 0 0 0 4.6 22h9.8" />
    </UiIcon>
  );
}

export function SearchIcon({ size = 16 }: { size?: number }) {
  return (
    <UiIcon size={size}>
      <circle cx="11" cy="11" r="8" />
      <path d="M21 21l-4.35-4.35" />
    </UiIcon>
  );
}

export function ChevronDownIcon({ size = 15 }: { size?: number }) {
  return (
    <UiIcon size={size}>
      <path d="M6 9l6 6 6-6" />
    </UiIcon>
  );
}

export function PinIcon({ filled = false, size = 13 }: { filled?: boolean; size?: number }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.7}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
      style={{ flexShrink: 0 }}
    >
      <path d="M12 17v5" />
      <path
        d="M9 10.76a2 2 0 0 1-1.11 1.79l-1.78.9A2 2 0 0 0 5 15.24V16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-.76a2 2 0 0 0-1.11-1.79l-1.78-.9A2 2 0 0 1 15 10.76V7a1 1 0 0 1 1-1 2 2 0 0 0 0-4H8a2 2 0 0 0 0 4 1 1 0 0 1 1 1z"
        fill={filled ? 'currentColor' : 'none'}
      />
    </svg>
  );
}
