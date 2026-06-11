import { describe, expect, it } from 'vitest';

import {
  activateGroup,
  activateInGroup,
  activatePanelInGroup,
  allOpenPaths,
  closeInGroup,
  closePanelInGroup,
  collapseEmptyGroup,
  duplicateToNewGroup,
  getActiveGroup,
  getGroup,
  groupContaining,
  initialGroupsState,
  isPathOpenAnywhere,
  moveTabBetweenGroups,
  openInGroup,
  openPanelInGroup,
  renamePathInGroups,
  reorderInGroup,
  splitGroup,
  type GroupsState,
} from './editorGroups';

/** Every operation must leave `activeGroupId` pointing at a live group. */
function expectActiveGroupLive(state: GroupsState): void {
  expect(state.groups.some((g) => g.id === state.activeGroupId)).toBe(true);
  expect(state.groups.length).toBeGreaterThanOrEqual(1);
}

/** A one-group state with the given files opened in order (last is active). */
function withFiles(groupId: string, ...paths: string[]): GroupsState {
  let s = initialGroupsState(groupId);
  for (const p of paths) s = openInGroup(s, groupId, p);
  return s;
}

describe('initialGroupsState', () => {
  it('is a single empty group that is active', () => {
    const s = initialGroupsState('g0');
    expect(s.groups).toHaveLength(1);
    expect(s.activeGroupId).toBe('g0');
    expect(s.groups[0]).toMatchObject({ id: 'g0', openTabs: [], activeFilePath: null, activePanel: null });
    expectActiveGroupLive(s);
  });
});

describe('openInGroup', () => {
  it('adds a tab, makes it active, focuses the group', () => {
    const s = openInGroup(initialGroupsState('g0'), 'g0', '/a.ts');
    const g = getActiveGroup(s);
    expect(g.openTabs).toEqual(['/a.ts']);
    expect(g.activeFilePath).toBe('/a.ts');
    expect(g.activePanel).toBeNull();
    expect(s.activeGroupId).toBe('g0');
  });

  it('appends multiple tabs in order, last active', () => {
    const s = withFiles('g0', '/a', '/b', '/c');
    expect(getActiveGroup(s).openTabs).toEqual(['/a', '/b', '/c']);
    expect(getActiveGroup(s).activeFilePath).toBe('/c');
  });

  it('does not duplicate an already-open tab; just activates it', () => {
    let s = withFiles('g0', '/a', '/b');
    s = openInGroup(s, 'g0', '/a');
    expect(getActiveGroup(s).openTabs).toEqual(['/a', '/b']);
    expect(getActiveGroup(s).activeFilePath).toBe('/a');
  });

  it('preview reuses a clean preview slot in place instead of piling up', () => {
    let s = openInGroup(initialGroupsState('g0'), 'g0', '/a', { preview: true });
    expect(getActiveGroup(s).previewFilePath).toBe('/a');
    s = openInGroup(s, 'g0', '/b', { preview: true });
    expect(getActiveGroup(s).openTabs).toEqual(['/b']); // /a replaced, not appended
    expect(getActiveGroup(s).previewFilePath).toBe('/b');
  });

  it('keeps a dirty preview tab (cleanPreview=false appends instead of replacing)', () => {
    let s = openInGroup(initialGroupsState('g0'), 'g0', '/a', { preview: true });
    s = openInGroup(s, 'g0', '/b', { preview: true, cleanPreview: false });
    expect(getActiveGroup(s).openTabs).toEqual(['/a', '/b']);
  });

  it('opening the current preview for keeps promotes it (clears preview flag)', () => {
    let s = openInGroup(initialGroupsState('g0'), 'g0', '/a', { preview: true });
    s = openInGroup(s, 'g0', '/a'); // for keeps
    expect(getActiveGroup(s).previewFilePath).toBeNull();
    expect(getActiveGroup(s).openTabs).toEqual(['/a']);
  });

  it('is a no-op for an unknown group', () => {
    const s = openInGroup(initialGroupsState('g0'), 'nope', '/a');
    expect(getActiveGroup(s).openTabs).toEqual([]);
  });
});

