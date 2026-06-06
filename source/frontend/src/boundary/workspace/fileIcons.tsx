import type { CSSProperties } from 'react';

/**
 * Inline tree icons. The folder and file glyphs come from the project icon pack
 * (clean rounded outlines on a 24px grid) so they fill the row the way Cursor's
 * do; the chevron is drawn inline. All monochrome — they read crisp against the
 * dark tree surface, while the chevron is muted by the tree CSS.
 */

const ICON_STROKE = '#d9d9d9';

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

export function FolderIcon({ size = 16 }: { open?: boolean; size?: number }) {
  // One folder silhouette — the chevron conveys open/closed.
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden style={{ flexShrink: 0 }}>
      <path
        d="M3 6.5C3 5.67157 3.67157 5 4.5 5H9L11 7H19.5C20.3284 7 21 7.67157 21 8.5V17.5C21 18.3284 20.3284 19 19.5 19H4.5C3.67157 19 3 18.3284 3 17.5V6.5Z"
        stroke={ICON_STROKE}
        strokeWidth="1.8"
        strokeLinejoin="round"
      />
    </svg>
  );
}

export function FileIcon({ size = 16, style }: IconProps & { name?: string }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden style={{ flexShrink: 0, ...style }}>
      <path
        d="M7 3H13L18 8V20C18 20.5523 17.5523 21 17 21H7C6.44772 21 6 20.5523 6 20V4C6 3.44772 6.44772 3 7 3Z"
        stroke={ICON_STROKE}
        strokeWidth="1.8"
        strokeLinejoin="round"
      />
      <path d="M13 3V8H18" stroke={ICON_STROKE} strokeWidth="1.8" strokeLinejoin="round" />
    </svg>
  );
}
