import { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import type { ReactNode } from 'react';

import { listenPopoutClosed, popoutPane, closePopoutPane } from '@infrastructure/tauri/bridge';

import { registerPaneRenderer } from './paneRegistry';

import styles from './Pane.module.css';

/**
 * Pane — a generic container for IDE blocks (file tree, editor, terminal, tool dock).
 *
 * It owns a small chrome (title bar + pop-out button) around an arbitrary child.
 * Every Pane has a stable id. A global PaneState tracks which panes are currently
 * "popped out" into a separate Tauri window — when a pane is popped out, the
 * docked slot renders a placeholder instead of the real children, and the same
 * children are rendered full-screen in the popped-out window (via `/popout/:id`).
 *
 * The Tauri side (window creation) is wired up in a follow-up step; this file
 * only owns the React surface.
 */

export type PaneId = string;

type PaneRuntimeState = {
  poppedOut: boolean;
};

type PaneRegistryValue = {
  state: Map<PaneId, PaneRuntimeState>;
  setPoppedOut: (id: PaneId, poppedOut: boolean) => void;
};

const PaneRegistryContext = createContext<PaneRegistryValue | null>(null);

export function PaneRegistryProvider({ children }: { children: ReactNode }) {
  const [version, setVersion] = useState(0);
  const stateRef = useMemo(() => new Map<PaneId, PaneRuntimeState>(), []);

  const setPoppedOut = useCallback(
    (id: PaneId, poppedOut: boolean) => {
      const prev = stateRef.get(id);
      if (prev?.poppedOut === poppedOut) {
        return;
      }
      stateRef.set(id, { poppedOut });
      setVersion((v) => v + 1);
    },
    [stateRef],
  );

  // When the user closes a popped-out window via the OS title bar, the Rust
  // side emits `pane:popout-closed`. Flip the pane back to docked so the main
  // window stops showing the placeholder.
  useEffect(() => {
    let unlisten: (() => void) | null = null;
    let cancelled = false;
    void listenPopoutClosed((paneId) => {
      setPoppedOut(paneId, false);
    }).then((dispose) => {
      if (cancelled) {
        dispose();
        return;
      }
      unlisten = dispose;
    });
    return () => {
      cancelled = true;
      unlisten?.();
    };
  }, [setPoppedOut]);

  const value = useMemo<PaneRegistryValue>(
    () => ({ state: stateRef, setPoppedOut }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [stateRef, setPoppedOut, version],
  );

  return <PaneRegistryContext.Provider value={value}>{children}</PaneRegistryContext.Provider>;
}

function useOptionalPaneRegistry(): PaneRegistryValue | null {
  return useContext(PaneRegistryContext);
}

export type PaneProps = {
  id: PaneId;
  title: string;
  /** Extra header controls placed next to the pop-out button. */
  actions?: ReactNode;
  /** If true, render only the body (no chrome). Set by the popped-out route. */
  bare?: boolean;
  /** Called when user clicks the pop-out button. Wired to Tauri in a later step. */
  onPopOut?: (id: PaneId) => void;
  /**
   * Optional override for what the popped-out window renders. By default the
   * popped-out window mirrors `children`; supply this when the docked tree
   * depends on contexts that don't exist in the popout window (e.g. the
   * editor uses IdeSessionContext) and you want a different surface there.
   */
  popoutRender?: () => ReactNode;
  children: ReactNode;
};

export function Pane({
  id,
  title,
  actions,
  bare = false,
  onPopOut,
  popoutRender,
  children,
}: PaneProps) {
  const registry = useOptionalPaneRegistry();
  const poppedOut = registry?.state.get(id)?.poppedOut ?? false;
  const [ctxMenu, setCtxMenu] = useState<{ x: number; y: number } | null>(null);

  // Register a renderer so /popout/:id can render the same children full-screen
  // in the detached window. Only the docked owner registers (bare mounts inside
  // the popout window — they consume, they don't publish).
  useEffect(() => {
    if (bare) {
      return;
    }
    const render = popoutRender ?? (() => children);
    return registerPaneRenderer(id, render);
  }, [bare, id, children, popoutRender]);

  // Close the context menu on outside click or Escape.
  useEffect(() => {
    if (ctxMenu == null) {
      return;
    }
    const onDown = (ev: PointerEvent) => {
      const target = ev.target as HTMLElement | null;
      if (target == null || !target.closest('[data-pane-ctxmenu]')) {
        setCtxMenu(null);
      }
    };
    const onKey = (ev: KeyboardEvent) => {
      if (ev.key === 'Escape') {
        setCtxMenu(null);
      }
    };
    window.addEventListener('pointerdown', onDown);
    window.addEventListener('keydown', onKey);
    return () => {
      window.removeEventListener('pointerdown', onDown);
      window.removeEventListener('keydown', onKey);
    };
  }, [ctxMenu]);

  const handlePopOut = useCallback(() => {
    onPopOut?.(id);
    registry?.setPoppedOut(id, true);
    void popoutPane(id).catch(() => {
      registry?.setPoppedOut(id, false);
    });
    setCtxMenu(null);
  }, [id, onPopOut, registry]);

  const handleBringBack = useCallback(() => {
    void closePopoutPane(id);
    registry?.setPoppedOut(id, false);
    setCtxMenu(null);
  }, [id, registry]);

  if (bare) {
    return <div className={styles.bareBody}>{children}</div>;
  }

  return (
    <section className={styles.paneShell} aria-label={title} data-pane-id={id}>
      <header
        className={styles.paneHeader}
        onContextMenu={(e) => {
          e.preventDefault();
          setCtxMenu({ x: e.clientX, y: e.clientY });
        }}
      >
        <span className={styles.paneTitle}>{title}</span>
        {actions != null ? <span className={styles.paneActions}>{actions}</span> : null}
      </header>
      <div className={styles.paneBody}>
        {poppedOut ? (
          <div className={styles.paneDetachedPlaceholder} role="status">
            <p className={styles.paneDetachedTitle}>In separate window</p>
            <button
              type="button"
              className={styles.paneDetachedButton}
              onClick={handleBringBack}
            >
              Bring back
            </button>
          </div>
        ) : (
          children
        )}
      </div>
      {ctxMenu != null ? (
        <ul
          data-pane-ctxmenu
          className={styles.paneCtxMenu}
          style={{ left: ctxMenu.x, top: ctxMenu.y }}
          role="menu"
        >
          {poppedOut ? (
            <li role="none">
              <button
                type="button"
                role="menuitem"
                className={styles.paneCtxMenuItem}
                onClick={handleBringBack}
              >
                Bring back to main window
              </button>
            </li>
          ) : (
            <li role="none">
              <button
                type="button"
                role="menuitem"
                className={styles.paneCtxMenuItem}
                onClick={handlePopOut}
              >
                Open in separate window
              </button>
            </li>
          )}
        </ul>
      ) : null}
    </section>
  );
}

/**
 * Hook for components that own a pane and need to react to its popped-out state
 * (e.g. to skip an expensive subscription while popped out).
 */
export function usePaneIsPoppedOut(id: PaneId): boolean {
  const registry = useOptionalPaneRegistry();
  const [poppedOut, setPoppedOut] = useState(registry?.state.get(id)?.poppedOut ?? false);
  useEffect(() => {
    if (registry == null) {
      return;
    }
    const tick = () => setPoppedOut(registry.state.get(id)?.poppedOut ?? false);
    tick();
  }, [id, registry]);
  return poppedOut;
}

export function setPaneRegistryPoppedOut(
  registry: PaneRegistryValue | null,
  id: PaneId,
  poppedOut: boolean,
): void {
  registry?.setPoppedOut(id, poppedOut);
}

export function usePaneRegistry(): PaneRegistryValue | null {
  return useContext(PaneRegistryContext);
}
