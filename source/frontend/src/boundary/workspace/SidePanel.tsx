import { useState } from 'react';
import type { ReactNode } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import { Menu } from '@boundary/common/Menu';

import { WorkspaceFileTree } from './WorkspaceFileTree';
import { SearchPanel } from './SearchPanel';
import { ExplorerIcon, SearchIcon, ChevronDownIcon } from './activityBarIcons';

import styles from './SidePanel.module.css';

type ViewId = 'explorer' | 'search';

/**
 * The registry of side-panel views. Today: Explorer + Search. This is the seam
 * the future plugin views (git, docker, the RE tools) register into — add an
 * entry here and a branch in renderView, and the activity bar grows a button.
 */
const VIEWS: Array<{ id: ViewId; label: string; icon: ReactNode }> = [
  { id: 'explorer', label: 'Explorer', icon: <ExplorerIcon /> },
  { id: 'search', label: 'Search', icon: <SearchIcon /> },
];

export function SidePanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const [active, setActive] = useState<ViewId>('explorer');
  const [menuAnchor, setMenuAnchor] = useState<HTMLElement | null>(null);

  return (
    <div className={styles.sidePanel}>
      <div className={styles.activityBar} role="tablist" aria-label="Side panel views">
        {VIEWS.map((view) => (
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
        <span className={styles.spacer} />
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
        {menuAnchor != null ? (
          <Menu
            groups={[VIEWS.map((view) => ({ label: view.label, onClick: () => setActive(view.id) }))]}
            position={{ kind: 'anchor', el: menuAnchor }}
            onClose={() => setMenuAnchor(null)}
            label="Views"
          />
        ) : null}
      </div>
      <div className={styles.viewBody}>
        {active === 'explorer' ? (
          <WorkspaceFileTree workspaceRoot={workspaceRoot} />
        ) : (
          <SearchPanel workspaceRoot={workspaceRoot} />
        )}
      </div>
    </div>
  );
}
