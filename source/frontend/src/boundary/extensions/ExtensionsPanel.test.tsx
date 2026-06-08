import { fireEvent, render, screen, within } from '@testing-library/react';
import { beforeEach, describe, expect, it } from 'vitest';

import { loadPlugins } from '@shared/plugins/loadPlugins';

import { ExtensionsPanel } from './ExtensionsPanel';

describe('ExtensionsPanel', () => {
  beforeEach(() => {
    localStorage.clear();
    // Seed the registry like the app does at startup, so the live toggle's
    // unregister actually fires (and notifies) under test.
    loadPlugins();
  });

  it('lists every plugin with delivery badges; all are toggleable', () => {
    render(<ExtensionsPanel />);
    expect(screen.getByText('Extensions')).toBeInTheDocument();
    expect(screen.getByText('Binary Tools')).toBeInTheDocument();
    expect(screen.getByText('Git')).toBeInTheDocument();
    expect(screen.getByText('Connections')).toBeInTheDocument();
    // Git + Binary Tools are bundled; Connections is recommended.
    expect(screen.getAllByText('Bundled').length).toBeGreaterThanOrEqual(2);
    expect(screen.getByText('Recommended')).toBeInTheDocument();
    // Every plugin can be disabled (bundled ones too) → 3 Disable buttons.
    expect(screen.getAllByRole('button', { name: /^disable$/i })).toHaveLength(3);
  });

  it('disabling a plugin moves it to the Disabled section live (Enable appears)', () => {
    render(<ExtensionsPanel />);
    // Find the Connections row and disable it.
    const connRow = screen.getByText('Connections').closest('[role="button"]') as HTMLElement;
    fireEvent.click(within(connRow).getByRole('button', { name: /^disable$/i }));
    // It moved to a "Disabled — 1" section and now offers Enable.
    expect(screen.getByText(/^Disabled — 1$/)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /^enable$/i })).toBeInTheDocument();
    expect(screen.getByText(/^Enabled — 2$/)).toBeInTheDocument();
  });
});
