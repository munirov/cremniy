/**
 * Draws a plugin's `icon` (a single 24×24 stroke path `d`, same shape as a view
 * railIconPath) the way every other Cremniy glyph renders: inline SVG,
 * `stroke="currentColor" fill="none"`, thin Lucide-style stroke, size set by the
 * caller. Used on the Extensions rows (small) and the details header (large).
 *
 * Plugins without an `icon` fall back to a neutral package glyph so a row never
 * renders blank.
 */

// Neutral "extension / package" glyph for plugins that don't supply an icon.
const FALLBACK_ICON =
  'M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16zM3.27 6.96 12 12.01l8.73-5.05M12 22.08V12';

export function PluginGlyph({
  path,
  size = 17,
  className,
}: {
  path?: string;
  size?: number;
  className?: string;
}) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.5}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
      className={className}
      style={{ flexShrink: 0 }}
    >
      <path d={path ?? FALLBACK_ICON} />
    </svg>
  );
}
