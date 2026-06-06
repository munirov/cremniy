import { useEffect, useState } from 'react';
import type { ReactNode } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';

import { WorkspaceFileTree } from './WorkspaceFileTree';
import { SearchPanel } from './SearchPanel';
import { ExplorerIcon, SearchIcon, ChevronDownIcon } from './activityBarIcons';
import { ViewsMenu } from './ViewsMenu';

import styles from './SidePanel.module.css';

type ViewId = 'explorer' | 'search';

/**
 * The registry of side-panel views. Today: Explorer + Search. This is the seam
 * the future plugin views (git, docker, the RE tools) register into — add an
 * entry here and a branch in the body, and it shows up in the activity bar /
 * chevron menu with a pin toggle.
 */
const VIEWS: Array<{ id: ViewId; label: string; icon: ReactNode }> = [
  { id: 'explorer', label: 'Explorer', icon: <ExplorerIcon size={17} /> },
  { id: 'search', label: 'Search', icon: <SearchIcon size={17} /> },
];

const PIN_STORAGE_KEY = 'cremniy.pinnedViews';

function loadPinned(): Set<ViewId> {
  try {
    const raw = localStorage.getItem(PIN_STORAGE_KEY);
    if (raw != null) {
      const arr: unknown = JSON.parse(raw);
      if (Array.isArray(arr)) {
        return new Set(arr.filter((x): x is ViewId => VIEWS.some((v) => v.id === x)));
      }
    }
  } catch {
    // ignore — fall back to all pinned
  }
  return new Set(VIEWS.map((v) => v.id));
}

export function SidePanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const [active, setActive] = useState<ViewId>('explorer');
  const [pinned, setPinned] = useState<Set<ViewId>>(loadPinned);
  const [menuAnchor, setMenuAnchor] = useState<HTMLElement | null>(null);

  useEffect(() => {
    try {
      localStorage.setItem(PIN_STORAGE_KEY, JSON.stringify([...pinned]));
    } catch {
      // ignore
    }
  }, [pinned]);

  const togglePin = (id: ViewId) =>
    setPinned((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });

  const pinnedViews = VIEWS.filter((v) => pinned.has(v.id));

  return (
    <div className={styles.sidePanel}>
      <div className={styles.activityBar} role="tablist" aria-label="Side panel views">
        {pinnedViews.map((view) => (
          <button
            key={view.id}
            type="button"
            role="tab"
            aria-selected={active === view.id}
            className={`${styles.activityBtn} ${active === view.id ? styles.activityBtnActive : ''}`}
            title={view.label}
            onClick={() => setActive(view.id)}
          >
            {view.icon}
          </button>
        ))}
        <button
          type="button"
          className={styles.activityBtn}
          title="Views"
          aria-haspopup="menu"
          onClick={(e) => {
            const el = e.currentTarget;
            setMenuAnchor((prev) => (prev ? null : el));
          }}
        >
          <ChevronDownIcon />
        </button>
      </div>
      <div className={styles.body}>
        <div className={styles.viewBody}>
          {active === 'explorer' ? (
            <WorkspaceFileTree workspaceRoot={workspaceRoot} />
          ) : (
            <SearchPanel workspaceRoot={workspaceRoot} />
          )}
        </div>
        {menuAnchor != null ? (
          <ViewsMenu
            anchor={menuAnchor}
            rows={VIEWS.map((v) => ({
              id: v.id,
              label: v.label,
              icon: v.icon,
              pinned: pinned.has(v.id),
              active: active === v.id,
            }))}
            onSelect={(id) => {
              setActive(id as ViewId);
              setMenuAnchor(null);
            }}
            onTogglePin={(id) => togglePin(id as ViewId)}
            onClose={() => setMenuAnchor(null)}
          />
        ) : null}
      </div>
    </div>
  );
}
