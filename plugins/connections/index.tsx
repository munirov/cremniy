import type { PluginManifest } from '@shared/plugins/contributions';
import { pluginHost } from '@shared/plugins/host';

import { ConnectionsPanel } from './ConnectionsPanel';
import { SftpPanel } from './SftpPanel';

const PANEL_ID = 'connections';
const SFTP_PANEL_ID = 'sftp';

/**
 * Connections (Hosts) — the first real Cremniy plugin. It manages saved serial /
 * SSH hosts and adds, purely through contributions:
 *   - a center panel (the hosts manager UI),
 *   - a 2nd center panel (SFTP — dual-pane file transfer over SSH),
 *   - Terminal-menu items to open each,
 *   - agent commands (dialog.openConnections / dialog.openSftp).
 * Nothing here is wired into core source; the host places each contribution. The
 * serial / SSH session rendering lives in the core terminal dock (it consumes
 * shared/connections/connectionBus), which this panel drives via "Connect"; SFTP
 * talks to the sftp_* backend (source/backend/src/sftp.rs) directly.
 */
// plug / server glyph for the Extensions row: a stacked server rack with a
// status LED + a plug pin reaching in.
const CONNECTIONS_GLYPH =
  'M4 4h13a2 2 0 0 1 2 2v3a2 2 0 0 1-2 2H4zM4 13h13a2 2 0 0 1 2 2v3a2 2 0 0 1-2 2H4z M7 7.5h.01 M7 16.5h.01 M22 7.5h-3 M22 16.5h-3';

const CONNECTIONS_README = `# Connections (Hosts)

Saved **serial** and **SSH** hosts, plus the terminals and file transfer that go
with them. The first real Cremniy plugin — it manages your connection list and
opens sessions in the IDE's terminal dock.

## Features

- A **Connections (Hosts)** manager — add, edit and remove saved serial / SSH hosts.
- One-click **Connect** — opens a serial or SSH terminal session in the dock.
- **SFTP** — a dual-pane file browser for transferring files over SSH.

## How to use

1. Open **Terminal → Connections (Hosts)…** (or run \`dialog.openConnections\`) to manage hosts.
2. Pick a host and **Connect** — the session opens in the terminal dock.
3. For file transfer, open **Terminal → SFTP…** (or run \`dialog.openSftp\`).

> Connections is a **recommended** add-on, not part of the base install. Enable it from the
> **Extensions** panel (Install), then reload.
`;

const connections: PluginManifest = {
  id: 'connections',
  name: 'Connections',
  description: 'Saved serial / SSH hosts, serial & SSH terminals, and SFTP file transfer.',
  icon: CONNECTIONS_GLYPH,
  version: '0.1.0',
  author: 'Cremniy',
  categories: ['Terminal', 'Remote'],
  links: [
    {
      label: 'Documentation',
      url: 'https://github.com/munirov/cremniy/blob/HEAD/documentation/architecture/PLUGINS.md',
    },
    { label: 'Repository', url: 'https://github.com/munirov/cremniy' },
  ],
  readme: CONNECTIONS_README,
  centerPanels: [
    { id: PANEL_ID, label: 'Connections', render: () => <ConnectionsPanel /> },
    { id: SFTP_PANEL_ID, label: 'SFTP', render: () => <SftpPanel /> },
  ],
  menuItems: [
    {
      menu: 'terminal',
      id: 'connections.open',
      label: 'Connections (Hosts)…',
      run: () => pluginHost().openPanel(PANEL_ID),
    },
    {
      menu: 'terminal',
      id: 'connections.sftp',
      label: 'SFTP…',
      run: () => pluginHost().openPanel(SFTP_PANEL_ID),
    },
  ],
  commands: [
    {
      name: 'dialog.openConnections',
      description:
        'Open the Connections (Hosts) panel — saved serial / SSH hosts — as a center tab.',
      run: () => pluginHost().openPanel(PANEL_ID),
    },
    {
      name: 'dialog.openSftp',
      description: 'Open the SFTP panel — dual-pane file transfer over SSH — as a center tab.',
      run: () => pluginHost().openPanel(SFTP_PANEL_ID),
    },
  ],
};

export default connections;