describe('activateInGroup', () => {
  it('activates an open tab and focuses its group', () => {
    let s = withFiles('g0', '/a', '/b');
    s = activateInGroup(s, 'g0', '/a');
    expect(getActiveGroup(s).activeFilePath).toBe('/a');
  });
  it('ignores a path that is not open in the group', () => {
    const s = withFiles('g0', '/a');
    expect(activateInGroup(s, 'g0', '/x')).toBe(s);
  });
});

describe('panels', () => {
  it('opens, activates, and closes a center panel in a group', () => {
    let s = openPanelInGroup(initialGroupsState('g0'), 'g0', 'settings');
    expect(getActiveGroup(s).openPanels).toEqual(['settings']);
    expect(getActiveGroup(s).activePanel).toBe('settings');
    s = openInGroup(s, 'g0', '/a'); // showing a file clears the panel overlay
    expect(getActiveGroup(s).activePanel).toBeNull();
    s = activatePanelInGroup(s, 'g0', 'settings');
    expect(getActiveGroup(s).activePanel).toBe('settings');
    s = closePanelInGroup(s, 'g0', 'settings');
    expect(getActiveGroup(s).openPanels).toEqual([]);
    // falls back to the file underneath
    expect(getActiveGroup(s).activePanel).toBeNull();
    expect(getActiveGroup(s).activeFilePath).toBe('/a');
  });
});

describe('closeInGroup', () => {
  it('removes the tab and re-picks the most-recently-used remaining one (MRU)', () => {
    let s = withFiles('g0', '/a', '/b', '/c'); // mru: a,b,c
    s = activateInGroup(s, 'g0', '/a'); // mru: b,c,a  active=a
    s = closeInGroup(s, 'g0', '/a');
    expect(getActiveGroup(s).openTabs).toEqual(['/b', '/c']);
    expect(getActiveGroup(s).activeFilePath).toBe('/c'); // most-recent before /a
  });

  it('closing a non-active tab leaves the active one untouched', () => {
    let s = withFiles('g0', '/a', '/b'); // active=b
    s = closeInGroup(s, 'g0', '/a');
    expect(getActiveGroup(s).activeFilePath).toBe('/b');
    expect(getActiveGroup(s).openTabs).toEqual(['/b']);
  });

  it('clears the preview flag when the preview tab is closed', () => {
    let s = openInGroup(initialGroupsState('g0'), 'g0', '/a', { preview: true });
    s = closeInGroup(s, 'g0', '/a');
    expect(getActiveGroup(s).previewFilePath).toBeNull();
  });

  it('the LAST group stays (empty) when its last tab closes', () => {
    let s = withFiles('g0', '/a');
    s = closeInGroup(s, 'g0', '/a');
    expect(s.groups).toHaveLength(1);
    expect(getActiveGroup(s).openTabs).toEqual([]);
    expect(getActiveGroup(s).activeFilePath).toBeNull();
    expectActiveGroupLive(s);
  });
});

describe('reorderInGroup', () => {
  it('moves a tab within its zone', () => {
    let s = withFiles('g0', '/a', '/b', '/c');
    s = reorderInGroup(s, 'g0', 0, 2);
    expect(getActiveGroup(s).openTabs).toEqual(['/b', '/c', '/a']);
  });
  it('ignores out-of-range / no-op indices', () => {
    const s = withFiles('g0', '/a', '/b');
    expect(reorderInGroup(s, 'g0', 0, 0)).toBe(s);
    expect(reorderInGroup(s, 'g0', 5, 0)).toBe(s);
  });
});

