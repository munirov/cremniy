import type { ReactNode } from 'react';

/**
 * Plugin contribution model. A plugin is a folder under the top-level
 * `plugins/<id>/` whose index default-exports a {@link PluginManifest}. It adds
 * functionality to the IDE only through these declarative contributions — it
 * never edits core source to wire a menu item or panel in. The host collects
 * the contributions (see registry.ts) and decides where they appear, so adding
 * a plugin is dropping a folder. Docs: documentation/architecture/PLUGINS.md.
 *
 * Local-only for now: plugins ship in this repo's `plugins/` folder and are
 * bundled at build time. Remote/server-delivered plugins are a later step.
 */

/** A center-tab panel a plugin adds (rendered like Settings / Advanced Git). */
export type CenterPanelContribution = {
  id: string;
  label: string;
  render: () => ReactNode;
};

/**
 * A tool a plugin adds to the right-edge ToolRail (and the tool dock it opens).
 * The rail draws `railIconPath` as a 24×24 stroke icon; selecting the tool shows
 * `render()` in the dock. `id` is the dock's active-tool key (also the agent
 * `tool.select` id), so it must be unique across all plugins.
 */
export type ToolTabContribution = {
  id: string;
  label: string;
  railIconPath: string;
  render: () => ReactNode;
};

/** Which host menu a contributed item is placed under. */
export type MenuId = 'terminal' | 'tools';

/** A menu item a plugin adds to one of the host's top-bar menus. */
export type MenuItemContribution = {
  menu: MenuId;
  /** Stable id, namespaced by plugin (e.g. "connections.open"). */
  id: string;
  label: string;
  run: () => void;
};

/** An agent / MCP command a plugin exposes through window.cremniy. */
export type CommandContribution = {
  name: string;
  description: string;
  run: (args: Record<string, unknown>) => unknown;
};

/** Everything a plugin contributes to the IDE. */
export type PluginManifest = {
  /** Stable unique id (also the folder name under plugins/). */
  id: string;
  /** Human-readable name (shown in a future plugins manager). */
  name: string;
  centerPanels?: CenterPanelContribution[];
  toolTabs?: ToolTabContribution[];
  menuItems?: MenuItemContribution[];
  commands?: CommandContribution[];
};
