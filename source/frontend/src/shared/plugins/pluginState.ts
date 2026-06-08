/**
 * Which plugins the user has turned off. Only `recommended` plugins can be
 * disabled — `bundled` ones are part of the IDE and always load. Persisted in
 * localStorage; read by loadPlugins() at startup and by the Extensions panel.
 *
 * Toggling takes effect on the next load (the Extensions panel offers a reload),
 * since contributions are read throughout the render tree and re-running
 * loadPlugins() cleanly is simpler than hot-swapping the registry.
 */
const STORAGE_KEY = 'cremniy.disabledPlugins';

function read(): Set<string> {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw == null) return new Set();
    const arr: unknown = JSON.parse(raw);
    return Array.isArray(arr) ? new Set(arr.filter((x): x is string => typeof x === 'string')) : new Set();
  } catch {
    return new Set();
  }
}

function write(ids: Set<string>): void {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify([...ids]));
  } catch {
    // best effort
  }
}

/** The set of disabled plugin ids. */
export function disabledPluginIds(): Set<string> {
  return read();
}

/** True if a plugin id is currently disabled by the user. */
export function isPluginDisabled(id: string): boolean {
  return read().has(id);
}

/** Enable or disable a plugin by id (persisted). Reload to apply. */
export function setPluginEnabled(id: string, enabled: boolean): void {
  const ids = read();
  if (enabled) ids.delete(id);
  else ids.add(id);
  write(ids);
}
