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

// branch glyph for the Extensions row / details header (same shape family as
// GIT_RAIL_ICON, drawn larger): trunk + merge arc + two nodes.
const GIT_GLYPH =
  'M6 3v12 M18 9a9 9 0 0 1-9 9 M15 6a3 3 0 1 0 6 0a3 3 0 1 0-6 0 M3 18a3 3 0 1 0 6 0a3 3 0 1 0-6 0';

const GIT_README = `# Git — Source Control

Native source-control for the workspace, built on the \`git\` already on your
PATH. The **Source Control** view in the activity bar shows the working tree;
the **Git** center tab opens the full toolbox.

## Features

- Working-tree status with inline file-tree decorations (added / modified / deleted / untracked / conflict).
- Branches — create, checkout, delete.
- **Merge** and **rebase** onto a chosen branch, with a clear conflict callout when one happens.
- **Stash** — save and restore work in progress.
- **History** — recent commits with hash, subject and author.
- **Remotes** — add, edit and remove.

## How to use

1. Open the **Source Control** view from the left activity bar to see status at a glance.
2. For branches, merge, rebase, stash, history and remotes, open the **Git** tab — run the
   \`dialog.openAdvancedGit\` command, or open it from the panel.

> No repository, or \`git\` not installed? The view shows a calm empty state instead of an error.

Git ships **bundled** with Cremniy and is always on.
`;

const git: PluginManifest = {
  id: 'git',
  name: 'Git',
  description: 'Source control — status, branches, merge, rebase, stash, history, remotes.',
  delivery: 'bundled',
  icon: GIT_GLYPH,
  version: '0.1.0',
  author: 'Cremniy',
  categories: ['Source Control'],
  links: [
    {
      label: 'Documentation',
      url: 'https://github.com/munirov/cremniy/blob/HEAD/documentation/architecture/PLUGINS.md',
    },
    { label: 'Repository', url: 'https://github.com/munirov/cremniy' },
  ],
  readme: GIT_README,
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
