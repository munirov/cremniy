import type { ReactNode } from 'react';

/**
 * Activity-bar glyphs — monochrome currentColor outlines on a 24px grid, same
 * line language as the file icons. currentColor lets the bar mute inactive
 * views and brighten the active one.
 */

function UiIcon({ size = 18, children }: { size?: number; children: ReactNode }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.8}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
      style={{ flexShrink: 0 }}
    >
      {children}
    </svg>
  );
}

export function ExplorerIcon() {
  return (
    <UiIcon size={15}>
      <rect x="9" y="9" width="13" height="13" rx="2" />
      <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" />
    </UiIcon>
  );
}

export function SearchIcon() {
  return (
    <UiIcon size={15}>
      <circle cx="11" cy="11" r="8" />
      <path d="M21 21l-4.35-4.35" />
    </UiIcon>
  );
}

export function ChevronDownIcon() {
  return (
    <UiIcon size={13}>
      <path d="M6 9l6 6 6-6" />
    </UiIcon>
  );
}
