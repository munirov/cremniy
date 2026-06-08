import { fireEvent, render, screen } from '@testing-library/react';
import { beforeAll, describe, expect, it, vi } from 'vitest';

// The rail tools are a plugin contribution now; stub the binary panel so this
// layout test stays about rail→dock wiring, not the panel's internals.
vi.mock('@plugins/tools/BinaryToolPanel', () => ({
  BinaryToolPanel: () => <h2>Binary / hex</h2>,
}));

// IdeToolDock pulls the active file off the session and the workspace root for
// its breadcrumb. Neither is exercised here (no file open), so stub both
// contexts to the empty state — same pattern the other layout/tool tests use.
vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: () => ({ activeFilePath: null }),
}));

vi.mock('@boundary/workspace/WorkspaceContext', () => ({
  useWorkspaceRoot: () => null,
}));

import { IdeToolDock } from '@boundary/layout/IdeToolDock';
import { ToolRail } from '@boundary/layout/ToolRail';
import { ToolDockProvider, useToolDock } from '@boundary/workspace/ToolDockContext';
import toolsPlugin from '@plugins/tools';
import { registerPlugin } from '@shared/plugins/registry';

// ToolRail / IdeToolDock read the plugin registry; the app populates it at
// startup (loadPlugins). Tests don't boot main.tsx, so register the Binary
// Tools plugin here so its rail tools are present (registerPlugin is idempotent
// per id).
beforeAll(() => {
  registerPlugin(toolsPlugin);
});

function Probe() {
  const { activeToolTab } = useToolDock();
  return <span data-testid="active-tab">{activeToolTab ?? 'none'}</span>;
}

// The clickable rail buttons live in ToolRail; IdeToolDock only renders the
// panel once a tab is active. Render both under one provider so a rail click
// drives the dock.
function renderRailWithDock() {
  return render(
    <ToolDockProvider>
      <Probe />
      <ToolRail />
      <IdeToolDock />
    </ToolDockProvider>,
  );
}

describe('IdeToolDock', () => {
  it('selects binary from rail and shows stub title', () => {
    renderRailWithDock();

    fireEvent.click(screen.getByRole('tab', { name: /binary \/ hex/i }));
    expect(screen.getByTestId('active-tab')).toHaveTextContent('binary');
    expect(screen.getByRole('heading', { name: /binary \/ hex/i })).toBeInTheDocument();
  });

  it('toggles off when same rail tab clicked twice', () => {
    renderRailWithDock();

    const bin = screen.getByRole('tab', { name: /binary \/ hex/i });
    fireEvent.click(bin);
    expect(screen.getByTestId('active-tab')).toHaveTextContent('binary');

    // Clicking the already-active rail tab toggles the dock closed:
    // selectToolTab clears activeToolTab, so the panel unmounts.
    fireEvent.click(bin);
    expect(screen.getByTestId('active-tab')).toHaveTextContent('none');
    expect(screen.queryByRole('heading', { name: /binary \/ hex/i })).not.toBeInTheDocument();
  });
});
