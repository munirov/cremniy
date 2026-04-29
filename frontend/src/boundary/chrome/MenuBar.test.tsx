import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';

import { MAIN_MENU_LABELS } from '@domain/menu/mainMenu';

import { MenuBar } from './MenuBar';

describe('MenuBar', () => {
  it('renders top-level menu labels in Qt MenuBarBuilder order', () => {
    render(<MenuBar />);

    const buttons = screen.getAllByRole('button');
    expect(buttons.map((el) => el.textContent)).toEqual([...MAIN_MENU_LABELS]);
  });
});
