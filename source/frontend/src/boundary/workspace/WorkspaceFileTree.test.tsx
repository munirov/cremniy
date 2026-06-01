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
}));

describe('WorkspaceFileTree', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    ideSessionMocks.activeFilePath = null;
    ideSessionMocks.fileTreeRevision = 0;
    ideSessionMocks.openFileFromWorkspace.mockReset();
    ideSessionMocks.openFileFromWorkspace.mockImplementation(() => Promise.resolve());
    ideSessionMocks.bumpFileTreeRevision.mockReset();
    vi.mocked(listDirectoryEntries).mockReset();
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
});
