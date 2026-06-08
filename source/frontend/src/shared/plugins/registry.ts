import type {
  CenterPanelContribution,
  CommandContribution,
  MenuId,
  MenuItemContribution,
  PluginManifest,
  ToolTabContribution,
  ViewContribution,
} from './contributions';

/**
 * In-memory registry of ACTIVE plugins — the ones currently contributing to the
 * IDE. Seeded at startup by loadPlugins() (skipping user-disabled ones); the
 * host's side panel, tool rail, menu bar, center-panel host, and agent bridge
 * read the merged contributions from here.
 *
 * It is reactive: enabling/disabling a plugin from the Extensions panel
 * register/unregisters it and bumps a version, so every consumer that subscribes
 * (via useRegistryVersion) re-renders and the plugin's UI appears / disappears
 * live — no reload. Persistence of the on/off choice lives in pluginState.
 */
const registered: PluginManifest[] = [];

let version = 0;
const listeners = new Set<() => void>();

function notify(): void {
  version += 1;
  for (const l of listeners) l();
}

/** Subscribe to registry changes (active-set add/remove). Returns unsubscribe. */
export function subscribeRegistry(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

/** Monotonic version — bumps on every active-set change (for useSyncExternalStore). */
export function registryVersion(): number {
  return version;
}

/** Register a plugin's contributions (mark it active). Idempotent per id
 *  (re-registering an id replaces it) so StrictMode double-invoke / HMR never
 *  duplicates entries. Notifies subscribers. */
export function registerPlugin(manifest: PluginManifest): void {
  const i = registered.findIndex((p) => p.id === manifest.id);
  if (i === -1) registered.push(manifest);
  else registered[i] = manifest;
  notify();
}

/** Remove a plugin from the active set (its contributions vanish). Notifies. */
export function unregisterPlugin(id: string): void {
  const i = registered.findIndex((p) => p.id === id);
  if (i !== -1) {
    registered.splice(i, 1);
    notify();
  }
}

/** The currently-active plugins. */
export function listPlugins(): readonly PluginManifest[] {
  return registered;
}

export function pluginCenterPanels(): CenterPanelContribution[] {
  return registered.flatMap((p) => p.centerPanels ?? []);
}

export function pluginToolTabs(): ToolTabContribution[] {
  return registered.flatMap((p) => p.toolTabs ?? []);
}

export function pluginViews(): ViewContribution[] {
  return registered.flatMap((p) => p.views ?? []);
}

export function pluginMenuItems(menu: MenuId): MenuItemContribution[] {
  return registered.flatMap((p) => p.menuItems ?? []).filter((m) => m.menu === menu);
}

export function pluginCommands(): CommandContribution[] {
  return registered.flatMap((p) => p.commands ?? []);
}
