import theme from './theme.json';

/**
 * Bundled file-type icon theme — our base icon set, to be customized over time.
 * Holds the SVG set + a name→icon mapping and resolves a file/folder name to its
 * icon URL, replicating VS Code's icon-theme resolution: exact filename →
 * longest matching extension → default icon.
 */

// Vite bundles + hashes each SVG; eager url glob → { './icons/x.svg': '/assets/x.hash.svg' }.
const ICON_URLS = import.meta.glob('./icons/*.svg', {
  eager: true,
  query: '?url',
  import: 'default',
}) as Record<string, string>;

type IconTheme = {
  iconDefinitions: Record<string, { iconPath: string }>;
  file: string;
  folder: string;
  folderExpanded?: string;
  folderNames?: Record<string, string>;
  fileNames?: Record<string, string>;
  fileExtensions?: Record<string, string>;
};

const t = theme as unknown as IconTheme;

function lowerKeys(m: Record<string, string> | undefined): Record<string, string> {
  const out: Record<string, string> = {};
  if (m) {
    for (const [k, v] of Object.entries(m)) {
      out[k.toLowerCase()] = v;
    }
  }
  return out;
}

const FILE_NAMES = lowerKeys(t.fileNames);
const FOLDER_NAMES = lowerKeys(t.folderNames);
const FILE_EXTENSIONS = lowerKeys(t.fileExtensions);

function urlForDef(defId: string | undefined): string | null {
  if (defId == null || defId === '') {
    return null;
  }
  const def = t.iconDefinitions[defId];
  if (def == null) {
    return null;
  }
  return ICON_URLS[def.iconPath] ?? null;
}

/** File name → themed icon URL. Exact filename, then the longest matching
 *  extension (so `app.module.css` beats `css`), then the default file icon. */
export function fileIconUrl(name: string): string | null {
  const lower = name.toLowerCase();
  const byName = urlForDef(FILE_NAMES[lower]);
  if (byName != null) {
    return byName;
  }
  const parts = lower.split('.');
  for (let i = 1; i < parts.length; i++) {
    const url = urlForDef(FILE_EXTENSIONS[parts.slice(i).join('.')]);
    if (url != null) {
      return url;
    }
  }
  return urlForDef(t.file);
}

/** Folder name → themed icon URL (named folders first, then the default). */
export function folderIconUrl(name: string, open: boolean): string | null {
  const named = urlForDef(FOLDER_NAMES[name.toLowerCase()]);
  if (named != null) {
    return named;
  }
  const expanded = open ? urlForDef(t.folderExpanded) : null;
  return expanded ?? urlForDef(t.folder);
}
