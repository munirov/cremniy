/**
 * Editor groups — the pure state machine behind VS Code-style split editors.
 *
 * The IDE editor area is one or more **groups** laid out left→right. Each group
 * owns its *view* state — which tabs are open, which is active, which is the
 * preview tab — while the document **buffers (path → text) stay global** in the
 * session provider and are shared across groups (a file open in two groups is
 * one buffer; edits and dirty state sync, saving once saves for both).
 *
 * Everything here is pure: `(state, …args) => GroupsState`. The drag-drop UI is
 * a thin layer that dispatches into these functions, so the part that can't be
 * verified by an automated bot (the drag interactions) reduces to calls that
 * ARE exhaustively unit-tested.
 *
 * A "tab" in a group is either a file path or a center panel (settings, byte
 * tools). The focus order (MRU) is tracked per group so closing the active tab
 * falls back to the most-recently-used remaining one — never an empty pane while
 * other tabs are open. MRU keys are the file path, or `panel:<id>` for a panel.
 */

export type GroupId = string;

export type EditorGroup = {
  id: GroupId;
  /** File tabs in this group, in display order (pinned-first is a UI concern). */
  openTabs: string[];
  /** Non-file center tabs (settings, byte tools) open in this group. */
  openPanels: string[];
  /** The active file in this group (shown when `activePanel` is null). */
  activeFilePath: string | null;
  /** When non-null, this center panel is shown over the file editor. */
  activePanel: string | null;
  /** The one file tab in preview (italic) mode — replaced by the next preview. */
  previewFilePath: string | null;
  /** Focus order, most-recent last. Keys: a file path, or `panel:<id>`. */
  mru: string[];
};

export type GroupsState = {
  /** Left→right == render order. Always at least one group. */
  groups: EditorGroup[];
  /** Always references an existing group. */
  activeGroupId: GroupId;
};

export type SplitSide = 'left' | 'right';

// ── tab-key helpers (a group's MRU mixes files + panels) ──────────────────────
const PANEL_PREFIX = 'panel:';
const panelKey = (id: string): string => `${PANEL_PREFIX}${id}`;
const isPanelKey = (k: string): boolean => k.startsWith(PANEL_PREFIX);
const panelIdOf = (k: string): string => k.slice(PANEL_PREFIX.length);

let autoGroupSeq = 0;
/** Mint a process-unique group id. Tests pass explicit ids to stay deterministic. */
export function nextGroupId(): GroupId {
  autoGroupSeq += 1;
  return `g${autoGroupSeq}`;
}

function emptyGroup(id: GroupId): EditorGroup {
  return {
    id,
    openTabs: [],
    openPanels: [],
    activeFilePath: null,
    activePanel: null,
    previewFilePath: null,
    mru: [],
  };
}

/** A fresh single-group state (mirrors the legacy single-session startup). */
export function initialGroupsState(id: GroupId = 'g0'): GroupsState {
  return { groups: [emptyGroup(id)], activeGroupId: id };
}

// ── selectors ────────────────────────────────────────────────────────────────
export function getGroup(state: GroupsState, id: GroupId): EditorGroup | null {
  return state.groups.find((g) => g.id === id) ?? null;
}
export function getActiveGroup(state: GroupsState): EditorGroup {
  return getGroup(state, state.activeGroupId) ?? state.groups[0]!;
}
export function groupContaining(state: GroupsState, path: string): GroupId | null {
  return state.groups.find((g) => g.openTabs.includes(path))?.id ?? null;
}
export function isPathOpenAnywhere(state: GroupsState, path: string): boolean {
  return state.groups.some((g) => g.openTabs.includes(path));
}
/** Union of every file path open in any group — used for global buffer GC. */
export function allOpenPaths(state: GroupsState): string[] {
  const seen = new Set<string>();
  for (const g of state.groups) {
    for (const p of g.openTabs) seen.add(p);
  }
  return [...seen];
}

// ── internal group transforms (pure; operate on one group) ────────────────────
function recordFocus(group: EditorGroup, key: string): EditorGroup {
  return { ...group, mru: [...group.mru.filter((k) => k !== key), key] };
}
function forgetFocus(group: EditorGroup, key: string): EditorGroup {
  return { ...group, mru: group.mru.filter((k) => k !== key) };
}

/**
 * Re-pick this group's active tab from its MRU (most-recent still-open key),
 * falling back to the last file, then the last panel, then empty. Used after the
 * active tab is removed.
 */