describe('splitGroup', () => {
  it('splits a file into a new group on the right, removing it from the source', () => {
    let s = withFiles('g1', '/a', '/b'); // active=b
    s = splitGroup(s, 'g1', '/b', 'right', 'g2');
    expect(s.groups.map((g) => g.id)).toEqual(['g1', 'g2']);
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/a']);
    expect(getGroup(s, 'g1')!.activeFilePath).toBe('/a'); // re-picked
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/b']);
    expect(getGroup(s, 'g2')!.activeFilePath).toBe('/b');
    expect(s.activeGroupId).toBe('g2');
    expectActiveGroupLive(s);
  });

  it('inserts the new group on the left when side=left', () => {
    let s = withFiles('g1', '/a', '/b');
    s = splitGroup(s, 'g1', '/a', 'left', 'g2');
    expect(s.groups.map((g) => g.id)).toEqual(['g2', 'g1']);
  });

  it('is a no-op when splitting a single-tab group (nothing to split away from)', () => {
    const s = withFiles('g1', '/a');
    const after = splitGroup(s, 'g1', '/a', 'right', 'g2');
    expect(after.groups).toHaveLength(1);
    expect(getGroup(after, 'g1')!.openTabs).toEqual(['/a']);
  });

  it('ignores an unknown path or a colliding new id', () => {
    const s = withFiles('g1', '/a', '/b');
    expect(splitGroup(s, 'g1', '/x', 'right', 'g2')).toBe(s);
    expect(splitGroup(s, 'g1', '/b', 'right', 'g1')).toBe(s);
  });
});

describe('duplicateToNewGroup', () => {
  it('opens the file in a NEW group on the right while keeping it in the source (shared)', () => {
    let s = withFiles('g1', '/a', '/b'); // active=b
    s = duplicateToNewGroup(s, 'g1', '/b', 'right', 'g2');
    expect(s.groups.map((g) => g.id)).toEqual(['g1', 'g2']);
    // Source UNCHANGED — /b still there, both tabs, /b still active.
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/a', '/b']);
    expect(getGroup(s, 'g1')!.activeFilePath).toBe('/b');
    // New group holds just /b, active, mru seeded.
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/b']);
    expect(getGroup(s, 'g2')!.activeFilePath).toBe('/b');
    expect(getGroup(s, 'g2')!.mru).toEqual(['/b']);
    // The file is open in BOTH groups (one shared buffer).
    expect(isPathOpenAnywhere(s, '/b')).toBe(true);
    expect(s.groups.filter((g) => g.openTabs.includes('/b'))).toHaveLength(2);
    // New group is active.
    expect(s.activeGroupId).toBe('g2');
    expectActiveGroupLive(s);
  });

  it('inserts the new group on the left when side=left', () => {
    let s = withFiles('g1', '/a', '/b');
    s = duplicateToNewGroup(s, 'g1', '/a', 'left', 'g2');
    expect(s.groups.map((g) => g.id)).toEqual(['g2', 'g1']);
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/a']);
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/a', '/b']); // source still has /a
  });

  it('duplicates a single-tab group (unlike splitGroup, this is NOT a no-op)', () => {
    let s = withFiles('g1', '/a'); // only tab
    s = duplicateToNewGroup(s, 'g1', '/a', 'right', 'g2');
    expect(s.groups.map((g) => g.id)).toEqual(['g1', 'g2']);
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/a']); // source kept it
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/a']);
    expect(s.activeGroupId).toBe('g2');
  });

  it('leaves the source group active-file / preview / mru untouched', () => {
    let s = openInGroup(initialGroupsState('g1'), 'g1', '/a', { preview: true });
    s = openInGroup(s, 'g1', '/b'); // /b for keeps, active; /a still preview
    const sourceBefore = getGroup(s, 'g1')!;
    s = duplicateToNewGroup(s, 'g1', '/b', 'right', 'g2');
    expect(getGroup(s, 'g1')).toEqual(sourceBefore); // byte-identical source group
    expect(getGroup(s, 'g1')!.previewFilePath).toBe('/a');
  });

  it('ignores an unknown path or a colliding new id', () => {
    const s = withFiles('g1', '/a', '/b');
    expect(duplicateToNewGroup(s, 'g1', '/x', 'right', 'g2')).toBe(s);
    expect(duplicateToNewGroup(s, 'nope', '/a', 'right', 'g2')).toBe(s);
    expect(duplicateToNewGroup(s, 'g1', '/b', 'right', 'g1')).toBe(s); // id collision
  });
});

