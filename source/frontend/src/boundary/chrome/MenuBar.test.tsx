import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { MAIN_MENU_LABELS } from '@domain/menu/mainMenu';

import { MenuBar } from './MenuBar';

describe('MenuBar', () => {
  it('renders top-level menu labels in Qt MenuBarBuilder order', () => {
    render(<MenuBar />);

    const buttons = screen.getAllByRole('button');
    expect(buttons.map((el) => el.textContent)).toEqual([...MAIN_MENU_LABELS]);
  });

  it('invokes onFileMenuAction with the chosen file menu id', () => {
    const onFileMenuAction = vi.fn();
    render(<MenuBar onFileMenuAction={onFileMenuAction} hasActiveDocument activeDocumentDirty />);

    fireEvent.click(screen.getByRole('button', { name: /^file$/i }));
    fireEvent.click(screen.getByRole('menuitem', { name: /open folder/i }));

    expect(onFileMenuAction).toHaveBeenCalledTimes(1);
    expect(onFileMenuAction).toHaveBeenCalledWith('openFolder');
  });

  it('closes file dropdown after a file menu action', () => {
    const onFileMenuAction = vi.fn();
    render(<MenuBar onFileMenuAction={onFileMenuAction} hasActiveDocument activeDocumentDirty />);

    const fileButton = screen.getByRole('button', { name: /^file$/i });
    fireEvent.click(fileButton);
    expect(screen.getByRole('menuitem', { name: /^save \*$/i })).toBeInTheDocument();

    fireEvent.click(screen.getByRole('menuitem', { name: /^save \*$/i }));
    expect(screen.queryByRole('menuitem', { name: /^save \*$/i })).not.toBeInTheDocument();
    expect(onFileMenuAction).toHaveBeenCalledWith('save');
  });

  it('exposes the terminal View item as a checked menu checkbox', () => {
    const { rerender } = render(<MenuBar terminalPanelVisible />);

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));

    expect(screen.queryByRole('menuitem', { name: /terminal panel/i })).not.toBeInTheDocument();
    expect(screen.getByRole('menuitemcheckbox', { name: /terminal panel/i })).toHaveAttribute('aria-checked', 'true');

    rerender(<MenuBar terminalPanelVisible={false} />);

    expect(screen.getByRole('menuitemcheckbox', { name: /terminal panel/i })).toHaveAttribute('aria-checked', 'false');
  });

  it('exposes word wrap as a checked View menu checkbox', () => {
    const onViewMenuAction = vi.fn();
    render(<MenuBar onViewMenuAction={onViewMenuAction} wordWrapEnabled />);

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    const wordWrapToggle = screen.getByRole('menuitemcheckbox', { name: /word wrap/i });
    expect(wordWrapToggle).toHaveAttribute('aria-checked', 'true');

    fireEvent.click(wordWrapToggle);

    expect(onViewMenuAction).toHaveBeenCalledWith('toggleWordWrap');
  });

  it('invokes the Edit find action instead of a top-level no-op', () => {
    const onEditMenuAction = vi.fn();
    render(<MenuBar onEditMenuAction={onEditMenuAction} hasActiveDocument />);

    fireEvent.click(screen.getByRole('button', { name: /^edit$/i }));
    fireEvent.click(screen.getByRole('menuitem', { name: /find in editor/i }));

    expect(onEditMenuAction).toHaveBeenCalledWith('findInEditor');
  });

  it('prevents default for matched editor-safe global shortcuts in text targets', () => {
    const onShortcut = vi.fn();
    const input = document.createElement('textarea');
    render(<MenuBar onShortcut={onShortcut} />);

    const event = new KeyboardEvent('keydown', { key: 's', ctrlKey: true, bubbles: true, cancelable: true });
    Object.defineProperty(event, 'target', { value: input, enumerable: true });
    window.dispatchEvent(event);

    expect(event.defaultPrevented).toBe(true);
    expect(onShortcut).toHaveBeenCalledWith({ kind: 'file', id: 'save' });
  });

  it('disables Build because no build actions exist yet', () => {
    render(<MenuBar />);

    expect(screen.getByRole('button', { name: /^build$/i })).toBeDisabled();
  });

  it('invokes Close editor when available', () => {
    const onFileMenuAction = vi.fn();
    render(<MenuBar onFileMenuAction={onFileMenuAction} closeEditorAvailable hasActiveDocument />);

    fireEvent.click(screen.getByRole('button', { name: /^file$/i }));
    fireEvent.click(screen.getByRole('menuitem', { name: /^close editor$/i }));

    expect(onFileMenuAction).toHaveBeenCalledWith('closeEditorTab');
  });

  it('disables Close editor when strip has no closable buffers', () => {
    const onFileMenuAction = vi.fn();
    render(
      <MenuBar onFileMenuAction={onFileMenuAction} closeEditorAvailable={false} hasActiveDocument />,
    );

    fireEvent.click(screen.getByRole('button', { name: /^file$/i }));
    expect(screen.getByRole('menuitem', { name: /^close editor$/i })).toBeDisabled();
  });

  it('disables Save when the active document is clean', () => {
    const onFileMenuAction = vi.fn();
    const { rerender } = render(
      <MenuBar onFileMenuAction={onFileMenuAction} hasActiveDocument activeDocumentDirty={false} />,
    );

    fireEvent.click(screen.getByRole('button', { name: /^file$/i }));

    expect(screen.getByRole('menuitem', { name: /^save$/i })).toBeDisabled();

    rerender(<MenuBar onFileMenuAction={onFileMenuAction} hasActiveDocument activeDocumentDirty />);

    const dirtySave = screen.getByRole('menuitem', { name: /^save \*$/i });
    expect(dirtySave).toBeEnabled();

    fireEvent.click(dirtySave);

    expect(onFileMenuAction).toHaveBeenCalledWith('save');
  });
});
