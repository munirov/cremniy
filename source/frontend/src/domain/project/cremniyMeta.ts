/**
 * `.cremniy` is the per-project home for everything the IDE wants to remember
 * across sessions. It lives in the project root next to its source files.
 *
 *   - `meta.name` / `meta.language` — set at create-time, shown in the
 *     window title and used as defaults for new files
 *   - `meta.createdAt` / `meta.lastOpenedAt` — diagnostics, recent list
 *   - `meta.session` — restored on next open: which files were open, which
 *     was active, pane sizes, terminal cwd + history pointer, etc.
 *
 * Unknown / future keys are preserved on round-trip — when normalising we
 * only fix up shapes we care about and leave the rest as `unknown`. This
 * way a newer Cremniy that adds more session state doesn't lose data when
 * an older build saves the file back.
 */

export type CremniyLanguage = 'C' | 'C++' | 'ASM' | 'C + ASM' | 'Custom';

export type CremniyPaneSizes = {
  /** Horizontal outer split widths. */
  outer?: number[];
  /** Vertical center split (editor + terminal). */
  center?: number[];
};

export type CremniySessionState = {
  /** Workspace files that were open in the editor (paths in their saved order). */
  openFiles?: string[];
  /** The file that had focus when the session was last saved. */
  activeFile?: string | null;
  /** Pinned editor tabs (Set serialises as array). */
  pinnedFiles?: string[];
  /** Last selected tool tab in the right rail, `null` = closed. */
  activeToolTab?: string | null;
  /** Pane layout (matches IdeLayoutSizes shape). */
  paneSizes?: CremniyPaneSizes;
  /** Whether the terminal panel was visible. */
  terminalVisible?: boolean;
  /** Workspace-relative cwd the terminal was last sitting in. */
  terminalCwd?: string | null;
};

export type CremniyMeta = {
  /** Schema version. Bump when we change shape in a breaking way. */
  version: number;
  /** Display name (defaults to folder name on create). */
  name: string;
  /** Project language; defines templates + disasm syntax defaults. */
  language: CremniyLanguage;
  /** Unix timestamp (ms). */
  createdAt: number;
  /** Unix timestamp (ms). */
  lastOpenedAt: number;
  /** Restorable session state. */
  session: CremniySessionState;
};

export const CREMNIY_META_VERSION = 1;

export function emptyCremniyMeta(
  name: string,
  language: CremniyLanguage = 'C',
): CremniyMeta {
  // `now` deliberately stamped here so `createdAt === lastOpenedAt` on freshly
  // created projects. Caller can overwrite either field if they need to.
  const now = Date.now();
  return {
    version: CREMNIY_META_VERSION,
    name,
    language,
    createdAt: now,
    lastOpenedAt: now,
    session: {
      openFiles: [],
      activeFile: null,
      pinnedFiles: [],
      activeToolTab: null,
      paneSizes: {},
      terminalVisible: true,
      terminalCwd: null,
    },
  };
}

function isLanguage(v: unknown): v is CremniyLanguage {
  return v === 'C' || v === 'C++' || v === 'ASM' || v === 'C + ASM' || v === 'Custom';
}

/** Tolerant parser — unknown values fall back to defaults, never throws. */
export function parseCremniyMeta(json: string, fallbackName: string): CremniyMeta {
  let raw: unknown;
  try {
    raw = JSON.parse(json);
  } catch {
    return emptyCremniyMeta(fallbackName);
  }
  if (raw == null || typeof raw !== 'object' || Array.isArray(raw)) {
    return emptyCremniyMeta(fallbackName);
  }
  const o = raw as Record<string, unknown>;
  const fallback = emptyCremniyMeta(fallbackName);
  const sessionRaw = o.session;
  const session: CremniySessionState =
    sessionRaw != null && typeof sessionRaw === 'object' && !Array.isArray(sessionRaw)
      ? normaliseSession(sessionRaw as Record<string, unknown>, fallback.session)
      : fallback.session;
  return {
    version: typeof o.version === 'number' ? o.version : fallback.version,
    name: typeof o.name === 'string' && o.name.trim() !== '' ? o.name : fallback.name,
    language: isLanguage(o.language) ? o.language : fallback.language,
    createdAt:
      typeof o.createdAt === 'number' && Number.isFinite(o.createdAt)
        ? o.createdAt
        : fallback.createdAt,
    lastOpenedAt:
      typeof o.lastOpenedAt === 'number' && Number.isFinite(o.lastOpenedAt)
        ? o.lastOpenedAt
        : fallback.lastOpenedAt,
    session,
  };
}

function normaliseSession(
  o: Record<string, unknown>,
  fb: CremniySessionState,
): CremniySessionState {
  const arr = (v: unknown): string[] | undefined =>
    Array.isArray(v) ? v.filter((x): x is string => typeof x === 'string') : undefined;
  const paneSizesRaw = o.paneSizes;
  let paneSizes: CremniyPaneSizes | undefined;
  if (paneSizesRaw != null && typeof paneSizesRaw === 'object' && !Array.isArray(paneSizesRaw)) {
    const p = paneSizesRaw as Record<string, unknown>;
    paneSizes = {
      outer: Array.isArray(p.outer)
        ? p.outer.filter((x): x is number => typeof x === 'number' && Number.isFinite(x))
        : undefined,
      center: Array.isArray(p.center)
        ? p.center.filter((x): x is number => typeof x === 'number' && Number.isFinite(x))
        : undefined,
    };
  }
  return {
    openFiles: arr(o.openFiles) ?? fb.openFiles,
    activeFile:
      typeof o.activeFile === 'string' || o.activeFile === null
        ? (o.activeFile as string | null)
        : fb.activeFile,
    pinnedFiles: arr(o.pinnedFiles) ?? fb.pinnedFiles,
    activeToolTab:
      typeof o.activeToolTab === 'string' || o.activeToolTab === null
        ? (o.activeToolTab as string | null)
        : fb.activeToolTab,
    paneSizes: paneSizes ?? fb.paneSizes,
    terminalVisible:
      typeof o.terminalVisible === 'boolean' ? o.terminalVisible : fb.terminalVisible,
    terminalCwd:
      typeof o.terminalCwd === 'string' || o.terminalCwd === null
        ? (o.terminalCwd as string | null)
        : fb.terminalCwd,
  };
}

export function stringifyCremniyMeta(meta: CremniyMeta): string {
  return JSON.stringify(meta, null, 2) + '\n';
}
