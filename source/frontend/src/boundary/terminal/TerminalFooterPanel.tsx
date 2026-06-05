import { useCallback, useEffect, useRef, useState } from 'react';

import {
  emptyCremniyMeta,
  parseCremniyMeta,
  stringifyCremniyMeta,
  type CremniyMeta,
} from '@domain/project/cremniyMeta';
import { readCremniyMeta, writeCremniyMeta } from '@infrastructure/tauri/bridge';

import { TerminalInstance } from './TerminalInstance';
import styles from './TerminalFooterPanel.module.css';

export type TerminalFooterPanelProps = {
  workspaceRoot: string | null;
  /** Incrementing counter — each bump (Terminal → New Terminal) spawns a tab. */
  newTerminalSignal?: number;
  /** When true the panel shows only its tab strip (minimised). Sessions stay
      alive; clicking a tab or the expand chevron restores it. */
  collapsed?: boolean;
  /** Hide button — minimise to the tab strip. */
  onCollapse?: () => void;
  /** Restore from the minimised strip (a tab click / the expand chevron). */
  onExpand?: () => void;
  /** Close button — fully remove the panel (resets the saved layout first). */
  onClose?: () => void;
};

type TerminalTab = {
  id: number;
  /** Live shell name reported by the instance (e.g. "powershell"); null while
      starting or after exit. Drives the tab label when no custom name is set. */
  shell: string | null;
  /** User-given name (double-click the tab to rename); null = auto label. */
  title: string | null;
};

/** Friendly tab label from a shell path: "…\powershell.exe" → "powershell". */
function shellLabel(shell: string | null): string | null {
  if (shell == null || shell.trim() === '') return null;
  const base = shell.split(/[\\/]/).pop() ?? shell;
  return base.replace(/\.exe$/i, '');
}

/** Last path segment, used as the fallback project name for a fresh .cremniy. */
function basename(path: string): string {
  const parts = path.split(/[\\/]/).filter((s) => s !== '');
  return parts.length > 0 ? parts[parts.length - 1] : path;
}

function TerminalIcon() {
  return (
    <svg
      aria-hidden
      width="13"
      height="13"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M5 8l3.5 3.5L5 15" />
      <path d="M12 16h6" />
    </svg>
  );
}

/**
 * Terminal dock — a tab bar over N live {@link TerminalInstance}s. Each tab
 * owns its own xterm + PTY session; inactive tabs stay mounted (hidden) so
 * their session and scrollback survive switching. "+" spawns another.
 */