function repickActive(group: EditorGroup): EditorGroup {
  const isOpen = (k: string): boolean =>
    isPanelKey(k) ? group.openPanels.includes(panelIdOf(k)) : group.openTabs.includes(k);
  for (let i = group.mru.length - 1; i >= 0; i--) {
    const k = group.mru[i]!;
    if (!isOpen(k)) continue;
    if (isPanelKey(k)) return { ...group, activePanel: panelIdOf(k) };
    return { ...group, activeFilePath: k, activePanel: null };
  }
  if (group.openTabs.length > 0) {
    return { ...group, activeFilePath: group.openTabs[group.openTabs.length - 1]!, activePanel: null };
  }
  if (group.openPanels.length > 0) {
    return { ...group, activePanel: group.openPanels[group.openPanels.length - 1]! };
  }
  return { ...group, activeFilePath: null, activePanel: null };
}

function isGroupEmpty(group: EditorGroup): boolean {
  return group.openTabs.length === 0 && group.openPanels.length === 0;
}

/** Replace one group by id with the result of `fn`, leaving the rest untouched. */
function mapGroup(state: GroupsState, id: GroupId, fn: (g: EditorGroup) => EditorGroup): GroupsState {
  return { ...state, groups: state.groups.map((g) => (g.id === id ? fn(g) : g)) };
}

// ── group-level operations ────────────────────────────────────────────────────

/** Focus a group (no-op if the id is unknown). */
export function activateGroup(state: GroupsState, id: GroupId): GroupsState {
  if (getGroup(state, id) == null || state.activeGroupId === id) {
    return state;
  }
  return { ...state, activeGroupId: id };
}

/**
 * Open a file tab in a group and make it active (also focuses the group). With
 * `preview`, a clean existing preview tab in that group is replaced in place
 * (single-click parity) instead of piling up tabs.
 */
export function openInGroup(
  state: GroupsState,
  groupId: GroupId,
  path: string,
  opts?: { preview?: boolean; cleanPreview?: boolean },
): GroupsState {
  if (getGroup(state, groupId) == null) {
    return state;
  }
  const preview = opts?.preview ?? false;
  const next = mapGroup(state, groupId, (g) => {
    let openTabs = g.openTabs;
    let previewFilePath = g.previewFilePath;
    if (!openTabs.includes(path)) {
      const old = g.previewFilePath;
      const canReplace =
        preview && old != null && old !== path && openTabs.includes(old) && (opts?.cleanPreview ?? true);
      if (canReplace && old != null) {
        openTabs = openTabs.map((p) => (p === old ? path : p));
        previewFilePath = path;
      } else {
        openTabs = [...openTabs, path];
        previewFilePath = preview ? path : g.previewFilePath;
      }
    } else if (preview) {
      previewFilePath = path;
    } else if (g.previewFilePath === path) {
      // Opening the current preview "for keeps" promotes it.
      previewFilePath = null;
    }
    const focused = recordFocus({ ...g, openTabs, previewFilePath, activeFilePath: path, activePanel: null }, path);
    return focused;
  });
  return { ...next, activeGroupId: groupId };
}

/** Activate an already-open file tab in a group (focuses the group). */
export function activateInGroup(state: GroupsState, groupId: GroupId, path: string): GroupsState {
  const g = getGroup(state, groupId);
  if (g == null || !g.openTabs.includes(path)) {
    return state;
  }
  const next = mapGroup(state, groupId, (gr) =>
    recordFocus({ ...gr, activeFilePath: path, activePanel: null }, path),
  );
  return { ...next, activeGroupId: groupId };
}

/** Open / activate a non-file center panel in a group (focuses the group). */
export function openPanelInGroup(state: GroupsState, groupId: GroupId, panelId: string): GroupsState {
  if (getGroup(state, groupId) == null) {
    return state;
  }
  const next = mapGroup(state, groupId, (g) => {
    const openPanels = g.openPanels.includes(panelId) ? g.openPanels : [...g.openPanels, panelId];
    return recordFocus({ ...g, openPanels, activePanel: panelId }, panelKey(panelId));
  });
  return { ...next, activeGroupId: groupId };
}

/** Re-focus an already-open panel in a group. */
export function activatePanelInGroup(state: GroupsState, groupId: GroupId, panelId: string): GroupsState {
  const g = getGroup(state, groupId);
  if (g == null || !g.openPanels.includes(panelId)) {
    return state;
  }
  const next = mapGroup(state, groupId, (gr) => recordFocus({ ...gr, activePanel: panelId }, panelKey(panelId)));
  return { ...next, activeGroupId: groupId };
}

