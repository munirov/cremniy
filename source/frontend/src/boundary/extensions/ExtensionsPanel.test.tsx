import { render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it } from 'vitest';

import { ExtensionsPanel } from './ExtensionsPanel';

describe('ExtensionsPanel', () => {
  beforeEach(() => {
    localStorage.clear();
  });

  it('lists installed plugins from the catalog with delivery badges', () => {
    render(<ExtensionsPanel />);
    expect(screen.getByText('Extensions')).toBeInTheDocument();
    // Bundled (ship with the IDE) + recommended (official add-on) are all shown.
    expect(screen.getByText('Binary Tools')).toBeInTheDocument();
    expect(screen.getByText('Git')).toBeInTheDocument();
    expect(screen.getByText('Connections')).toBeInTheDocument();
    // Git + Binary Tools are bundled.
    expect(screen.getAllByText('Bundled').length).toBeGreaterThanOrEqual(2);
    // Connections is recommended → it has a Disable control; bundled ones don't.
    expect(screen.getByRole('button', { name: /disable/i })).toBeInTheDocument();
  });
});
