import { PLUGINS } from '@plugins/index';

import { registerPlugin } from './registry';
import { isPluginDisabled } from './pluginState';

/**
 * Discover and register every plugin shipped in the top-level `plugins/` folder.
 * Called once at startup (main.tsx) before React mounts, so contributions are in
 * place when the UI first renders.
 *
 * Today the set is the static list exported by plugins/index.ts (bundled at
 * build time). The seam is deliberately this one function: swapping it for a
 * runtime/remote loader later changes nothing in the rest of the app.
 */
export function loadPlugins(): void {
  for (const plugin of PLUGINS) {
    // Bundled plugins always load; a recommended plugin loads unless the user
    // turned it off in the Extensions panel.
    const bundled = (plugin.delivery ?? 'recommended') === 'bundled';
    if (!bundled && isPluginDisabled(plugin.id)) continue;
    registerPlugin(plugin);
  }
}
