import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

const {
  mockConnList,
  mockSftpOpen,
  mockSftpList,
  mockSftpClose,
  mockPickFolder,
  mockListDirectoryEntries,
} = vi.hoisted(() => ({
  mockConnList: vi.fn(),
  mockSftpOpen: vi.fn(),
  mockSftpList: vi.fn(),
  mockSftpClose: vi.fn(),
  mockPickFolder: vi.fn(),
  mockListDirectoryEntries: vi.fn(),
}));

vi.mock('@infrastructure/tauri/bridge', () => ({
  connList: mockConnList,
  sftpOpen: mockSftpOpen,
  sftpList: mockSftpList,
  sftpClose: mockSftpClose,
  sftpRead: vi.fn(),
  sftpWrite: vi.fn(),
  pickFolder: mockPickFolder,
  listDirectoryEntries: mockListDirectoryEntries,
  readWorkspaceFileBytes: vi.fn(),
  writeWorkspaceFileBytes: vi.fn(),
  createEmptyFileUnderWorkspace: vi.fn(),
}));

import { SftpPanel } from '@plugins/connections/SftpPanel';

describe('SftpPanel', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    mockConnList.mockResolvedValue([
      {
        id: 's1',
        label: 'prod',
        kind: 'ssh',
        tags: [],
        ssh: { address: '10.0.0.1', port: 22, username: 'root', password: 'hunter2' },
      },
      // A serial host that must NOT appear in the SFTP host dropdown.
      { id: 'c1', label: 'Board', kind: 'serial', tags: [], serial: { port: 'COM7', baud: 115200 } },
    ]);
    mockSftpOpen.mockResolvedValue(undefined);
    mockSftpList.mockResolvedValue([
      { name: 'etc', path: '/home/root/etc', isDir: true, size: 0 },
      { name: 'notes.txt', path: '/home/root/notes.txt', isDir: false, size: 12 },
    ]);
    mockSftpClose.mockResolvedValue(undefined);
  });

  it('lists only SSH hosts in the dropdown', async () => {
    render(<SftpPanel />);
    await waitFor(() => expect(mockConnList).toHaveBeenCalled());
    expect(await screen.findByRole('option', { name: 'prod' })).toBeInTheDocument();
    expect(screen.queryByRole('option', { name: 'Board' })).not.toBeInTheDocument();
  });

  it('connects: opens the SFTP session and lists the remote home', async () => {
    render(<SftpPanel />);
    const connect = await screen.findByRole('button', { name: /^connect$/i });
    fireEvent.click(connect);

    await waitFor(() => expect(mockSftpOpen).toHaveBeenCalledTimes(1));
    expect(mockSftpOpen).toHaveBeenCalledWith('sftp-panel', '10.0.0.1', 22, 'root', 'hunter2');
    // Remote listing kicks off right after connect (".": the session start dir).
    await waitFor(() => expect(mockSftpList).toHaveBeenCalledWith('sftp-panel', '.'));
    expect(await screen.findByText('notes.txt')).toBeInTheDocument();
  });

  it('picks a local folder and lists it in the left pane', async () => {
    mockPickFolder.mockResolvedValue('/tmp/work');
    mockListDirectoryEntries.mockResolvedValue([
      { name: 'a.bin', path: '/tmp/work/a.bin', isDirectory: false },
    ]);
    render(<SftpPanel />);

    fireEvent.click(await screen.findByRole('button', { name: /change/i }));
    await waitFor(() =>
      expect(mockListDirectoryEntries).toHaveBeenCalledWith('/tmp/work', '/tmp/work'),
    );
    expect(await screen.findByText('a.bin')).toBeInTheDocument();
  });
});
