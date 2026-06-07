import type { PluginManifest } from '@shared/plugins/contributions';
import { pluginHost } from '@shared/plugins/host';

import { ConnectionsPanel } from './ConnectionsPanel';

const PANEL_ID = 'connections';

/**
 * Connections (Hosts) — the first real Cremniy plugin. It manages saved serial /
 * SSH hosts and adds, purely through contributions:
 *   - a center panel (the hosts manager UI),
 *   - a Terminal-menu item to open it,
 *   - an agent command (dialog.openConnections).
 * Nothing here is wired into core source; the host places each contribution. The
 * serial / SSH session rendering lives in the core terminal dock (it consumes
 * shared/connections/connectionBus), which this panel drives via "Connect".
 */
const connections: PluginManifest = {
  id: 'connections',
  name: 'Connections',
  centerPanels: [{ id: PANEL_ID, label: 'Connections', render: () => <ConnectionsPanel /> }],
  menuItems: [
    {
      menu: 'terminal',
      id: 'connections.open',
      label: 'Connections (Hosts)…',
      run: () => pluginHost().openPanel(PANEL_ID),
    },
  ],
  commands: [
    {
      name: 'dialog.openConnections',
      description:
        'Open the Connections (Hosts) panel — saved serial / SSH hosts — as a center tab.',
      run: () => pluginHost().openPanel(PANEL_ID),
    },
  ],
};

export default connections;
