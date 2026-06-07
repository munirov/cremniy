import { act, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import { listDirectoryEntries } from '@infrastructure/tauri/bridge';

import { WorkspaceFileTree } from './WorkspaceFileTree';

const ideSessionMocks = vi.hoisted(() => ({
  activeFilePath: null as string | null,
  openFileFromWorkspace: vi.fn((_path: string) => Promise.resolve()),
  bumpFileTreeRevision: vi.fn(),
  fileTreeRevision: 0,
}));

vi.mock('./IdeSessionContext', () => ({
  useIdeSession: () => ({
    activeFilePath: ideSessionMocks.activeFilePath,
    openFileFromWorkspace: ideSessionMocks.openFileFromWorkspace,
    bumpFileTreeRevision: ideSessionMocks.bumpFileTreeRevision,
    fileTreeRevision: ideSessionMocks.fileTreeRevision,
  }),
}));

vi.mock('@infrastructure/tauri/bridge', () => ({
  listDirectoryEntries: vi.fn(),
  // The tree mounts GitDecorationsProvider, which discovers repos + reads status.
  // Stub both to "no repos" so decorations stay inert in these tests.
  gitRepos: vi.fn(() => Promise.resolve([])),
  gitStatus: vi.fn(() =>
    Promise.resolve({ isRepo: false, branch: null, ahead: 0, behind: 0, files: [] }),
  ),
}));

vi.mock('@boundary/notifications/NotificationContext', () => ({
  useNotify: () => ({
    info: vi.fn(),
    success: vi.fn(),
    warn: vi.fn(),
    error: vi.fn(),
  }),
}));

vi.mock('@infrastructure/preferences/preferencesBridge', () => ({
  loadPreferences: () => Promise.resolve({ excludedFilePatterns: '' }),
}));

describe('WorkspaceFileTree', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    localStorage.clear(); // tree order + manual nesting persist here
    ideSessionMocks.activeFilePath = null;
    ideSessionMocks.fileTreeRevision = 0;
    ideSessionMocks.openFileFromWorkspace.mockReset();
    ideSessionMocks.openFileFromWorkspace.mockImplementation(() => Promise.resolve());
    ideSessionMocks.bumpFileTreeRevision.mockReset();
    vi.mocked(listDirectoryEntries).mockReset();
    // Default so the root-load effect never rejects in tests that don't set
    // their own resolution (each test overrides with mockReturnValueOnce).
    vi.mocked(listDirectoryEntries).mockResolvedValue([]);
  });

  it('shows empty state when no workspace root', () => {
    render(<WorkspaceFileTree workspaceRoot={null} />);

    expect(screen.getByRole('status')).toHaveTextContent('No workspace folder open.');
    expect(screen.getByText('Use File → Open folder.')).toBeInTheDocument();
    expect(listDirectoryEntries).not.toHaveBeenCalled();
  });

  it('shows empty state when workspace path is empty string', () => {
    render(<WorkspaceFileTree workspaceRoot={{ path: '' }} />);

    expect(screen.getByRole('status')).toHaveTextContent('No workspace folder open.');
    expect(listDirectoryEntries).not.toHaveBeenCalled();
  });

  it('shows loading then tree after root listDirectoryEntries resolves', async () => {
    let resolveRoot!: (entries: Awaited<ReturnType<typeof listDirectoryEntries>>) => void;
    const rootPromise = new Promise<Awaited<ReturnType<typeof listDirectoryEntries>>>((r) => {
      resolveRoot = r;
    });
    vi.mocked(listDirectoryEntries).mockReturnValueOnce(rootPromise as never);

    render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

    const busy = screen.getByRole('status', { busy: true });
    expect(busy).toHaveTextContent('Loading files…');

    await act(async () => {
      resolveRoot([{ name: 'readme.md', path: '/w/readme.md', isDirectory: false }]);
    });

    await waitFor(() => {
      expect(screen.getByRole('tree', { name: 'Workspace files' })).toBeInTheDocument();
    });
    expect(screen.getByRole('treeitem', { name: 'readme.md' })).toBeInTheDocument();
    expect(listDirectoryEntries).toHaveBeenCalledWith('/w', '/w');
  });

  it('shows empty folder message when root directory has no entries', async () => {
    vi.mocked(listDirectoryEntries).mockResolvedValueOnce([]);

    render(<WorkspaceFileTree workspaceRoot={{ path: '/empty' }} />);

    await waitFor(() => {
      expect(screen.getByRole('status')).toHaveTextContent('This folder is empty.');
    });
    expect(listDirectoryEntries).toHaveBeenCalledWith('/empty', '/empty');
  });

  it('expands a folder and lists children from listDirectoryEntries', async () => {
    vi.mocked(listDirectoryEntries)
      .mockResolvedValueOnce([{ name: 'src', path: '/w/src', isDirectory: true }])
      .mockResolvedValueOnce([
        { name: 'main.ts', path: '/w/src/main.ts', isDirectory: false },
      ]);

    render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

    await waitFor(() => {
      expect(screen.getByRole('treeitem', { name: 'src' })).toBeInTheDocument();
    });

    fireEvent.click(screen.getByRole('treeitem', { name: 'src' }));

    await waitFor(() => {
      expect(screen.getByRole('treeitem', { name: 'main.ts' })).toBeInTheDocument();
    });
    expect(listDirectoryEntries).toHaveBeenLastCalledWith('/w', '/w/src');
  });

  describe('keyboard navigation', () => {
    it('gives the first row the single roving tab stop', async () => {
      vi.mocked(listDirectoryEntries).mockResolvedValueOnce([
        { name: 'a.txt', path: '/w/a.txt', isDirectory: false },
        { name: 'b.txt', path: '/w/b.txt', isDirectory: false },
      ]);
      render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

      const a = await screen.findByRole('treeitem', { name: 'a.txt' });
      expect(a).toHaveAttribute('tabindex', '0');
      expect(screen.getByRole('treeitem', { name: 'b.txt' })).toHaveAttribute('tabindex', '-1');
    });

    it('moves focus to the next/previous row with ArrowDown/ArrowUp', async () => {
      vi.mocked(listDirectoryEntries).mockResolvedValueOnce([
        { name: 'a.txt', path: '/w/a.txt', isDirectory: false },
        { name: 'b.txt', path: '/w/b.txt', isDirectory: false },
      ]);
      render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

      const a = await screen.findByRole('treeitem', { name: 'a.txt' });
      a.focus();
      const tree = screen.getByRole('tree', { name: 'Workspace files' });
      fireEvent.keyDown(tree, { key: 'ArrowDown' });
      expect(screen.getByRole('treeitem', { name: 'b.txt' })).toHaveFocus();
      fireEvent.keyDown(tree, { key: 'ArrowUp' });
      expect(a).toHaveFocus();
    });

    it('expands a folder with ArrowRight and collapses with ArrowLeft', async () => {
      vi.mocked(listDirectoryEntries)
        .mockResolvedValueOnce([{ name: 'src', path: '/w/src', isDirectory: true }])
        .mockResolvedValueOnce([{ name: 'main.ts', path: '/w/src/main.ts', isDirectory: false }]);
      render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

      const srcItem = await screen.findByRole('treeitem', { name: 'src' });
      const tree = screen.getByRole('tree', { name: 'Workspace files' });
      srcItem.focus();
      fireEvent.keyDown(tree, { key: 'ArrowRight' });
      expect(await screen.findByRole('treeitem', { name: 'main.ts' })).toBeInTheDocument();

      srcItem.focus();
      fireEvent.keyDown(tree, { key: 'ArrowLeft' });
      await waitFor(() => {
        expect(screen.queryByRole('treeitem', { name: 'main.ts' })).not.toBeInTheDocument();
      });
    });

    it('opens the focused file with Enter', async () => {
      vi.mocked(listDirectoryEntries).mockResolvedValueOnce([
        { name: 'notes.txt', path: '/w/notes.txt', isDirectory: false },
      ]);
      render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

      const item = await screen.findByRole('treeitem', { name: 'notes.txt' });
      item.focus();
      // The leaf is a <button>, so Enter activates it natively (click).
      fireEvent.click(item);
      expect(ideSessionMocks.openFileFromWorkspace).toHaveBeenCalledWith('/w/notes.txt');
    });
  });

  it('calls openFileFromWorkspace when a file tree item is clicked', async () => {
    vi.mocked(listDirectoryEntries).mockResolvedValueOnce([
      { name: 'notes.txt', path: '/w/notes.txt', isDirectory: false },
    ]);

    render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

    await waitFor(() => {
      expect(screen.getByRole('treeitem', { name: 'notes.txt' })).toBeInTheDocument();
    });

    fireEvent.click(screen.getByRole('treeitem', { name: 'notes.txt' }));

    expect(ideSessionMocks.openFileFromWorkspace).toHaveBeenCalledTimes(1);
    expect(ideSessionMocks.openFileFromWorkspace).toHaveBeenCalledWith('/w/notes.txt');
  });

  it('shows error when root listDirectoryEntries rejects', async () => {
    vi.mocked(listDirectoryEntries).mockRejectedValueOnce(new Error('list_directory failed'));

    render(<WorkspaceFileTree workspaceRoot={{ path: '/bad' }} />);

    await waitFor(() => {
      expect(screen.getByRole('alert')).toHaveTextContent('Could not list workspace.');
    });
    expect(screen.getByText('list_directory failed')).toBeInTheDocument();
  });

  it('marks active file treeitem when activeFilePath matches', async () => {
    ideSessionMocks.activeFilePath = '/w/app.ts';
    vi.mocked(listDirectoryEntries).mockResolvedValueOnce([
      { name: 'app.ts', path: '/w/app.ts', isDirectory: false },
    ]);

    render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

    await waitFor(() => {
      expect(screen.getByRole('treeitem', { name: 'app.ts' })).toHaveAttribute('aria-selected', 'true');
    });
  });

  it('context menu on a file leaf offers New file and New folder (parent directory)', async () => {
    vi.mocked(listDirectoryEntries).mockResolvedValueOnce([
      { name: 'readme.md', path: '/w/readme.md', isDirectory: false },
    ]);

    render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

    await waitFor(() => {
      expect(screen.getByRole('treeitem', { name: 'readme.md' })).toBeInTheDocument();
    });

    fireEvent.contextMenu(screen.getByRole('treeitem', { name: 'readme.md' }));

    await waitFor(() => {
      expect(screen.getByTestId('file-tree-ctx-menu')).toBeInTheDocument();
    });
    expect(screen.getByRole('menuitem', { name: 'New file…' })).toBeInTheDocument();
    expect(screen.getByRole('menuitem', { name: 'New folder…' })).toBeInTheDocument();
  });

  describe('manual nesting', () => {
    const DRAG_PATH = 'application/x-cremniy-tree-path';
    const DRAG_FILE = 'application/x-cremniy-tree-file';

    it('nests a file onto a sibling, then un-nests it from the context menu', async () => {
      vi.mocked(listDirectoryEntries).mockResolvedValue([
        { name: 'a.md', path: '/w/a.md', isDirectory: false },
        { name: 'b.md', path: '/w/b.md', isDirectory: false },
      ]);
      render(<WorkspaceFileTree workspaceRoot={{ path: '/w' }} />);

      const aItem = await screen.findByRole('treeitem', { name: 'a.md' });
      expect(screen.getByRole('treeitem', { name: 'b.md' })).toBeInTheDocument();

      // Drop b.md onto a.md → b nests under a (visual only, persisted).
      fireEvent.drop(aItem, {
        dataTransfer: {
          getData: (t: string) => (t === DRAG_PATH ? '/w/b.md' : ''),
          types: [DRAG_PATH, DRAG_FILE],
        },
      });

      // b.md is hidden under the (collapsed) a.md, which is now expandable.
      await waitFor(() => {
        expect(screen.queryByRole('treeitem', { name: 'b.md' })).not.toBeInTheDocument();
      });
      expect(screen.getByRole('treeitem', { name: 'a.md' })).toHaveAttribute('aria-expanded');

      // Expand a.md (ArrowRight) → b.md appears as its nested child.
      const tree = screen.getByRole('tree', { name: 'Workspace files' });
      screen.getByRole('treeitem', { name: 'a.md' }).focus();
      fireEvent.keyDown(tree, { key: 'ArrowRight' });
      const bItem = await screen.findByRole('treeitem', { name: 'b.md' });

      // Un-nest b.md from its context menu → both back at the top level.
      fireEvent.contextMenu(bItem);
      fireEvent.click(screen.getByRole('menuitem', { name: 'Un-nest' }));

      await waitFor(() => {
        expect(screen.getByRole('treeitem', { name: 'b.md' })).toBeInTheDocument();
      });
      expect(screen.getByRole('treeitem', { name: 'a.md' })).not.toHaveAttribute('aria-expanded');
    });
  });
});
