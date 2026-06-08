import { fireEvent, render, screen } from '@testing-library/react';
import { beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';

import gitPlugin from '@plugins/git';
import { registerPlugin } from '@shared/plugins/registry';

import { IdeEditorTabStrip } from './IdeEditorTabStrip';

// The Advanced Git center panel ('advancedGit', label "Git") is a Git-plugin
// contribution now; the tab strip resolves it via resolveCenterPanel → the
// plugin registry, which the app fills at startup (loadPlugins). Tests don't
// boot main.tsx, so register the Git plugin here (registerPlugin is idempotent).
beforeAll(() => {
  registerPlugin(gitPlugin);
});

const ideSessionMocks = vi.hoisted(() => ({
  openFilePaths: [] as string[],
  activeFilePath: null as string | null,
  dirtyFilePaths: [] as string[],
  pinnedFilePaths: new Set<string>(),
  openPanels: [] as string[],
  activePanel: null as string | null,
  previewFilePath: null as string | null,
  activateOpenFile: vi.fn(),
  closeOpenFile: vi.fn(),
  closeOtherOpenFiles: vi.fn(),
  closeAllOpenFiles: vi.fn(),
  togglePinFilePath: vi.fn(),
  reorderOpenFiles: vi.fn(),
  activatePanel: vi.fn(),
  closePanel: vi.fn(),
}));

vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: () => ({
    openFilePaths: ideSessionMocks.openFilePaths,
    activeFilePath: ideSessionMocks.activeFilePath,
    dirtyFilePaths: ideSessionMocks.dirtyFilePaths,
    pinnedFilePaths: ideSessionMocks.pinnedFilePaths,
    openPanels: ideSessionMocks.openPanels,
    activePanel: ideSessionMocks.activePanel,
    previewFilePath: ideSessionMocks.previewFilePath,
    activateOpenFile: ideSessionMocks.activateOpenFile,
    closeOpenFile: ideSessionMocks.closeOpenFile,
    closeOtherOpenFiles: ideSessionMocks.closeOtherOpenFiles,
    closeAllOpenFiles: ideSessionMocks.closeAllOpenFiles,
    togglePinFilePath: ideSessionMocks.togglePinFilePath,
    reorderOpenFiles: ideSessionMocks.reorderOpenFiles,
    activatePanel: ideSessionMocks.activatePanel,
    closePanel: ideSessionMocks.closePanel,
  }),
}));

describe('IdeEditorTabStrip', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    ideSessionMocks.openFilePaths = [];
    ideSessionMocks.activeFilePath = null;
    ideSessionMocks.dirtyFilePaths = [];
    ideSessionMocks.pinnedFilePaths = new Set<string>();
    ideSessionMocks.openPanels = [];
    ideSessionMocks.activePanel = null;
    ideSessionMocks.previewFilePath = null;
    ideSessionMocks.activateOpenFile.mockReset();
    ideSessionMocks.closeOpenFile.mockReset();
    ideSessionMocks.closeOtherOpenFiles.mockReset();
    ideSessionMocks.closeAllOpenFiles.mockReset();
    ideSessionMocks.togglePinFilePath.mockReset();
    ideSessionMocks.reorderOpenFiles.mockReset();
    ideSessionMocks.activatePanel.mockReset();
    ideSessionMocks.closePanel.mockReset();
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

  it('renders pinned tabs first with a pin glyph', () => {
    ideSessionMocks.openFilePaths = ['/a/first.txt', '/a/pinned.txt'];
    ideSessionMocks.activeFilePath = '/a/first.txt';
    ideSessionMocks.pinnedFilePaths = new Set(['/a/pinned.txt']);

    render(<IdeEditorTabStrip />);

    const tabs = screen.getAllByRole('tab');
    // Pinned tab sorts ahead of the unpinned one and carries the 📌 marker.
    expect(tabs[0]).toHaveTextContent('📌');
    expect(tabs[0]).toHaveTextContent('pinned.txt');
    expect(tabs[1]).toHaveTextContent('first.txt');
  });

  it('renders a center-panel tab and activates it on click', () => {
    // Non-file center tabs (settings/git) render alongside file tabs via
    // resolveCenterPanel. With no file open, the empty-file branch is skipped
    // because a panel is open.
    ideSessionMocks.openPanels = ['settings'];
    ideSessionMocks.activePanel = 'settings';

    render(<IdeEditorTabStrip />);

    const panelTab = screen.getByRole('tab', { name: 'Settings' });
    expect(panelTab).toHaveAttribute('aria-selected', 'true');

    // Re-clicking re-activates the panel through the session.
    fireEvent.click(panelTab);
    expect(ideSessionMocks.activatePanel).toHaveBeenCalledWith('settings');
  });

  it('closes a center-panel tab via its close control', () => {
    ideSessionMocks.openPanels = ['advancedGit'];
    ideSessionMocks.activePanel = 'advancedGit';

    render(<IdeEditorTabStrip />);

    fireEvent.click(screen.getByRole('button', { name: 'Close Git' }));
    expect(ideSessionMocks.closePanel).toHaveBeenCalledWith('advancedGit');
  });

  it('marks the active file tab as unselected while a panel is active', () => {
    // A file tab is "active" only when its path matches AND no panel is showing.
    ideSessionMocks.openFilePaths = ['/a/first.txt'];
    ideSessionMocks.activeFilePath = '/a/first.txt';
    ideSessionMocks.openPanels = ['settings'];
    ideSessionMocks.activePanel = 'settings';

    render(<IdeEditorTabStrip />);

    expect(screen.getByRole('tab', { name: 'first.txt' })).toHaveAttribute(
      'aria-selected',
      'false',
    );
    expect(screen.getByRole('tab', { name: 'Settings' })).toHaveAttribute(
      'aria-selected',
      'true',
    );
  });
});
