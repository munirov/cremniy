import type { ReactNode } from 'react';

import type { PaneId } from './Pane';

/**
 * Frontend registry mapping a stable pane id to the React node it renders in
 * its popped-out window. The same id is used in the popout URL (`/popout/:id`)
 * and on the Tauri side (`popout-<id>` window label).
 *
 * Pane components register their rendered node here at mount time; the
 * `/popout/:id` route reads from this registry to render the same node
 * full-screen in the detached window.
 */

type PaneRenderer = () => ReactNode;

const registry = new Map<PaneId, PaneRenderer>();
const listeners = new Set<() => void>();

export function registerPaneRenderer(id: PaneId, render: PaneRenderer): () => void {
  registry.set(id, render);
  notifyListeners();
  return () => {
    if (registry.get(id) === render) {
      registry.delete(id);
      notifyListeners();
    }
  };
}

export function getPaneRenderer(id: PaneId): PaneRenderer | null {
  return registry.get(id) ?? null;
}

export function subscribePaneRegistry(listener: () => void): () => void {
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
}

function notifyListeners(): void {
  for (const listener of listeners) {
    listener();
  }
}