/**
 * Close a file tab in a group. Re-picks the group's active tab if it was the one
 * shown, drops the preview flag if it was the preview, and collapses the group
 * when it becomes empty — unless it is the last remaining group.
 */
export function closeInGroup(state: GroupsState, groupId: GroupId, path: string): GroupsState {
  const g = getGroup(state, groupId);
  if (g == null || !g.openTabs.includes(path)) {
    return state;
  }
  let next = mapGroup(state, groupId, (gr) => {
    let g2: EditorGroup = {
      ...gr,
      openTabs: gr.openTabs.filter((t) => t !== path),
      previewFilePath: gr.previewFilePath === path ? null : gr.previewFilePath,
    };
    g2 = forgetFocus(g2, path);
    // Re-pick only if the closed file was the one on screen (no panel covering it).
    if (g2.activePanel == null && g2.activeFilePath === path) {
      g2 = repickActive(g2);
    } else if (g2.activeFilePath === path) {
      g2 = { ...g2, activeFilePath: null };
    }
    return g2;
  });
  next = collapseEmptyGroup(next, groupId);
  return next;
}

/** Close a center panel in a group (re-pick + collapse-if-empty, like files). */
export function closePanelInGroup(state: GroupsState, groupId: GroupId, panelId: string): GroupsState {
  const g = getGroup(state, groupId);
  if (g == null || !g.openPanels.includes(panelId)) {
    return state;
  }
  let next = mapGroup(state, groupId, (gr) => {
    let g2: EditorGroup = { ...gr, openPanels: gr.openPanels.filter((p) => p !== panelId) };
    g2 = forgetFocus(g2, panelKey(panelId));
    if (g2.activePanel === panelId) {
      g2 = repickActive({ ...g2, activePanel: null });
    }
    return g2;
  });
  next = collapseEmptyGroup(next, groupId);
  return next;
}

/** Reorder a file tab within a group (caller keeps pinned/unpinned zones valid). */
export function reorderInGroup(
  state: GroupsState,
  groupId: GroupId,
  fromIndex: number,
  toIndex: number,
): GroupsState {
  const g = getGroup(state, groupId);
  if (g == null) {
    return state;
  }
  const tabs = g.openTabs;
  if (
    fromIndex < 0 ||
    toIndex < 0 ||
    fromIndex >= tabs.length ||
    toIndex >= tabs.length ||
    fromIndex === toIndex
  ) {
    return state;
  }
  return mapGroup(state, groupId, (gr) => {
    const nextTabs = gr.openTabs.slice();
    const [moved] = nextTabs.splice(fromIndex, 1);
    nextTabs.splice(toIndex, 0, moved!);
    return { ...gr, openTabs: nextTabs };
  });
}

/**
 * Remove an empty group (no tabs, no panels) unless it is the only group. If the
 * removed group was active, focus its left neighbour (else the new first group).
 */
export function collapseEmptyGroup(state: GroupsState, groupId: GroupId): GroupsState {
  if (state.groups.length <= 1) {
    return state;
  }
  const idx = state.groups.findIndex((g) => g.id === groupId);
  if (idx < 0 || !isGroupEmpty(state.groups[idx]!)) {
    return state;
  }
  const groups = state.groups.filter((g) => g.id !== groupId);
  let activeGroupId = state.activeGroupId;
  if (activeGroupId === groupId) {
    const neighbour = state.groups[idx - 1] ?? state.groups[idx + 1]!;
    activeGroupId = neighbour.id;
  }
  return { groups, activeGroupId };
}

/**
 * Move a file tab from one group to another at `toIndex` (default: end), making
 * it active in the target and focusing the target. A same-group move degenerates
 * to a reorder. The source group collapses if it becomes empty (unless it is the
 * last group). If the target already has the path, it is de-duped (just
 * activated there).
 */
