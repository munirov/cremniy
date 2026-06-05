import { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import type { ReactNode } from 'react';

import { listenPopoutClosed, popoutPane, closePopoutPane } from '@infrastructure/tauri/bridge';
import { Menu } from '@boundary/common/Menu';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import { registerPaneRenderer } from './paneRegistry';

import styles from './Pane.module.css';

/**
 * Pane — a generic container for IDE blocks (file tree, editor, terminal, tool dock).
 *
 * No visible chrome / header: the block fills its slot edge-to-edge. The only
 * pane-level affordance is a right-click menu (open-in-separate-window /
 * bring-back) shown when the user right-clicks an EMPTY area of the block —
 * right-clicking actual content (a hex byte, a tree row) lets that content's
 * own menu win (we bail when the event was already `preventDefault`-ed).
 *
 * Every Pane has a stable id. A global PaneState tracks which panes are
 * currently "popped out" into a separate Tauri window.
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

/** One group of menu items; groups render separated by a thin divider. */
export type PaneMenuItem = { label: string; onClick: () => void; danger?: boolean };

export type PaneProps = {
  id: PaneId;
  /** Used for aria-label and as the popped-out window title. */
  title: string;
  /** If true, render only the body (no chrome / menu). Set by the popout route. */
  bare?: boolean;
  onPopOut?: (id: PaneId) => void;
  /**
   * Extra context-menu groups contributed by the pane's content (e.g. the
   * binary panel's Copy / Go-to actions). Each inner array is a group; groups
   * render separated by a divider, with the pane-level window actions appended
   * last in their own group.
   */
  extraMenuGroups?: PaneMenuItem[][];
  /**
   * Optional override for what the popped-out window renders. By default the
   * popped-out window mirrors `children`; supply this when the docked tree
   * depends on contexts that don't exist in the popout window.
   */
  popoutRender?: () => ReactNode;
  children: ReactNode;
};

export function Pane({
  id,
  title,
  bare = false,
  onPopOut,
  extraMenuGroups,
  popoutRender,
  children,
}: PaneProps) {
  const registry = useOptionalPaneRegistry();
  const workspaceRoot = useWorkspaceRoot();
  const poppedOut = registry?.state.get(id)?.poppedOut ?? false;
  const [ctxMenu, setCtxMenu] = useState<{ x: number; y: number } | null>(null);

  // Register a renderer so /popout/:id can render the same children full-screen
  // in the detached window. Only the docked owner registers.
  useEffect(() => {
    if (bare) {
      return;
    }
    const render = popoutRender ?? (() => children);
    return registerPaneRenderer(id, render);
  }, [bare, id, children, popoutRender]);

  const handlePopOut = useCallback(() => {
    onPopOut?.(id);
    registry?.setPoppedOut(id, true);
    void popoutPane(id, workspaceRoot?.path ?? null).catch(() => {
      registry?.setPoppedOut(id, false);
    });
    setCtxMenu(null);
  }, [id, onPopOut, registry, workspaceRoot]);

  const handleBringBack = useCallback(() => {
    void closePopoutPane(id);
    registry?.setPoppedOut(id, false);
    setCtxMenu(null);
  }, [id, registry]);

  if (bare) {
    return <div className={styles.bareBody}>{children}</div>;
  }

  // Build the menu groups: content-contributed groups first, then the
  // pane-level window action in its own group (divider between them).
  const windowGroup: PaneMenuItem[] = poppedOut
    ? [{ label: 'Bring back to main window', onClick: handleBringBack }]
    : [{ label: 'Open in separate window', onClick: handlePopOut }];
  const groups: PaneMenuItem[][] = [...(extraMenuGroups ?? []), windowGroup];

  return (
    <section
      className={styles.paneShell}
      aria-label={title}
      data-pane-id={id}
      onContextMenu={(e) => {
        // Let content menus (hex byte, tree row, disasm line) win — they call
        // preventDefault on their own elements. Only the bare pane surface
        // opens the window menu.
        if (e.defaultPrevented) {
          return;
        }
        e.preventDefault();
        setCtxMenu({ x: e.clientX, y: e.clientY });
      }}
    >
      <div className={styles.paneBody}>
        {poppedOut ? (
          <div className={styles.paneDetachedPlaceholder} role="status">
            <p className={styles.paneDetachedTitle}>In separate window</p>
            <button type="button" className={styles.paneDetachedButton} onClick={handleBringBack}>
              Bring back
            </button>
          </div>
        ) : (
          children
        )}
      </div>
      {ctxMenu != null ? (
        <Menu
          groups={groups}
          position={{ kind: 'point', x: ctxMenu.x, y: ctxMenu.y }}
          onClose={() => setCtxMenu(null)}
          label={`${title} actions`}
        />
      ) : null}
    </section>
  );
}

/**
 * Hook for components that own a pane and need to react to its popped-out state.
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
