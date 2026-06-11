import { createContext, useContext, useMemo, useState, type ReactNode } from 'react';

/**
 * Tracks whether an editor tab is currently being dragged, so the per-group
 * edge drop-zones (GroupEditorPane) only mount during a drag — they're inert
 * (and invisible) the rest of the time.
 *
 * The flag is set on a tab's `dragStart` and cleared on `dragEnd` (and on drop).
 * It's deliberately just a boolean: the drag *payload* (path + source group)
 * rides on the native `dataTransfer`, read in the drop handlers. This context
 * only answers "is a drag in flight?" so the overlay can appear.
 */
export type TabDragContextValue = {
  /** True between a tab's dragStart and its dragEnd / drop. */
  isDraggingTab: boolean;
  /** Mark a tab drag as started (tab strip calls this on dragStart). */
  beginTabDrag: () => void;
  /** Mark the drag as finished (tab strip dragEnd, or a drop-zone on drop). */
  endTabDrag: () => void;
};

const TabDragContext = createContext<TabDragContextValue | null>(null);

export function TabDragProvider({ children }: { children: ReactNode }) {
  const [isDraggingTab, setIsDraggingTab] = useState(false);
  const value = useMemo<TabDragContextValue>(
    () => ({
      isDraggingTab,
      beginTabDrag: () => setIsDraggingTab(true),
      endTabDrag: () => setIsDraggingTab(false),
    }),
    [isDraggingTab],
  );
  return <TabDragContext.Provider value={value}>{children}</TabDragContext.Provider>;
}

/**
 * Read the tab-drag flag. Returns a safe inert value when used outside a
 * provider (e.g. a tab strip rendered in isolation in a unit test): the drag
 * still reorders within its group, there are simply no edge drop-zones.
 */
export function useTabDrag(): TabDragContextValue {
  return useContext(TabDragContext) ?? INERT;
}

const INERT: TabDragContextValue = {
  isDraggingTab: false,
  beginTabDrag: () => undefined,
  endTabDrag: () => undefined,
};
