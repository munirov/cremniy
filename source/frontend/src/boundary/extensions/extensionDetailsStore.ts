/**
 * Which plugin the Extension Details center-tab is showing.
 *
 * Center panels open by id with no arguments (see IdeSession.openPanel /
 * resolveCenterPanel), so the single `extensionDetails` panel is parameterised
 * out-of-band: the Extensions panel sets the selected plugin id here just before
 * calling openPanel('extensionDetails'), and the details panel reads it on
 * render. A plain module variable is enough — there is one details tab and it
 * always shows the most recently chosen plugin (re-opening it for another plugin
 * just re-points this id, exactly like VS Code reusing its Extension tab).
 */
let selectedId: string | null = null;

/** The plugin id the details panel should show, or null if none chosen yet. */
export function getSelectedExtension(): string | null {
  return selectedId;
}

/** Point the details panel at a plugin id (call before opening the panel). */
export function setSelectedExtension(id: string): void {
  selectedId = id;
}
