import { useSyncExternalStore } from 'react';

import { registryVersion, subscribeRegistry } from './registry';

/**
 * Re-render the calling component whenever the active plugin set changes
 * (a plugin enabled / disabled from the Extensions panel). Consumers that draw
 * plugin contributions — the side panel, tool rail, menu bar, center-panel host,
 * agent commands — call this so the UI updates live, without a reload.
 */
export function useRegistryVersion(): number {
  return useSyncExternalStore(subscribeRegistry, registryVersion, registryVersion);
}
