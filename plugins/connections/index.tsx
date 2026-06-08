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
const connections: PluginManifest = {
  id: 'connections',
  name: 'Connections',
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
