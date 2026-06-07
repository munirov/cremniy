/**
 * Cross-panel channel for "open this saved host as a live session". The Hosts
 * manager (a center panel) publishes a request; the terminal dock (a bottom
 * panel) subscribes and spawns / focuses a tab for it. A tiny module-level bus
 * keeps the two panels decoupled — neither imports the other, and there is no
 * payload to thread through the RootApp → IdeDockview → panel prop chain (which
 * only carries the counter-style `newTerminalSignal`).
 */

/** A request to open a saved connection as a live terminal tab. */
export type OpenConnectionRequest = {
  /** Profile id — the terminal dock de-dups tabs by this (re-open = focus). */
  connId: string;
  /** Tab label (the host's friendly name). */
  label: string;
  /** Serial transport params (set for a serial host). */
  serial?: { port: string; baud: number };
  /** SSH transport params (set for an SSH host). */
  ssh?: { address: string; port: number; username: string; password?: string | null };
};

type Listener = (req: OpenConnectionRequest) => void;

const listeners = new Set<Listener>();

/** Ask the terminal dock to open (or focus) a tab for this saved host. */
export function openConnection(req: OpenConnectionRequest): void {
  for (const listener of listeners) {
    listener(req);
  }
}

/** Subscribe to open-connection requests; returns an unsubscribe fn. */
export function subscribeOpenConnection(listener: Listener): () => void {
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
}