describe('moveTabBetweenGroups', () => {
  function twoGroups(): GroupsState {
    // g1: [/a,/b], g2: [/c]
    let s = withFiles('g1', '/a', '/b');
    s = splitGroup(s, 'g1', '/b', 'right', 'g2'); // g1:[/a], g2:[/b]
    s = openInGroup(s, 'g2', '/c'); // g2:[/b,/c]
    s = openInGroup(s, 'g1', '/d'); // g1:[/a,/d]
    return s; // g1:[/a,/d], g2:[/b,/c]
  }

  it('moves a tab to another group at the end, active in target, focuses target', () => {
    let s = twoGroups();
    s = moveTabBetweenGroups(s, 'g1', 'g2', '/a');
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/d']);
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/b', '/c', '/a']);
    expect(getGroup(s, 'g2')!.activeFilePath).toBe('/a');
    expect(s.activeGroupId).toBe('g2');
  });

  it('inserts at a given index', () => {
    let s = twoGroups();
    s = moveTabBetweenGroups(s, 'g1', 'g2', '/a', 0);
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/a', '/b', '/c']);
  });

  it('collapses the source when its last tab moves away', () => {
    let s = twoGroups();
    s = moveTabBetweenGroups(s, 'g1', 'g2', '/a');
    s = moveTabBetweenGroups(s, 'g1', 'g2', '/d'); // g1 now empty → collapses
    expect(s.groups.map((g) => g.id)).toEqual(['g2']);
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/b', '/c', '/a', '/d']);
    expectActiveGroupLive(s);
  });

  it('de-dupes when the target already has the path', () => {
    let s = twoGroups();
    s = openInGroup(s, 'g1', '/c'); // /c now in both g1 and g2
    s = moveTabBetweenGroups(s, 'g1', 'g2', '/c');
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/b', '/c']); // not doubled
    expect(getGroup(s, 'g2')!.activeFilePath).toBe('/c');
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/a', '/d']); // /c removed from source
  });

  it('a same-group move is a reorder', () => {
    let s = twoGroups();
    s = moveTabBetweenGroups(s, 'g1', 'g1', '/a'); // move /a to end of g1
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/d', '/a']);
  });
});

describe('collapseEmptyGroup', () => {
  it('removes an empty non-last group and re-homes the active id to the left neighbour', () => {
    let s = withFiles('g1', '/a', '/b');
    s = splitGroup(s, 'g1', '/b', 'right', 'g2'); // g1:[/a], g2:[/b], active g2
    s = closeInGroup(s, 'g2', '/b'); // g2 empties → collapses via closeInGroup
    expect(s.groups.map((g) => g.id)).toEqual(['g1']);
    expect(s.activeGroupId).toBe('g1');
    expectActiveGroupLive(s);
  });
  it('never removes the last group', () => {
    let s = withFiles('g0', '/a');
    s = closeInGroup(s, 'g0', '/a');
    expect(collapseEmptyGroup(s, 'g0').groups).toHaveLength(1);
  });
});

describe('renamePathInGroups', () => {
  it('renames a path in every group that holds it (and active/preview/mru)', () => {
    let s = withFiles('g1', '/a', '/b');
    s = splitGroup(s, 'g1', '/b', 'right', 'g2');
    s = openInGroup(s, 'g2', '/a'); // /a now in g1 and g2
    s = renamePathInGroups(s, '/a', '/renamed');
    expect(getGroup(s, 'g1')!.openTabs).toEqual(['/renamed']);
    expect(getGroup(s, 'g2')!.openTabs).toEqual(['/b', '/renamed']);
    expect(getGroup(s, 'g2')!.activeFilePath).toBe('/renamed');
  });

  it('merges into an existing tab when the new path is already open in a group', () => {
    let s = withFiles('g0', '/old', '/new');
    s = renamePathInGroups(s, '/old', '/new');
    expect(getActiveGroup(s).openTabs).toEqual(['/new']); // merged, not duplicated
  });

  it('is a no-op when old == new', () => {
    const s = withFiles('g0', '/a');
    expect(renamePathInGroups(s, '/a', '/a')).toBe(s);
  });
});

