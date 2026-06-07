/**
 * Host capabilities exposed to plugin contributions at runtime. A plugin's
 * menu-item / command `run()` reaches the live IDE through this singleton —
 * RootApp installs it once after mount, so it's always present by the time any
 * handler fires (handlers only run on user action, long after startup).
 *
 * Kept deliberately small: plugins get capabilities, not the IDE's internals.
 */
export type PluginHost = {
  /** Open a registered center panel by id (a plugin's own or a core one). */
  openPanel: (id: string) => void;
  /** Close a center panel by id. */
  closePanel: (id: string) => void;
};

let host: PluginHost | null = null;

/** Install the host API (RootApp, once it has the live session callbacks). */
export function setPluginHost(api: PluginHost): void {
  host = api;
}

/** Access the host API from a contribution handler. Throws if used before the
 *  IDE installed it (i.e. before any UI exists) — that would be a wiring bug. */
export function pluginHost(): PluginHost {
  if (host == null) {
    throw new Error('Plugin host API used before it was installed.');
  }
  return host;
}
