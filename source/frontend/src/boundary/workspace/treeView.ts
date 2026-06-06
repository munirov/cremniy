import type { WorkspaceDirectoryEntry } from '@domain/workspace/directoryEntry';

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
