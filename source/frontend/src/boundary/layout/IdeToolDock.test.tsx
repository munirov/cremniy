import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

vi.mock('@boundary/tools/BinaryToolPanel', () => ({
  BinaryToolPanel: () => <h2>Binary / hex</h2>,
}));

import { IdeToolDock } from '@boundary/layout/IdeToolDock';
import { ToolDockProvider, useToolDock } from '@boundary/workspace/ToolDockContext';

function Probe() {
  const { activeToolTab } = useToolDock();
  return <span data-testid="active-tab">{activeToolTab ?? 'none'}</span>;
}

describe('IdeToolDock', () => {
  it('selects binary from rail and shows stub title', () => {
    render(
      <ToolDockProvider>
        <Probe />
        <IdeToolDock />
      </ToolDockProvider>,
    );

    fireEvent.click(screen.getByRole('tab', { name: /binary \/ hex/i }));
    expect(screen.getByTestId('active-tab')).toHaveTextContent('binary');
    expect(screen.getByRole('heading', { name: /binary \/ hex/i })).toBeInTheDocument();
  });

  it('toggles off when same rail tab clicked twice', () => {
    render(
      <ToolDockProvider>
        <Probe />
        <IdeToolDock />
      </ToolDockProvider>,
    );

    const bin = screen.getByRole('tab', { name: /binary \/ hex/i });
    fireEvent.click(bin);
    fireEvent.click(bin);
    expect(screen.getByTestId('active-tab')).toHaveTextContent('none');
  });
});
