import { PLUGINS } from '@plugins/index';

import { registerPlugin, unregisterPlugin } from './registry';
import { setPluginEnabled } from './pluginState';

/**
 * Turn a plugin on/off LIVE. Persists the choice (pluginState) and adds/removes
 * it from the active registry, so its contributions — side-panel view, tool-rail
 * tools, center panels, menu items, agent commands — appear or disappear
 * immediately in every subscribed consumer. No reload.
 *
 * Any plugin can be toggled (bundled ones included — the badge only says where it
 * came from). Uninstall / download-from-server is a separate, later capability.
 */
export function setPluginActive(id: string, active: boolean): void {
  setPluginEnabled(id, active);
  if (active) {
    const manifest = PLUGINS.find((p) => p.id === id);
    if (manifest != null) registerPlugin(manifest);
  } else {
    unregisterPlugin(id);
  }
}
