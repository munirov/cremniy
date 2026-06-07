import { normalizeFsPath, parentDirectoryPath } from '@domain/workspace/paths';
import type { GitFileStatus, GitStatus } from '@infrastructure/tauri/bridge';

/**
 * Explorer git decorations — the badge letter + colour shown on a tree row to
 * mirror its git status (VS Code-style). Pure logic lives here so it can be
 * unit-tested without React; the provider/hook in GitDecorationsContext.tsx
 * fetches status and feeds it in.
 */
export type GitDecoKind =
  | 'untracked'
  | 'added'
  | 'modified'
  | 'renamed'
  | 'deleted'
  | 'conflict';

export type GitDeco = {
  kind: GitDecoKind;
  /** Single-letter badge (U/A/M/R/D/!), shown on the row for direct changes. */
  letter: string;
  /** Human label for the row title attribute. */
  label: string;
};

/** Folder rollup precedence — a folder takes the colour of its "loudest" child. */
const PRIORITY: Record<GitDecoKind, number> = {
  conflict: 6,
  deleted: 5,
  modified: 4,
  renamed: 3,
  added: 2,
  untracked: 1,
};

/** Map one porcelain file status to its decoration. */
export function fileDeco(f: GitFileStatus): GitDeco {
  if (f.untracked) {
    return { kind: 'untracked', letter: 'U', label: 'Untracked' };
  }
  const x = (f.indexStatus || ' ').charAt(0);
  const y = (f.workStatus || ' ').charAt(0);
  // Merge conflict: either side unmerged, or both-added / both-deleted.
  if (x === 'U' || y === 'U' || (x === 'A' && y === 'A') || (x === 'D' && y === 'D')) {
    return { kind: 'conflict', letter: '!', label: 'Conflict' };
  }
  // Prefer the working-tree change (unstaged) when present, else the index.
  const s = y !== ' ' ? y : x;
  switch (s) {
    case 'A':
      return { kind: 'added', letter: 'A', label: 'Added' };
    case 'D':
      return { kind: 'deleted', letter: 'D', label: 'Deleted' };
    case 'R':
      return { kind: 'renamed', letter: 'R', label: 'Renamed' };
    case 'C':
      return { kind: 'added', letter: 'C', label: 'Copied' };
    case 'T':
      return { kind: 'modified', letter: 'T', label: 'Type changed' };
    case 'M':
    default:
      return { kind: 'modified', letter: 'M', label: 'Modified' };
  }
}

export type GitDecoMaps = {
  /** Normalised abs path → decoration for the changed file/dir itself. */
  files: Map<string, GitDeco>;
  /** Normalised abs path → rolled-up decoration for an ancestor folder. */
  dirs: Map<string, GitDeco>;
};

export const EMPTY_DECO_MAPS: GitDecoMaps = { files: new Map(), dirs: new Map() };

/**
 * Build the lookup maps from the status of every discovered repo. Each changed
 * file decorates itself and tints every ancestor folder up to the workspace
 * root with the highest-priority status beneath it.
 */
export function buildDecorations(statuses: GitStatus[], workspaceRoot: string): GitDecoMaps {
  const files = new Map<string, GitDeco>();
  const dirs = new Map<string, GitDeco>();
  const rootNorm = normalizeFsPath(workspaceRoot);
  for (const st of statuses) {
    if (!st.isRepo) {
      continue;
    }
    for (const f of st.files) {
      const key = normalizeFsPath(f.absPath);
      if (key === '') {
        continue;
      }
      const deco = fileDeco(f);
      files.set(key, deco);
      // Roll up to ancestor folders, stopping at (and including) the workspace root.
      let parent = parentDirectoryPath(f.absPath);
      while (parent !== '') {
        const pk = normalizeFsPath(parent);
        const cur = dirs.get(pk);
        if (cur == null || PRIORITY[deco.kind] > PRIORITY[cur.kind]) {
          dirs.set(pk, deco);
        }
        if (pk === rootNorm) {
          break;
        }
        const next = parentDirectoryPath(parent);
        if (next === parent) {
          break;
        }
        parent = next;
      }
    }
  }
  return { files, dirs };
}

export type GitDecoResult = {
  deco: GitDeco;
  /** True when this is an inherited folder tint (no letter), false for a direct change. */
  rollup: boolean;
};

/** Look up the decoration for a tree row by its absolute path. */
export function decorationFor(
  maps: GitDecoMaps,
  absPath: string,
  isDir: boolean,
): GitDecoResult | null {
  const key = normalizeFsPath(absPath);
  if (key === '') {
    return null;
  }
  const direct = maps.files.get(key);
  if (direct != null) {
    return { deco: direct, rollup: false };
  }
  if (isDir) {
    const rolled = maps.dirs.get(key);
    if (rolled != null) {
      return { deco: rolled, rollup: true };
    }
  }
  return null;
}
