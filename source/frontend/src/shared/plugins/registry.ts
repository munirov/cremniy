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
 * In-memory registry of loaded plugins. Populated once at startup by
 * loadPlugins() (from the top-level plugins/ folder); the host's menu bar,
 * center-panel host, and agent bridge read the merged contributions from here.
 */
const registered: PluginManifest[] = [];

/** Register a plugin's contributions. Idempotent per id (re-registering an id
 *  replaces it) so StrictMode double-invoke / HMR never duplicates entries. */
export function registerPlugin(manifest: PluginManifest): void {
  const i = registered.findIndex((p) => p.id === manifest.id);
  if (i === -1) registered.push(manifest);
  else registered[i] = manifest;
}

/** All registered plugins (for a future plugins-manager view). */
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
