import { fireEvent, render, screen, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, expect, it, vi } from 'vitest';

import { BinaryFindGoDialog } from './BinaryFindGoDialog';

function statusRegion() {
  return screen.getByRole('status');
}

describe('BinaryFindGoDialog', () => {
  it('shows offset validation when Go is pressed with empty offset', async () => {
    const user = userEvent.setup();
    const onGoToOffset = vi.fn();
    render(
      <BinaryFindGoDialog
        bufferLength={100}
        onClose={vi.fn()}
        onFindBytes={() => true}
        onGoToOffset={onGoToOffset}
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
        bufferLength={100}
        onClose={vi.fn()}
        onFindBytes={() => true}
        onGoToOffset={vi.fn()}
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
        bufferLength={100}
        onClose={vi.fn()}
        onFindBytes={vi.fn()}
        onGoToOffset={vi.fn()}
      />,
    );

    await user.type(screen.getByLabelText('Pattern'), '414');
    await user.click(screen.getByRole('button', { name: 'Find' }));

    expect(
      within(statusRegion()).getByText('Hex bytes need an even number of digits.'),
    ).toBeInTheDocument();
  });

  it('shows not found when onFindBytes returns false', async () => {
    const user = userEvent.setup();
    const onFindBytes = vi.fn().mockReturnValue(false);
    render(
      <BinaryFindGoDialog
        bufferLength={4}
        onClose={vi.fn()}
        onFindBytes={onFindBytes}
        onGoToOffset={vi.fn()}
      />,
    );

    await user.type(screen.getByLabelText('Pattern'), 'ff');
    await user.click(screen.getByRole('button', { name: 'Find' }));

    expect(onFindBytes).toHaveBeenCalled();
    expect(within(statusRegion()).getByText('Not found.')).toBeInTheDocument();
  });

  it('calls onGoToOffset for valid hex offset', async () => {
    const user = userEvent.setup();
    const onGoToOffset = vi.fn();
    render(
      <BinaryFindGoDialog
        bufferLength={256}
        onClose={vi.fn()}
        onFindBytes={() => true}
        onGoToOffset={onGoToOffset}
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
        bufferLength={10}
        onClose={onClose}
        onFindBytes={() => true}
        onGoToOffset={vi.fn()}
      />,
    );

    await user.click(screen.getByRole('button', { name: 'Close' }));
    expect(onClose).toHaveBeenCalled();
  });

  it('closes on Escape', () => {
    const onClose = vi.fn();
    render(
      <BinaryFindGoDialog
        bufferLength={10}
        onClose={onClose}
        onFindBytes={() => true}
        onGoToOffset={vi.fn()}
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
        bufferLength={256}
        onClose={vi.fn()}
        onFindBytes={() => true}
        onGoToOffset={onGoToOffset}
      />,
    );

    await user.type(screen.getByLabelText('Offset'), '10');
    await user.keyboard('{Enter}');
    expect(onGoToOffset).toHaveBeenCalledWith(16);
  });
});
