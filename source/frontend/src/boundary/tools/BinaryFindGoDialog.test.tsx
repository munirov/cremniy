import { fireEvent, render, screen, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, expect, it, vi } from 'vitest';

import { BinaryFindGoDialog } from './BinaryFindGoDialog';

function statusRegion() {
  return screen.getByRole('status');
}

// "Hex" appears as both an offset-radix radio and a search-mode radio, so scope
// the search-mode pick to its own group.
function selectHexSearchMode() {
  return within(screen.getByRole('group', { name: 'Search mode' })).getByRole('radio', {
    name: 'Hex',
  });
}

describe('BinaryFindGoDialog', () => {
  it('shows offset validation when Go is pressed with empty offset', async () => {
    const user = userEvent.setup();
    const onGoToOffset = vi.fn();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array(100)}
        cursorOffset={0}
        onClose={vi.fn()}
        onGoToOffset={onGoToOffset}
        onSelectRange={vi.fn()}
      />,
    );

    await user.click(screen.getByRole('button', { name: 'Go' }));

    expect(within(statusRegion()).getByText('Enter an offset.')).toBeInTheDocument();
    expect(onGoToOffset).not.toHaveBeenCalled();
  });

  it('shows decimal validation for invalid offset input', async () => {
    const user = userEvent.setup();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array(100)}
        cursorOffset={0}
        onClose={vi.fn()}
        onGoToOffset={vi.fn()}
        onSelectRange={vi.fn()}
      />,
    );

    await user.click(screen.getByRole('radio', { name: 'Decimal' }));
    await user.type(screen.getByLabelText('Offset'), '10a');
    await user.click(screen.getByRole('button', { name: 'Go' }));

    expect(
      within(statusRegion()).getByText('Decimal offset must be non-negative digits only.'),
    ).toBeInTheDocument();
  });

  it('shows hex find validation for odd digit count', async () => {
    const user = userEvent.setup();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array(100)}
        cursorOffset={0}
        onClose={vi.fn()}
        onGoToOffset={vi.fn()}
        onSelectRange={vi.fn()}
      />,
    );

    await user.click(selectHexSearchMode());
    await user.type(screen.getByLabelText('Find'), '414');
    await user.click(screen.getByRole('button', { name: 'Find next' }));

    expect(
      within(statusRegion()).getByText('Hex bytes need an even number of digits.'),
    ).toBeInTheDocument();
  });

  it('shows not found when the pattern is absent from the buffer', async () => {
    const user = userEvent.setup();
    const onSelectRange = vi.fn();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array([0x41, 0x42, 0x43, 0x44])}
        cursorOffset={0}
        onClose={vi.fn()}
        onGoToOffset={vi.fn()}
        onSelectRange={onSelectRange}
      />,
    );

    await user.click(selectHexSearchMode());
    await user.type(screen.getByLabelText('Find'), 'ff');
    await user.click(screen.getByRole('button', { name: 'Find next' }));

    expect(onSelectRange).not.toHaveBeenCalled();
    expect(within(statusRegion()).getByText('Not found.')).toBeInTheDocument();
  });

  it('selects the match range when the pattern is found in the buffer', async () => {
    const user = userEvent.setup();
    const onSelectRange = vi.fn();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array([0x41, 0x42, 0xff, 0x43])}
        cursorOffset={0}
        onClose={vi.fn()}
        onGoToOffset={vi.fn()}
        onSelectRange={onSelectRange}
      />,
    );

    await user.click(selectHexSearchMode());
    await user.type(screen.getByLabelText('Find'), 'ff');
    await user.click(screen.getByRole('button', { name: 'Find next' }));

    expect(onSelectRange).toHaveBeenCalledWith(2, 1);
  });

  it('calls onGoToOffset for valid hex offset', async () => {
    const user = userEvent.setup();
    const onGoToOffset = vi.fn();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array(256)}
        cursorOffset={0}
        onClose={vi.fn()}
        onGoToOffset={onGoToOffset}
        onSelectRange={vi.fn()}
      />,
    );

    await user.type(screen.getByLabelText('Offset'), '10');
    await user.click(screen.getByRole('button', { name: 'Go' }));

    expect(onGoToOffset).toHaveBeenCalledWith(16);
    expect(statusRegion()).toHaveTextContent('');
  });

  it('closes when Close is clicked', async () => {
    const user = userEvent.setup();
    const onClose = vi.fn();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array(10)}
        cursorOffset={0}
        onClose={onClose}
        onGoToOffset={vi.fn()}
        onSelectRange={vi.fn()}
      />,
    );

    await user.click(screen.getByRole('button', { name: 'Close' }));
    expect(onClose).toHaveBeenCalled();
  });

  it('closes on Escape', () => {
    const onClose = vi.fn();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array(10)}
        cursorOffset={0}
        onClose={onClose}
        onGoToOffset={vi.fn()}
        onSelectRange={vi.fn()}
      />,
    );

    fireEvent.keyDown(window, { key: 'Escape' });
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it('triggers Go when Enter is pressed in offset field', async () => {
    const user = userEvent.setup();
    const onGoToOffset = vi.fn();
    render(
      <BinaryFindGoDialog
        buffer={new Uint8Array(256)}
        cursorOffset={0}
        onClose={vi.fn()}
        onGoToOffset={onGoToOffset}
        onSelectRange={vi.fn()}
      />,
    );

    await user.type(screen.getByLabelText('Offset'), '10');
    await user.keyboard('{Enter}');
    expect(onGoToOffset).toHaveBeenCalledWith(16);
  });
});
