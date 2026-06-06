import type { WorkspaceDirectoryEntry } from '@domain/workspace/directoryEntry';
import type { ManualNestingForDir } from './nesting';

/**
 * Purely-visual file-tree customization, persisted per workspace. v1 lives in
 * localStorage (like the pinned views); it can migrate into `.cremniy` later so
 * it travels with the project. `order` maps a directory path to a custom display
 * order of its entry names.
 */
export type TreeOrder = Record<string, string[]>;

function storageKey(workspacePath: string): string {
  return `cremniy.treeOrder:${workspacePath}`;
}

export function loadTreeOrder(workspacePath: string): TreeOrder {
  if (workspacePath === '') {
    return {};
  }
  try {
    const raw = localStorage.getItem(storageKey(workspacePath));
    if (raw != null) {
      const parsed: unknown = JSON.parse(raw);
      if (parsed != null && typeof parsed === 'object' && !Array.isArray(parsed)) {
        const out: TreeOrder = {};
        for (const [dir, names] of Object.entries(parsed as Record<string, unknown>)) {
          if (Array.isArray(names)) {
            out[dir] = names.filter((n): n is string => typeof n === 'string');
          }
        }
        return out;
      }
    }
  } catch {
    // ignore — fall back to empty (default sort)
  }
  return {};
}

export function saveTreeOrder(workspacePath: string, order: TreeOrder): void {
  if (workspacePath === '') {
    return;
  }
  try {
    localStorage.setItem(storageKey(workspacePath), JSON.stringify(order));
  } catch {
    // ignore — persistence is best-effort
  }
}

/**
 * Sort entries by the directory's custom order. Names listed in the custom
 * order come first in that order; everything else keeps its incoming order
 * (the backend's folders-first / alphabetical) right after them.
 */
export function sortByOrder(
  entries: WorkspaceDirectoryEntry[],
  dirPath: string,
  order: TreeOrder,
): WorkspaceDirectoryEntry[] {
  const custom = order[dirPath];
  if (custom == null || custom.length === 0) {
    return entries;
  }
  const rank = new Map(custom.map((n, i) => [n, i] as const));
  return [...entries].sort((a, b) => {
    const ra = rank.has(a.name) ? (rank.get(a.name) as number) : Number.POSITIVE_INFINITY;
    const rb = rank.has(b.name) ? (rank.get(b.name) as number) : Number.POSITIVE_INFINITY;
    return ra === rb ? 0 : ra - rb;
  });
}

/**
 * Manual file-nesting overrides, persisted per workspace: dirPath → (childName →
 * parentName | null). Layered on top of the automatic nesting patterns — see
 * [[nesting]]. Same localStorage-v1 home as the custom order above.
 */
export type ManualNesting = Record<string, ManualNestingForDir>;

function nestingKey(workspacePath: string): string {
  return `cremniy.treeNesting:${workspacePath}`;
}

export function loadManualNesting(workspacePath: string): ManualNesting {
  if (workspacePath === '') {
    return {};
  }
  try {
    const raw = localStorage.getItem(nestingKey(workspacePath));
    if (raw != null) {
      const parsed: unknown = JSON.parse(raw);
      if (parsed != null && typeof parsed === 'object' && !Array.isArray(parsed)) {
        const out: ManualNesting = {};
        for (const [dir, m] of Object.entries(parsed as Record<string, unknown>)) {
          if (m != null && typeof m === 'object' && !Array.isArray(m)) {
            const dirMap: ManualNestingForDir = {};
            for (const [child, parent] of Object.entries(m as Record<string, unknown>)) {
              if (parent === null || typeof parent === 'string') {
                dirMap[child] = parent;
              }
            }
            out[dir] = dirMap;
          }
        }
        return out;
      }
    }
  } catch {
    // ignore — fall back to empty (automatic nesting only)
  }
  return {};
}

export function saveManualNesting(workspacePath: string, nesting: ManualNesting): void {
  if (workspacePath === '') {
    return;
  }
  try {
    localStorage.setItem(nestingKey(workspacePath), JSON.stringify(nesting));
  } catch {
    // ignore — persistence is best-effort
  }
}
