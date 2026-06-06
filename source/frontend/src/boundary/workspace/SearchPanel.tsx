import { useEffect, useState } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';

import { WorkspaceFileTree } from './WorkspaceFileTree';
import { SearchIcon } from './activityBarIcons';

import styles from './SearchPanel.module.css';

/**
 * Search view — a query box over the workspace that drives the file tree's
 * filter (folders open to reveal matches). Frontend filename match for now;
 * content search can register as its own view later.
 */
export function SearchPanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const [query, setQuery] = useState('');
  const [debounced, setDebounced] = useState('');

  // Debounce so each keystroke doesn't re-expand the whole tree.
  useEffect(() => {
    const timer = setTimeout(() => setDebounced(query), 200);
    return () => clearTimeout(timer);
  }, [query]);

  return (
    <div className={styles.panel}>
      <div className={styles.searchBox}>
        <span className={styles.searchIcon}>
          <SearchIcon />
        </span>
        <input
          className={styles.searchInput}
          type="text"
          placeholder="Search files by name"
          aria-label="Search files by name"
          value={query}
          autoFocus
          onChange={(e) => setQuery(e.target.value)}
        />
        {query !== '' ? (
          <button type="button" className={styles.clearBtn} aria-label="Clear search" onClick={() => setQuery('')}>
            ×
          </button>
        ) : null}
      </div>
      <div className={styles.results}>
        <WorkspaceFileTree workspaceRoot={workspaceRoot} filter={debounced} />
      </div>
    </div>
  );
}
