import { useEffect, useMemo, useState } from 'react';
import type { ReactNode } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import { pluginViews } from '@shared/plugins/registry';
import { useRegistryVersion } from '@shared/plugins/useRegistry';

import { ExtensionsPanel } from '@boundary/extensions/ExtensionsPanel';

import { WorkspaceFileTree } from './WorkspaceFileTree';
import { SearchPanel } from './SearchPanel';
import { ExplorerIcon, SearchIcon, ExtensionsIcon, ChevronDownIcon } from './activityBarIcons';
import { ViewsMenu } from './ViewsMenu';

import styles from './SidePanel.module.css';

/**
 * Draws a plugin view's `railIconPath` (a single 24×24 stroke `d`) the same way
 * the core glyphs render — so a contributed view (e.g. Git) sits in the activity
 * bar indistinguishably from Explorer / Search.
 */
function PluginViewIcon({ path, size = 17 }: { path: string; size?: number }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.7}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
      style={{ flexShrink: 0 }}
    >
      <path d={path} />
    </svg>
  );
}

type ViewEntry = { id: string; label: string; icon: ReactNode; render?: () => ReactNode };

/**
 * The side-panel views. Explorer + Search are CORE; everything else (Source
 * Control, and future docker / RE packs) arrives through plugin `views`
 * contributions — see documentation/architecture/PLUGINS.md. A plugin view
 * carries its own `render()`; core views render a hardcoded body branch below.
 * Computed per render so a freshly-loaded plugin shows up without extra wiring.
 */
function buildViews(): ViewEntry[] {
  const core: ViewEntry[] = [
    { id: 'explorer', label: 'Explorer', icon: <ExplorerIcon size={17} /> },
    { id: 'search', label: 'Search', icon: <SearchIcon size={17} /> },
  ];
  const plugin: ViewEntry[] = pluginViews().map((v) => ({
    id: v.id,
    label: v.label,
    icon: <PluginViewIcon path={v.railIconPath} />,
    render: v.render,
  }));
  // Extensions (the plugin manager) sits last, like VS Code — a core view.
  const extensions: ViewEntry = {
    id: 'extensions',
    label: 'Extensions',
    icon: <ExtensionsIcon size={17} />,
    render: () => <ExtensionsPanel />,
  };
  return [...core, ...plugin, extensions];
}

// Persist the UNPINNED ids (the inverse of "pinned"): a view shows in the
// activity bar unless the user explicitly hides it from the Views menu. Storing
// the inverse means a newly-introduced view — Extensions added in an update, or a
// freshly-enabled plugin — is shown by DEFAULT, instead of vanishing because it
// wasn't part of an older saved "pinned" set.
const UNPIN_STORAGE_KEY = 'cremniy.unpinnedViews';

function loadUnpinned(): Set<string> {
  try {
    const raw = localStorage.getItem(UNPIN_STORAGE_KEY);
    if (raw != null) {
      const arr: unknown = JSON.parse(raw);
      if (Array.isArray(arr)) {
        return new Set(arr.filter((x): x is string => typeof x === 'string'));
      }
    }
  } catch {
    // ignore — default: nothing hidden
  }
  return new Set();
}

export function SidePanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  // Rebuild views whenever the active plugin set changes (Extensions toggle), so
  // a plugin's side-panel view appears / disappears live.
  const registryVersion = useRegistryVersion();
  const views = useMemo(() => buildViews(), [registryVersion]);
  const [active, setActive] = useState<string>('explorer');
  const [unpinned, setUnpinned] = useState<Set<string>>(loadUnpinned);
  const [menuAnchor, setMenuAnchor] = useState<HTMLElement | null>(null);

  useEffect(() => {
    try {
      localStorage.setItem(UNPIN_STORAGE_KEY, JSON.stringify([...unpinned]));
    } catch {
      // ignore
    }
  }, [unpinned]);

  // If the active view's plugin was just disabled, fall back to Explorer.
  useEffect(() => {
    if (!views.some((v) => v.id === active)) setActive('explorer');
  }, [views, active]);

  const togglePin = (id: string) =>
    setUnpinned((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id); // currently hidden → show it
      else next.add(id); // currently shown → hide it
      return next;
    });

  // The activity bar shows pinned views PLUS the active one — so opening an
  // unpinned view from the Views menu still surfaces its icon (highlighted) while
  // it's open, instead of leaving the bar with no indication. It drops back out
  // of the bar once you switch away (VS Code behaviour).
  const pinnedViews = views.filter((v) => !unpinned.has(v.id) || v.id === active);
  const activeView = views.find((v) => v.id === active);

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
          ) : active === 'search' ? (
            <SearchPanel workspaceRoot={workspaceRoot} />
          ) : activeView?.render != null ? (
            activeView.render()
          ) : null}
        </div>
        {menuAnchor != null ? (
          <ViewsMenu
            anchor={menuAnchor}
            rows={views.map((v) => ({
              id: v.id,
              label: v.label,
              icon: v.icon,
              pinned: !unpinned.has(v.id),
              active: active === v.id,
            }))}
            onSelect={(id) => {
              setActive(id);
              setMenuAnchor(null);
            }}
            onTogglePin={(id) => togglePin(id)}
            onClose={() => setMenuAnchor(null)}
          />
        ) : null}
      </div>
    </div>
  );
}
