import { fireEvent, render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import { IdeEditorTabStrip } from './IdeEditorTabStrip';

const ideSessionMocks = vi.hoisted(() => ({
  openFilePaths: [] as string[],
  activeFilePath: null as string | null,
  dirtyFilePaths: [] as string[],
  activateOpenFile: vi.fn(),
  closeOpenFile: vi.fn(),
}));

vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: () => ({
    openFilePaths: ideSessionMocks.openFilePaths,
    activeFilePath: ideSessionMocks.activeFilePath,
    dirtyFilePaths: ideSessionMocks.dirtyFilePaths,
    activateOpenFile: ideSessionMocks.activateOpenFile,
    closeOpenFile: ideSessionMocks.closeOpenFile,
  }),
}));

describe('IdeEditorTabStrip', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    ideSessionMocks.openFilePaths = [];
    ideSessionMocks.activeFilePath = null;
    ideSessionMocks.dirtyFilePaths = [];
    ideSessionMocks.activateOpenFile.mockReset();
    ideSessionMocks.closeOpenFile.mockReset();
  });

  it('shows empty state when no files are open', () => {
    render(<IdeEditorTabStrip />);

    expect(screen.getByRole('toolbar', { name: 'Open document tabs' })).toBeInTheDocument();
    expect(screen.getByText('No open files')).toBeInTheDocument();
  });

  it('renders a tab per open path with file name labels', () => {
    ideSessionMocks.openFilePaths = ['/proj/alpha.txt', 'D:\\ws\\nested\\beta.md'];
    ideSessionMocks.activeFilePath = '/proj/alpha.txt';

    render(<IdeEditorTabStrip />);

    const tabs = screen.getAllByRole('tab');
    expect(tabs).toHaveLength(2);
    expect(tabs[0]).toHaveTextContent('alpha.txt');
    expect(tabs[1]).toHaveTextContent('beta.md');
    expect(tabs[0]).toHaveAttribute('aria-selected', 'true');
    expect(tabs[1]).toHaveAttribute('aria-selected', 'false');
  });

  it('marks dirty tabs in their label', () => {
    ideSessionMocks.openFilePaths = ['/proj/alpha.txt'];
    ideSessionMocks.activeFilePath = '/proj/alpha.txt';
    ideSessionMocks.dirtyFilePaths = ['/proj/alpha.txt'];

    render(<IdeEditorTabStrip />);

    expect(screen.getByRole('tab', { name: 'alpha.txt *' })).toBeInTheDocument();
  });

  it('calls activateOpenFile when a tab is clicked', () => {
    ideSessionMocks.openFilePaths = ['/a/first.txt', '/a/second.txt'];
    ideSessionMocks.activeFilePath = '/a/first.txt';

    render(<IdeEditorTabStrip />);

    fireEvent.click(screen.getByRole('tab', { name: 'second.txt' }));

    expect(ideSessionMocks.activateOpenFile).toHaveBeenCalledTimes(1);
    expect(ideSessionMocks.activateOpenFile).toHaveBeenCalledWith('/a/second.txt');
  });

  it('calls closeOpenFile when close control is clicked', () => {
    ideSessionMocks.openFilePaths = ['/x/one.rs'];
    ideSessionMocks.activeFilePath = '/x/one.rs';

    render(<IdeEditorTabStrip />);

    fireEvent.click(screen.getByRole('button', { name: 'Close one.rs' }));

    expect(ideSessionMocks.closeOpenFile).toHaveBeenCalledTimes(1);
    expect(ideSessionMocks.closeOpenFile).toHaveBeenCalledWith('/x/one.rs');
  });

  it('closes tab row on middle-button (auxclick)', () => {
    ideSessionMocks.openFilePaths = ['/pkg/main.rs'];
    ideSessionMocks.activeFilePath = '/pkg/main.rs';

    render(<IdeEditorTabStrip />);

    const rowEl = document.querySelector('[data-open-file-path="/pkg/main.rs"]');
    expect(rowEl).not.toBeNull();
    fireEvent(
      rowEl!,
      new MouseEvent('auxclick', {
        bubbles: true,
        cancelable: true,
        button: 1,
      }),
    );

    expect(ideSessionMocks.closeOpenFile).toHaveBeenCalledTimes(1);
    expect(ideSessionMocks.closeOpenFile).toHaveBeenCalledWith('/pkg/main.rs');
  });

  it('cycles active tab with ArrowRight key on tab strip', () => {
    ideSessionMocks.openFilePaths = ['/x/a.ts', '/x/b.ts'];
    ideSessionMocks.activeFilePath = '/x/a.ts';

    render(<IdeEditorTabStrip />);

    const tablist = screen.getByRole('tablist');
    fireEvent.keyDown(tablist, { key: 'ArrowRight', code: 'ArrowRight' });

    expect(ideSessionMocks.activateOpenFile).toHaveBeenCalledTimes(1);
    expect(ideSessionMocks.activateOpenFile).toHaveBeenCalledWith('/x/b.ts');
  });

  it('cycles active tab with ArrowLeft from first to last file', () => {
    ideSessionMocks.openFilePaths = ['/x/a.ts', '/x/b.ts'];
    ideSessionMocks.activeFilePath = '/x/a.ts';

    render(<IdeEditorTabStrip />);

    const tablist = screen.getByRole('tablist');
    fireEvent.keyDown(tablist, { key: 'ArrowLeft', code: 'ArrowLeft' });

    expect(ideSessionMocks.activateOpenFile).toHaveBeenCalledWith('/x/b.ts');
  });

  it('gives tabindex 0 only to the active tab select button', () => {
    ideSessionMocks.openFilePaths = ['/a/first.txt', '/a/second.txt'];
    ideSessionMocks.activeFilePath = '/a/second.txt';

    render(<IdeEditorTabStrip />);

    const tabs = screen.getAllByRole('tab');
    expect(tabs[0]).toHaveAttribute('tabIndex', '-1');
    expect(tabs[1]).toHaveAttribute('tabIndex', '0');
  });
});
