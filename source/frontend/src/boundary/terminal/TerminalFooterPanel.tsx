import { useCallback, useEffect, useRef, useState } from 'react';

import {
  emptyCremniyMeta,
  parseCremniyMeta,
  stringifyCremniyMeta,
  type CremniyMeta,
} from '@domain/project/cremniyMeta';
import { readCremniyMeta, writeCremniyMeta } from '@infrastructure/tauri/bridge';
import { registerAgentCommands } from '@shared/agent/agentBridge';
import { subscribeOpenConnection } from '@shared/connections/connectionBus';

import { TerminalInstance } from './TerminalInstance';
import { SerialInstance } from './SerialInstance';
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

type SerialTabConn = {
  /** Profile id — used to de-dup tabs (re-opening a host focuses its tab). */
  connId: string;
  /** Stable serial session id for this tab's SerialInstance. */
  sessionId: string;
  port: string;
  baud: number;
};

type TerminalTab = {
  id: number;
  /** Live shell name reported by the instance (e.g. "powershell"); null while
      starting or after exit. Drives the tab label when no custom name is set. */
  shell: string | null;
  /** User-given name (double-click the tab to rename); null = auto label. */
  title: string | null;
  /** Set when this tab is a serial connection rather than a local shell. Such
      tabs render a {@link SerialInstance} and are ephemeral (not persisted). */
  conn?: SerialTabConn | null;
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

  // Per-tab pure scrollback readers (tab id → read fn), reported up by each live
  // TerminalInstance. Backs the `terminal.read` / `terminal.list` agent commands.
  // Refs (not state): the registration effect runs once, so `run` reads these
  // live instead of closing over a stale snapshot.
  const readersRef = useRef<Map<number, () => string>>(new Map());
  const tabsRef = useRef<TerminalTab[]>(tabs);
  tabsRef.current = tabs;
  const activeIdRef = useRef(activeId);
  activeIdRef.current = activeId;

  const handleReader = useCallback((id: number, read: (() => string) | null) => {
    if (read == null) readersRef.current.delete(id);
    else readersRef.current.set(id, read);
  }, []);

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

    // Serial connection tabs are ephemeral — only the local shell tabs persist,
    // so reopening the workspace doesn't silently re-open serial ports.
    const shellTabs = tabs.filter((t) => t.conn == null);
    const activeIndex = Math.max(0, shellTabs.findIndex((t) => t.id === activeId));
    const names = shellTabs.map((t) => t.title);
    const count = Math.max(1, shellTabs.length);
    // Keep .cremniy minimal — only write `names` once the user has named a tab.
    const hasNames = names.some((n) => n != null && n !== '');
    const candidate = hasNames
      ? { count, activeIndex, names }
      : { count, activeIndex };
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

  // Open (or focus) a tab for a saved host, published by the Hosts manager over
  // the connection bus. De-dup by profile id via tabsRef so a re-open focuses
  // the live tab instead of stacking duplicates. id + sessionId are computed
  // outside the state updater (StrictMode-safe — no double-consumed ids).
  const openConnectionTab = useCallback(
    (req: { connId: string; label: string; serial?: { port: string; baud: number } }) => {
      onExpand?.();
      const existing = tabsRef.current.find((t) => t.conn?.connId === req.connId);
      if (existing != null) {
        setActiveId(existing.id);
        return;
      }
      const serial = req.serial;
      if (serial == null) return; // only serial transport is wired today
      const id = nextIdRef.current;
      nextIdRef.current += 1;
      const sessionId = crypto.randomUUID();
      setTabs((prev) => [
        ...prev,
        {
          id,
          shell: null,
          title: req.label,
          conn: { connId: req.connId, sessionId, port: serial.port, baud: serial.baud },
        },
      ]);
      setActiveId(id);
    },
    [onExpand],
  );

  useEffect(() => subscribeOpenConnection(openConnectionTab), [openConnectionTab]);

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

  // Type-1 (read-only) agent commands: hand an agent the full terminal
  // scrollback as text — the whole buffer, not just what's on screen. Pure data
  // return; they never scroll/resize/focus the terminal. Registered once; `run`
  // reads tabs/active/readers through refs. Docs: AGENT_CONTROL.md.
  useEffect(() => {
    const labelFor = (tab: TerminalTab, i: number): string =>
      tab.title ?? shellLabel(tab.shell) ?? `Terminal ${i + 1}`;

    return registerAgentCommands([
      {
        name: 'terminal.read',
        description:
          'Read the full terminal scrollback as text { lines? } (no UI change). Reads the active terminal; `lines` keeps only the last N lines. Use terminal.list for other tabs.',
        run: (args) => {
          const tabs = tabsRef.current;
          if (tabs.length === 0) throw new Error('No terminal is open.');
          const activeIdNow = activeIdRef.current;
          const index = Math.max(0, tabs.findIndex((t) => t.id === activeIdNow));
          const tab = tabs[index]!;
          const read = readersRef.current.get(tab.id);
          if (read == null) {
            throw new Error('The active terminal is not ready yet (no buffer).');
          }
          let text = read();
          const total = text === '' ? 0 : text.split('\n').length;
          const n = typeof args.lines === 'number' ? Math.floor(args.lines) : null;
          if (n != null && n > 0) {
            text = text.split('\n').slice(-n).join('\n');
          }
          return {
            sessionIndex: index,
            label: labelFor(tab, index),
            terminalCount: tabs.length,
            lineCount: total,
            text,
          };
        },
      },
      {
        name: 'terminal.list',
        description:
          'List open terminals { } — index, label, whether active and whether its buffer is readable (no UI change).',
        run: () => {
          const tabs = tabsRef.current;
          const activeIdNow = activeIdRef.current;
          return tabs.map((tab, i) => ({
            index: i,
            label: labelFor(tab, i),
            shell: tab.shell,
            active: tab.id === activeIdNow,
            readable: readersRef.current.has(tab.id),
          }));
        },
      },
    ]);
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
              {collapsed ? (
                // Expand — chevron up out of the strip.
                <svg aria-hidden width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                  <path d="M6 15l6-6 6 6" />
                </svg>
              ) : (
                // Minimise — same underscore bar as the window's minimize control.
                <svg aria-hidden width="14" height="14" viewBox="0 0 12 12">
                  <rect x="2" y="6" width="8" height="1" fill="currentColor" />
                </svg>
              )}
            </button>
            <button
              type="button"
              className={`${styles.addBtn} ${styles.panelActionDanger}`}
              onClick={closePanel}
              title="Close terminal (starts fresh next time)"
              aria-label="Close terminal"
            >
              {/* Cross — same as the window's close control. */}
              <svg aria-hidden width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round">
                <path d="M3 3l8 8M11 3l-8 8" />
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
            {tab.conn != null ? (
              <SerialInstance
                sessionId={tab.conn.sessionId}
                port={tab.conn.port}
                baud={tab.conn.baud}
                active={tab.id === activeId}
                collapsed={collapsed}
              />
            ) : (
              <TerminalInstance
                workspaceRoot={workspaceRoot}
                active={tab.id === activeId}
                collapsed={collapsed}
                onShellChange={(shell) => handleShell(tab.id, shell)}
                onReaderChange={(read) => handleReader(tab.id, read)}
              />
            )}
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
