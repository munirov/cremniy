import type { PluginManifest } from '@shared/plugins/contributions';
import { pluginHost } from '@shared/plugins/host';

import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

import { GitPanel } from './GitPanel';
import { AdvancedGitDialog } from './AdvancedGitDialog';

const VIEW_ID = 'git';
const PANEL_ID = 'advancedGit';

// git-branch glyph (Source Control), as a single 24×24 stroke path so it draws
// like every other rail icon: the trunk + the merge arc + two nodes (circles
// expressed as paired arcs, since railIconPath is one `d` string).
const GIT_RAIL_ICON =
  'M6 3v12 M18 9a9 9 0 0 1-9 9 M15 6a3 3 0 1 0 6 0a3 3 0 1 0-6 0 M3 18a3 3 0 1 0 6 0a3 3 0 1 0-6 0';

/**
 * Source Control — the Git "pack". It contributes, purely through declarative
 * contributions (nothing wired into core source):
 *   - a side-panel `view` (the Source Control activity-bar entry + its body),
 *   - a center panel (the Advanced Git tab: branches / merge / rebase / stash /
 *     history / remotes),
 *   - an agent command (dialog.openAdvancedGit) that opens that panel.
 *
 * The wrapper components below adapt the args-less `render()` contribution shape
 * to the panels' props: GitPanel needs the workspace root; AdvancedGitTab needs
 * the root + a closePanel for its Close button. Both read the live IDE via the
 * boundary contexts, exactly as the old in-core wrappers did.
 *
 * Stays in CORE for now (a decoration-contribution model is a follow-up): the
 * file-tree git dots (gitDecorations) and the backend git bridge — GitPanel /
 * AdvancedGit consume @infrastructure/tauri/bridge git* from here.
 */
function GitView() {
  const workspaceRoot = useWorkspaceRoot();
  return <GitPanel workspaceRoot={workspaceRoot} />;
}

function AdvancedGitTab() {
  const workspaceRoot = useWorkspaceRoot();
  const { closePanel } = useIdeSession();
  return (
    <AdvancedGitDialog
      embedded
      workspaceRoot={workspaceRoot?.path ?? null}
      onClose={() => closePanel(PANEL_ID)}
    />
  );
}

const git: PluginManifest = {
  id: 'git',
  name: 'Git',
  views: [
    {
      id: VIEW_ID,
      label: 'Source Control',
      railIconPath: GIT_RAIL_ICON,
      render: () => <GitView />,
    },
  ],
  centerPanels: [{ id: PANEL_ID, label: 'Git', render: () => <AdvancedGitTab /> }],
  commands: [
    {
      name: 'dialog.openAdvancedGit',
      description:
        'Open the Advanced Git panel (branches, merge, rebase, stash, history, remotes) as a center tab.',
      run: () => pluginHost().openPanel(PANEL_ID),
    },
  ],
};

export default git;
