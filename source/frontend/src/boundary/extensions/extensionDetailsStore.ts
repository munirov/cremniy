/**
 * Which plugin the Extension Details center-tab is showing.
 *
 * Center panels open by id with no arguments (see IdeSession.openPanel /
 * resolveCenterPanel), so the single `extensionDetails` panel is parameterised
 * out-of-band: the Extensions panel sets the selected plugin id here just before
 * calling openPanel('extensionDetails'), and the details panel reads it.
 *
 * It is reactive (subscribe). Re-opening the details tab for a DIFFERENT plugin
 * while it's already the active tab is a no-op at the IdeSession level (same
 * panel id → no state change → no re-render), so the details panel must observe
 * this store directly to re-render when the chosen plugin changes — otherwise the
 * tab would keep showing the previously selected plugin.
 */
let selectedId: string | null = null;
const listeners = new Set<() => void>();

/** The plugin id the details panel should show, or null if none chosen yet. */
export function getSelectedExtension(): string | null {
  return selectedId;
}

/** Point the details panel at a plugin id (call before opening the panel). */
export function setSelectedExtension(id: string): void {
  if (id === selectedId) return;
  selectedId = id;
  for (const l of listeners) l();
}

/** Subscribe to selection changes (for useSyncExternalStore). Returns unsubscribe. */
export function subscribeSelectedExtension(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}
