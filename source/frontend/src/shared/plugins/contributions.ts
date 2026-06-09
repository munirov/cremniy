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

/**
 * A view a plugin adds to the left-edge activity bar / side panel (alongside the
 * core Explorer + Search). The bar draws `railIconPath` as a 24×24 stroke icon;
 * selecting the view shows `render()` in the panel body. `id` is the active-view
 * key (and the pinned-views storage key), so it must be unique across all
 * plugins. `render` takes no args — a view needing the workspace root reads it
 * via useWorkspaceRoot() in a small wrapper component.
 */
export type ViewContribution = {
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

/** An external link shown in a plugin's details page (Resources block). */
export type PluginLink = {
  /** Row label (e.g. "Documentation", "Repository"). */
  label: string;
  /** Target URL — opened externally. */
  url: string;
};

/**
 * How a plugin reaches the user. `bundled` ships with the IDE and is always on
 * (e.g. Git). `recommended` is first-party/official but not part of the base
 * install — it's enabled through the Extensions panel so the base package stays
 * small (e.g. Connections). Defaults to `recommended` when omitted. (Third-party
 * marketplace delivery is a later step — see PLUGINS.md.)
 */
export type PluginDelivery = 'bundled' | 'recommended';

/** Everything a plugin contributes to the IDE. */
export type PluginManifest = {
  /** Stable unique id (also the folder name under plugins/). */
  id: string;
  /** Human-readable name (shown in the Extensions panel). */
  name: string;
  /** One-line summary for the Extensions panel. */
  description?: string;
  /** Delivery model — see {@link PluginDelivery}. Defaults to `recommended`. */
  delivery?: PluginDelivery;

  // ── Presentation metadata (Extensions panel + details page) ──────────────
  // Optional, additive: a plugin without these still lists/works; supplying
  // them makes its Extensions row and details center-tab richer.
  /**
   * Plugin glyph — a single 24×24 stroke SVG path `d` (same shape as a view's
   * `railIconPath`), drawn `stroke="currentColor" fill="none"` at the card size
   * and large in the details header. Keep it Lucide-style: thin, no fill.
   */
  icon?: string;
  /** Semantic version string shown on the row / details (e.g. "0.1.0"). */
  version?: string;
  /** Publisher / author name shown muted next to the plugin name. */
  author?: string;
  /** Free-form category tags rendered as chips on the details page. */
  categories?: string[];
  /** External resource links (docs / repository) listed on the details page. */
  links?: PluginLink[];
  /**
   * Long description in Markdown — the body of the plugin's details page,
   * rendered by the `@cremniy/markdown-view` package (full CommonMark + GFM).
   * This is where a contributor describes what the plugin does and how to use
   * it. Falls back to `description` when absent.
   */
  readme?: string;

  centerPanels?: CenterPanelContribution[];
  toolTabs?: ToolTabContribution[];
  /** Side-panel views (activity bar) this plugin adds — e.g. Source Control. */
  views?: ViewContribution[];
  menuItems?: MenuItemContribution[];
  commands?: CommandContribution[];
};
