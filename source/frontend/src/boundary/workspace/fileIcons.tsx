import type { CSSProperties } from 'react';

import { fileIconUrl, folderIconUrl } from './fileicons/fileIconTheme';

/**
 * Tree / panel icons. File and folder glyphs come from the bundled icon theme,
 * resolved by name (see fileicons/fileIconTheme). The chevron is drawn inline in
 * currentColor so the tree CSS can mute it independently.
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
      <path
        d="M6 4 L10 8 L6 12"
        stroke="currentColor"
        strokeWidth="1.6"
        fill="none"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function ThemedIcon({
  url,
  size,
  style,
}: {
  url: string | null;
  size: number;
  style?: CSSProperties;
}) {
  if (url == null) {
    // Should not happen (resolver falls back to the default icon), but never
    // collapse the row layout if an asset is missing.
    return (
      <span
        aria-hidden
        style={{ display: 'inline-block', width: size, height: size, flexShrink: 0, ...style }}
      />
    );
  }
  return (
    <img
      src={url}
      width={size}
      height={size}
      alt=""
      draggable={false}
      style={{ flexShrink: 0, display: 'block', ...style }}
    />
  );
}

export function FileIcon({ name = '', size = 16, style }: IconProps & { name?: string }) {
  return <ThemedIcon url={fileIconUrl(name)} size={size} style={style} />;
}

export function FolderIcon({ open = false, size = 16, style }: IconProps & { open?: boolean }) {
  return <ThemedIcon url={folderIconUrl('', open)} size={size} style={style} />;
}
