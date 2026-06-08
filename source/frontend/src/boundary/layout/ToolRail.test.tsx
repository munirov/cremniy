import { fireEvent, render, screen } from '@testing-library/react';
import { beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';

// ToolRail is a launcher: clicking a rail icon opens that byte tool as a center
// tab via the session's `openPanel`, and the icon of the active center tab is
// marked selected. (Tools used to live in a right-edge dock; now they're
// ordinary center tabs like files and the Git / Connections panels.)
const { openPanel, state } = vi.hoisted(() => ({
  openPanel: vi.fn(),
  state: { activePanel: null as string | null },
}));

vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: () => ({ openPanel, activePanel: state.activePanel }),
}));

import { ToolRail } from '@boundary/layout/ToolRail';
import toolsPlugin from '@plugins/tools';
import { registerPlugin } from '@shared/plugins/registry';

// ToolRail reads the plugin registry; tests don't boot main.tsx, so register the
// Binary Tools plugin so its rail tools are present (idempotent per id).
beforeAll(() => {
  registerPlugin(toolsPlugin);
});

beforeEach(() => {
  openPanel.mockClear();
  state.activePanel = null;
});

describe('ToolRail', () => {
  it('opens a byte tool as a center tab when its rail icon is clicked', () => {
    render(<ToolRail />);
    fireEvent.click(screen.getByRole('tab', { name: /binary \/ hex/i }));
    expect(openPanel).toHaveBeenCalledWith('binary');
  });

  it('marks the rail icon of the active center tab as selected', () => {
    state.activePanel = 'disassembler';
    render(<ToolRail />);
    expect(screen.getByRole('tab', { name: /disassembler/i })).toHaveAttribute(
      'aria-selected',
      'true',
    );
    expect(screen.getByRole('tab', { name: /binary \/ hex/i })).toHaveAttribute(
      'aria-selected',
      'false',
    );
  });
});
