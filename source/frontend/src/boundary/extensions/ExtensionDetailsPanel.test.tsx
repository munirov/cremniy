import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';

import { ExtensionDetailsPanel } from './ExtensionDetailsPanel';
import { setSelectedExtension } from './extensionDetailsStore';

describe('ExtensionDetailsPanel', () => {
  it('renders the selected plugin: header, metadata and a rendered readme', () => {
    // The panel is parameterised via the store (center panels open by id only).
    setSelectedExtension('git');
    render(<ExtensionDetailsPanel />);

    // The whole panel is labelled with the plugin name.
    expect(screen.getByLabelText('Extension: Git')).toBeInTheDocument();

    // Metadata sidebar: the block labels + a Resources link.
    expect(screen.getByText('Identifier')).toBeInTheDocument();
    expect(screen.getByText('Categories')).toBeInTheDocument();
    expect(screen.getByText('Resources')).toBeInTheDocument();
    expect(screen.getByRole('link', { name: 'Repository' })).toBeInTheDocument();

    // The readme markdown rendered into real headings (`#`/`##` → <h1>/<h2>).
    expect(screen.getByRole('heading', { name: /Git — Source Control/i })).toBeInTheDocument();
    expect(screen.getByRole('heading', { name: 'Features' })).toBeInTheDocument();

    // Contributes block surfaces what the plugin adds — incl. its command name
    // (also mentioned in the readme, hence getAllByText).
    expect(screen.getByText('Contributes')).toBeInTheDocument();
    expect(screen.getAllByText('dialog.openAdvancedGit').length).toBeGreaterThanOrEqual(1);
  });

  it('shows an empty state when the selected id matches no plugin', () => {
    setSelectedExtension('does-not-exist');
    render(<ExtensionDetailsPanel />);
    expect(screen.getByText(/select an extension/i)).toBeInTheDocument();
  });
});