describe('activateGroup', () => {
  it('switches the active group and ignores unknown ids', () => {
    let s = withFiles('g1', '/a', '/b');
    s = splitGroup(s, 'g1', '/b', 'right', 'g2'); // active g2
    s = activateGroup(s, 'g1');
    expect(s.activeGroupId).toBe('g1');
    expect(activateGroup(s, 'nope')).toBe(s);
  });
});

describe('active-group invariant across collapse sequences', () => {
  it('split → close original → close last tab: activeGroupId stays live and openable', () => {
    let s = withFiles('g0', '/a');
    s = duplicateToNewGroup(s, 'g0', '/a', 'right', 'g1'); // /a in both, active g1
    s = closeInGroup(s, 'g0', '/a'); // g0 empties → collapses; only g1 left
    expect(s.groups.map((g) => g.id)).toEqual(['g1']);
    expectActiveGroupLive(s);
    s = closeInGroup(s, 'g1', '/a'); // last group stays, now empty
    expectActiveGroupLive(s);

    // Opening into the (live) active group from an empty editor always works.
    const after = openInGroup(s, s.activeGroupId, '/b');
    expect(after).not.toBe(s);
    expect(getActiveGroup(after).openTabs).toEqual(['/b']);
    expect(getActiveGroup(after).activeFilePath).toBe('/b');
  });

  it('moveTabBetweenGroups collapsing the source keeps activeGroupId live and openable', () => {
    let s = withFiles('g0', '/a');
    s = duplicateToNewGroup(s, 'g0', '/a', 'right', 'g1');
    s = openInGroup(s, 'g1', '/b'); // g0:[/a], g1:[/a,/b]
    s = moveTabBetweenGroups(s, 'g0', 'g1', '/a'); // g0 empties → collapses
    expect(s.groups.map((g) => g.id)).toEqual(['g1']);
    expectActiveGroupLive(s);
    s = closeInGroup(s, 'g1', '/a');
    s = closeInGroup(s, 'g1', '/b');
    expectActiveGroupLive(s);
    expect(getActiveGroup(openInGroup(s, s.activeGroupId, '/c')).activeFilePath).toBe('/c');
  });

  it('closePanelInGroup collapsing a panel-only group keeps activeGroupId live and panel-openable', () => {
    let s = withFiles('g0', '/a');
    s = duplicateToNewGroup(s, 'g0', '/a', 'right', 'g1');
    s = closeInGroup(s, 'g1', '/a'); // g1 collapses → only g0
    s = openPanelInGroup(s, s.activeGroupId, 'settings');
    s = closeInGroup(s, s.activeGroupId, '/a');
    s = closePanelInGroup(s, s.activeGroupId, 'settings'); // last group stays, empty
    expectActiveGroupLive(s);
    const after = openPanelInGroup(s, s.activeGroupId, 'settings');
    expect(after).not.toBe(s);
    expect(getActiveGroup(after).activePanel).toBe('settings');
  });
});

describe('selectors', () => {
  it('groupContaining / isPathOpenAnywhere / allOpenPaths span all groups', () => {
    let s = withFiles('g1', '/a', '/b');
    s = splitGroup(s, 'g1', '/b', 'right', 'g2');
    expect(groupContaining(s, '/a')).toBe('g1');
    expect(groupContaining(s, '/b')).toBe('g2');
    expect(groupContaining(s, '/x')).toBeNull();
    expect(isPathOpenAnywhere(s, '/b')).toBe(true);
    expect(isPathOpenAnywhere(s, '/x')).toBe(false);
    expect(allOpenPaths(s).sort()).toEqual(['/a', '/b']);
  });
});