export function moveTabBetweenGroups(
  state: GroupsState,
  fromGroupId: GroupId,
  toGroupId: GroupId,
  path: string,
  toIndex?: number,
): GroupsState {
  const from = getGroup(state, fromGroupId);
  const to = getGroup(state, toGroupId);
  if (from == null || to == null || !from.openTabs.includes(path)) {
    return state;
  }
  if (fromGroupId === toGroupId) {
    const fromIdx = from.openTabs.indexOf(path);
    const target = toIndex == null ? from.openTabs.length - 1 : Math.min(toIndex, from.openTabs.length - 1);
    return activateInGroup(reorderInGroup(state, fromGroupId, fromIdx, target), fromGroupId, path);
  }

  // Insert into the target (de-dupe if already there).
  let next = mapGroup(state, toGroupId, (g) => {
    let openTabs = g.openTabs;
    if (!openTabs.includes(path)) {
      const at = toIndex == null ? openTabs.length : Math.max(0, Math.min(toIndex, openTabs.length));
      openTabs = [...openTabs.slice(0, at), path, ...openTabs.slice(at)];
    }
    return recordFocus({ ...g, openTabs, activeFilePath: path, activePanel: null }, path);
  });

  // Remove from the source (re-pick + collapse-if-empty).
  next = mapGroup(next, fromGroupId, (g) => {
    let g2: EditorGroup = {
      ...g,
      openTabs: g.openTabs.filter((t) => t !== path),
      previewFilePath: g.previewFilePath === path ? null : g.previewFilePath,
    };
    g2 = forgetFocus(g2, path);
    if (g2.activePanel == null && g2.activeFilePath === path) {
      g2 = repickActive(g2);
    } else if (g2.activeFilePath === path) {
      g2 = { ...g2, activeFilePath: null };
    }
    return g2;
  });
  next = { ...next, activeGroupId: toGroupId };
  return collapseEmptyGroup(next, fromGroupId);
}

/**
 * Split a file off the source group into a NEW group placed on the given side.
 * The new group holds just `path` (active). If `path` is the source's only tab,
 * this would create-then-collapse into a no-op, so it is treated as a no-op
 * (you can't split a single-tab group away from itself).
 */
export function splitGroup(
  state: GroupsState,
  sourceGroupId: GroupId,
  path: string,
  side: SplitSide,
  newGroupId: GroupId,
): GroupsState {
  const source = getGroup(state, sourceGroupId);
  if (source == null || !source.openTabs.includes(path)) {
    return state;
  }
  // Splitting the only tab of a single-tab group to a fresh group is a no-op.
  if (source.openTabs.length === 1 && source.openPanels.length === 0) {
    return activateGroup(state, sourceGroupId);
  }
  if (getGroup(state, newGroupId) != null) {
    return state; // id collision — caller must mint a fresh id
  }

  // Remove the path from the source (re-pick + preview clear).
  const groups = state.groups.map((g) => {
    if (g.id !== sourceGroupId) return g;
    let g2: EditorGroup = {
      ...g,
      openTabs: g.openTabs.filter((t) => t !== path),
      previewFilePath: g.previewFilePath === path ? null : g.previewFilePath,
    };
    g2 = forgetFocus(g2, path);
    if (g2.activePanel == null && g2.activeFilePath === path) {
      g2 = repickActive(g2);
    } else if (g2.activeFilePath === path) {
      g2 = { ...g2, activeFilePath: null };
    }
    return g2;
  });

  const created: EditorGroup = {
    ...emptyGroup(newGroupId),
    openTabs: [path],
    activeFilePath: path,
    mru: [path],
  };
  const srcIdx = groups.findIndex((g) => g.id === sourceGroupId);
  const insertAt = side === 'left' ? srcIdx : srcIdx + 1;
  const withNew = [...groups.slice(0, insertAt), created, ...groups.slice(insertAt)];
  return { groups: withNew, activeGroupId: newGroupId };
}

/**
 * Save-As / rename: rewrite `oldPath` → `newPath` in every group (a file open in
 * two groups renames in both). If a group already has `newPath` open, the old
 * tab is dropped (merge) rather than duplicated.
 */
export function renamePathInGroups(state: GroupsState, oldPath: string, newPath: string): GroupsState {
  if (oldPath === newPath || oldPath === '') {
    return state;
  }
  return {
    ...state,
    groups: state.groups.map((g) => {
      if (!g.openTabs.includes(oldPath)) {
        return g;
      }
      const alreadyHasNew = g.openTabs.includes(newPath);
      const openTabs = alreadyHasNew
        ? g.openTabs.filter((t) => t !== oldPath)
        : g.openTabs.map((t) => (t === oldPath ? newPath : t));
      const rename = (v: string | null) => (v === oldPath ? newPath : v);
      const mru = g.mru.map((k) => (k === oldPath ? newPath : k)).filter((k, i, a) => a.indexOf(k) === i);
      return {
        ...g,
        openTabs,
        activeFilePath: rename(g.activeFilePath),
        previewFilePath: rename(g.previewFilePath),
        mru,
      };
    }),
  };
}