export function TerminalFooterPanel({
  workspaceRoot,
  newTerminalSignal = 0,
  collapsed = false,
  onCollapse,
  onExpand,
  onClose,
}: TerminalFooterPanelProps) {
  const [tabs, setTabs] = useState<TerminalTab[]>([]);
  const [activeId, setActiveId] = useState<number>(0);
  // Gates rendering until .cremniy has been read, so we don't start a throwaway
  // session before knowing how many tabs to restore.
  const [loaded, setLoaded] = useState(false);
  const nextIdRef = useRef(1);
  // Last known full project meta — we read-modify-write it so persisting the
  // terminal layout never clobbers other session fields.
  const metaRef = useRef<CremniyMeta | null>(null);
  // Inline rename state: which tab's label is being edited + its draft text.
  const [editingId, setEditingId] = useState<number | null>(null);
  const [draft, setDraft] = useState('');
  // When a rename ends via Enter/Escape we tear the input down ourselves; this
  // flag tells the resulting blur not to commit a second (or unwanted) time.
  const skipBlurRef = useRef(false);

  // Restore tab layout from .cremniy when the workspace opens / changes.
  useEffect(() => {
    const root = workspaceRoot?.trim() ?? '';
    let cancelled = false;
    setLoaded(false);

    if (root === '') {
      // No workspace — one tab that shows its own "open a folder" prompt.
      metaRef.current = null;
      setTabs([{ id: 1, shell: null, title: null }]);
      setActiveId(1);
      nextIdRef.current = 2;
      setLoaded(true);
      return;
    }

    void (async () => {
      let meta: CremniyMeta;
      try {
        meta = parseCremniyMeta(await readCremniyMeta(root), basename(root));
      } catch {
        meta = emptyCremniyMeta(basename(root));
      }
      if (cancelled) return;
      metaRef.current = meta;
      const persisted = meta.session.terminals;
      const count = Math.max(1, persisted?.count ?? 1);
      const names = persisted?.names ?? [];
      const restored: TerminalTab[] = Array.from({ length: count }, (_, i) => ({
        id: i + 1,
        shell: null,
        title: names[i] ?? null,
      }));
      const activeIndex = Math.min(Math.max(0, persisted?.activeIndex ?? 0), count - 1);
      nextIdRef.current = count + 1;
      setTabs(restored);
      setActiveId(restored[activeIndex].id);
      setLoaded(true);
    })();

    return () => {
      cancelled = true;
    };
  }, [workspaceRoot]);

  // Persist tab count + active index + custom names back to .cremniy (debounced)
  // whenever they change. Shell-label updates also fire this, but the guard
  // skips no-op writes.
  useEffect(() => {
    const root = workspaceRoot?.trim() ?? '';
    const meta = metaRef.current;
    if (!loaded || root === '' || meta == null) return;

    const activeIndex = Math.max(0, tabs.findIndex((t) => t.id === activeId));
    const names = tabs.map((t) => t.title);
    // Keep .cremniy minimal — only write `names` once the user has named a tab.
    const hasNames = names.some((n) => n != null && n !== '');
    const candidate = hasNames
      ? { count: tabs.length, activeIndex, names }
      : { count: tabs.length, activeIndex };
    const prev = meta.session.terminals;
    if (prev != null && JSON.stringify(prev) === JSON.stringify(candidate)) {
      return;
    }
    const next: CremniyMeta = {
      ...meta,
      session: { ...meta.session, terminals: candidate },
    };
    metaRef.current = next;

    const handle = window.setTimeout(() => {
      void writeCremniyMeta(root, stringifyCremniyMeta(next)).catch(() => undefined);
    }, 300);
    return () => window.clearTimeout(handle);
  }, [tabs, activeId, loaded, workspaceRoot]);

  const addTerminal = useCallback(() => {
    const id = nextIdRef.current;
    nextIdRef.current += 1;
    setTabs((prev) => [...prev, { id, shell: null, title: null }]);
    setActiveId(id);
  }, []);

  const startRename = useCallback((id: number, current: string) => {
    skipBlurRef.current = false;
    setEditingId(id);
    setDraft(current);
  }, []);

  const commitRename = useCallback((id: number, value: string) => {
    const name = value.trim();
    setTabs((prev) =>
      prev.map((t) => (t.id === id ? { ...t, title: name === '' ? null : name } : t)),
    );
    setEditingId(null);
  }, []);

  const cancelRename = useCallback(() => {
    skipBlurRef.current = true;
    setEditingId(null);
  }, []);

  // Spawn a tab when Terminal → New Terminal bumps the signal. A bump that
  // arrives while the panel is hidden/loading is absorbed (revealing just shows
  // the restored tabs); only bumps while the panel is open add a fresh tab.
  const appliedSignalRef = useRef(newTerminalSignal);
  useEffect(() => {
    if (!loaded) {
      appliedSignalRef.current = newTerminalSignal;
      return;
    }
    if (newTerminalSignal === appliedSignalRef.current) return;
    appliedSignalRef.current = newTerminalSignal;
    onExpand?.();
    addTerminal();
  }, [newTerminalSignal, loaded, addTerminal, onExpand]);

  // "Close" = reset the saved layout to one fresh terminal, then fully remove
  // the panel — so reopening starts clean. "Hide" only minimises (onCollapse).
  const closePanel = useCallback(() => {
    const root = workspaceRoot?.trim() ?? '';
    const meta = metaRef.current;
    if (root !== '' && meta != null) {
      const next: CremniyMeta = {
        ...meta,
        session: { ...meta.session, terminals: { count: 1, activeIndex: 0 } },
      };
      metaRef.current = next;
      void writeCremniyMeta(root, stringifyCremniyMeta(next)).catch(() => undefined);
    }
    onClose?.();
  }, [workspaceRoot, onClose]);

  const closeTerminal = useCallback((id: number) => {
    setTabs((prev) => {
      const index = prev.findIndex((t) => t.id === id);
      if (index === -1) return prev;
      const next = prev.filter((t) => t.id !== id);
      // If we closed the active tab, activate a neighbour (prefer the one to
      // the left, like editor tabs).
      setActiveId((current) => {
        if (current !== id) return current;
        if (next.length === 0) return current;
        const fallback = next[Math.max(0, index - 1)];
        return fallback.id;
      });
      return next;
    });
  }, []);

  const handleShell = useCallback((id: number, shell: string | null) => {
    setTabs((prev) => prev.map((t) => (t.id === id ? { ...t, shell } : t)));
  }, []);

  return (
    <section
      className={styles.terminalPanel}
      aria-label="Terminal"
      data-testid="terminal-panel"
    >
      <div className={styles.tabBar}>
        <div className={styles.tabList} role="tablist" aria-label="Open terminals">
          {tabs.map((tab, i) => {
            const isActive = tab.id === activeId;
            const label = tab.title ?? shellLabel(tab.shell) ?? `Terminal ${i + 1}`;
            const isEditing = editingId === tab.id;
            return (
              <div
                key={tab.id}
                role="tab"
                aria-selected={isActive}
                tabIndex={0}
                className={`${styles.tab} ${isActive ? styles.tabActive : ''}`}
                onClick={() => {
                  if (collapsed) onExpand?.();
                  setActiveId(tab.id);
                }}
                onKeyDown={(e) => {
                  if (isEditing) return;
                  if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    if (collapsed) onExpand?.();
                    setActiveId(tab.id);
                  }
                }}
                title={isEditing ? undefined : `${label} — double-click to rename`}
              >
                <span className={styles.tabIcon}>
                  <TerminalIcon />
                </span>
                {isEditing ? (
                  <input
                    className={styles.tabRename}
                    value={draft}
                    autoFocus
                    spellCheck={false}
                    style={{ width: `${Math.min(Math.max(draft.length + 1, 4), 18)}ch` }}
                    onChange={(e) => setDraft(e.target.value)}
                    onFocus={(e) => e.currentTarget.select()}
                    onClick={(e) => e.stopPropagation()}
                    onKeyDown={(e) => {
                      e.stopPropagation();
                      if (e.key === 'Enter') {
                        skipBlurRef.current = true;
                        commitRename(tab.id, draft);
                      } else if (e.key === 'Escape') {
                        cancelRename();
                      }
                    }}
                    onBlur={() => {
                      if (skipBlurRef.current) {
                        skipBlurRef.current = false;
                        return;
                      }
                      commitRename(tab.id, draft);
                    }}
                  />
                ) : (
                  <span
                    className={styles.tabLabel}
                    onDoubleClick={(e) => {
                      e.stopPropagation();
                      startRename(tab.id, label);
                    }}
                  >
                    {label}
                  </span>
                )}
                {isEditing ? null : (
                  <button
                    type="button"
                    className={styles.tabClose}
                    onClick={(e) => {
                      e.stopPropagation();
                      closeTerminal(tab.id);
                    }}
                    title="Close terminal"
                    aria-label={`Close ${label}`}
                  >
                    <svg aria-hidden width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round">
                      <path d="M6 6l12 12M18 6l-12 12" />
                    </svg>
                  </button>
                )}
              </div>
            );
          })}
        </div>
        <button
          type="button"
          className={styles.addBtn}
          onClick={() => {
            onExpand?.();
            addTerminal();
          }}
          title="New terminal"
          aria-label="New terminal"
        >
          <svg aria-hidden width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round">
            <path d="M12 5v14M5 12h14" />
          </svg>
        </button>

        {onCollapse != null ? (
          <div className={styles.panelActions}>
            <button
              type="button"
              className={styles.addBtn}
              onClick={() => (collapsed ? onExpand?.() : onCollapse())}
              title={collapsed ? 'Expand terminal' : 'Hide terminal (minimise to tab strip)'}
              aria-label={collapsed ? 'Expand terminal' : 'Hide terminal'}
            >
              <svg aria-hidden width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                <path d={collapsed ? 'M6 15l6-6 6 6' : 'M6 9l6 6 6-6'} />
              </svg>
            </button>
            <button
              type="button"
              className={`${styles.addBtn} ${styles.panelActionDanger}`}
              onClick={closePanel}
              title="Close terminal (starts fresh next time)"
              aria-label="Close terminal"
            >
              <svg aria-hidden width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
                <path d="M4 7h16M9 7V5a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2M6 7l1 13a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1l1-13" />
              </svg>
            </button>
          </div>
        ) : null}
      </div>

      <div
        className={styles.instances}
        style={collapsed ? { display: 'none' } : undefined}
      >
        {tabs.map((tab) => (
          <div
            key={tab.id}
            className={styles.instanceWrap}
            style={{ display: tab.id === activeId ? 'flex' : 'none' }}
          >
            <TerminalInstance
              workspaceRoot={workspaceRoot}
              active={tab.id === activeId}
              onShellChange={(shell) => handleShell(tab.id, shell)}
            />
          </div>
        ))}
        {loaded && tabs.length === 0 ? (
          <div className={styles.emptyState}>
            <button type="button" className={styles.emptyStateBtn} onClick={addTerminal}>
              New terminal
            </button>
          </div>
        ) : null}
      </div>
    </section>
  );
}
