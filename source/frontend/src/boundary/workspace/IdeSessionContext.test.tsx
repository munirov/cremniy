import { act, renderHook, screen } from '@testing-library/react';
import { MemoryRouter, useLocation } from 'react-router-dom';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ReactNode } from 'react';

import { DEFAULT_APP_PREFERENCES } from '@domain/preferences/appPreferences';

import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import {
  getWorkspaceFileSize,
  pickFile,
  pickFolder,
  pickSaveFile,
  readUserFile,
  readWorkspaceUserFile,
  writeUserFile,
} from '@infrastructure/tauri/bridge';

import { NotificationProvider } from '@boundary/notifications/NotificationContext';

import { IdeSessionProvider, useIdeSession } from './IdeSessionContext';
import { WorkspaceProvider } from './WorkspaceContext';

vi.mock('@infrastructure/tauri/bridge', () => ({
  pickFolder: vi.fn(),
  pickFile: vi.fn(),
  pickSaveFile: vi.fn(),
  readUserFile: vi.fn(),
  readWorkspaceUserFile: vi.fn(),
  writeUserFile: vi.fn(),
  getWorkspaceFileSize: vi.fn(),
}));

vi.mock('@infrastructure/preferences/preferencesBridge', () => ({
  loadPreferences: vi.fn(),
  savePreferences: vi.fn(),
}));

function useSessionAndLocation() {
  const session = useIdeSession();
  const loc = useLocation();
  return { session, loc };
}

function createWrapper(initialEntry: string) {
  return function Wrapper({ children }: { children: ReactNode }) {
    return (
      <MemoryRouter initialEntries={[initialEntry]}>
        <NotificationProvider>
          <WorkspaceProvider>
            <IdeSessionProvider>{children}</IdeSessionProvider>
          </WorkspaceProvider>
        </NotificationProvider>
      </MemoryRouter>
    );
  };
}

describe('IdeSessionContext runFileMenuAction', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    vi.mocked(pickFolder).mockReset();
    vi.mocked(pickFile).mockReset();
    vi.mocked(pickSaveFile).mockReset();
    vi.mocked(readUserFile).mockReset();
    vi.mocked(readWorkspaceUserFile).mockReset();
    vi.mocked(writeUserFile).mockReset();
    vi.mocked(getWorkspaceFileSize).mockReset();
    vi.mocked(loadPreferences).mockReset();
    vi.mocked(savePreferences).mockReset();
    vi.mocked(pickFolder).mockResolvedValue(null);
    vi.mocked(pickFile).mockResolvedValue(null);
    vi.mocked(pickSaveFile).mockResolvedValue(null);
    vi.mocked(readUserFile).mockResolvedValue('');
    vi.mocked(readWorkspaceUserFile).mockResolvedValue('');
    vi.mocked(writeUserFile).mockResolvedValue(undefined);
    vi.mocked(getWorkspaceFileSize).mockResolvedValue(1024);
    vi.mocked(loadPreferences).mockResolvedValue(DEFAULT_APP_PREFERENCES);
    vi.mocked(savePreferences).mockResolvedValue(undefined);
  });

  it('openFolder picks folder, persists preferences, and navigates with root query', async () => {
    vi.mocked(pickFolder).mockResolvedValueOnce('D:\\my-ws');
    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFolder');
    });

    expect(pickFolder).toHaveBeenCalledTimes(1);
    expect(loadPreferences).toHaveBeenCalledTimes(1);
    expect(savePreferences).toHaveBeenCalledTimes(1);
    expect(result.current.loc.pathname).toBe('/ide');
    expect(decodeURIComponent(result.current.loc.search)).toContain('root=D:\\my-ws');
  });

  it('openFolder does nothing when pickFolder returns null', async () => {
    vi.mocked(pickFolder).mockResolvedValueOnce(null);
    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFolder');
    });

    expect(savePreferences).not.toHaveBeenCalled();
    expect(result.current.loc.search).toBe('');
  });

  it('openFolder does nothing when pickFolder returns empty string', async () => {
    vi.mocked(pickFolder).mockResolvedValueOnce('');
    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFolder');
    });

    expect(savePreferences).not.toHaveBeenCalled();
  });

  it('openFile loads file under parent workspace and sets editor state', async () => {
    vi.mocked(pickFile).mockResolvedValueOnce('D:\\ws\\notes.txt');
    vi.mocked(readUserFile).mockResolvedValueOnce('hello');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });

    expect(readUserFile).toHaveBeenCalledWith('D:\\ws\\notes.txt');
    expect(result.current.session.activeFilePath).toBe('D:\\ws\\notes.txt');
    expect(result.current.session.documentText).toBe('hello');
    expect(decodeURIComponent(result.current.loc.search)).toContain('root=D:\\ws');
  });

  it('openFile alerts when parent directory cannot be resolved', async () => {
    vi.mocked(pickFile).mockResolvedValueOnce('bare-name.txt');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });

    expect(await screen.findByRole('alert')).toHaveTextContent(
      'Could not determine folder for the selected file.',
    );
    expect(readUserFile).not.toHaveBeenCalled();
    expect(savePreferences).not.toHaveBeenCalled();
  });

  it('save writes to active path when file is open', async () => {
    vi.mocked(pickFile).mockResolvedValueOnce('/proj/a.txt');
    vi.mocked(readUserFile).mockResolvedValueOnce('x');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });

    await act(async () => {
      result.current.session.setDocumentText('updated');
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('save');
    });

    expect(writeUserFile).toHaveBeenCalledWith('/proj/a.txt', 'updated');
    expect(pickSaveFile).not.toHaveBeenCalled();
    expect(result.current.session.activeDocumentDirty).toBe(false);
  });

  it('tracks dirty state for edited open files', async () => {
    vi.mocked(pickFile).mockResolvedValueOnce('/proj/a.txt');
    vi.mocked(readUserFile).mockResolvedValueOnce('saved');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });
    expect(result.current.session.activeDocumentDirty).toBe(false);
    expect(result.current.session.dirtyFilePaths).toEqual([]);

    await act(async () => {
      result.current.session.setDocumentText('edited');
    });

    expect(result.current.session.activeDocumentDirty).toBe(true);
    expect(result.current.session.dirtyFilePaths).toEqual(['/proj/a.txt']);
  });

  it('save uses save-as flow when no active file', async () => {
    vi.mocked(pickSaveFile).mockResolvedValueOnce('/out/new.txt');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      result.current.session.setDocumentText('draft');
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('save');
    });

    expect(pickSaveFile).toHaveBeenCalledWith(null);
    expect(writeUserFile).toHaveBeenCalledWith('/out/new.txt', 'draft');
    expect(result.current.session.activeFilePath).toBe('/out/new.txt');
  });

  it('saveAs always prompts and writes chosen path', async () => {
    vi.mocked(pickSaveFile).mockResolvedValueOnce('/x/y.md');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      result.current.session.setDocumentText('body');
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('saveAs');
    });

    expect(pickSaveFile).toHaveBeenCalledWith(null);
    expect(writeUserFile).toHaveBeenCalledWith('/x/y.md', 'body');
  });

  it('saveAs passes active path as default when set', async () => {
    vi.mocked(pickFile).mockResolvedValueOnce('/w/f.txt');
    vi.mocked(readUserFile).mockResolvedValueOnce('');
    vi.mocked(pickSaveFile).mockResolvedValueOnce('/w/f-copy.txt');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('saveAs');
    });

    expect(pickSaveFile).toHaveBeenCalledWith('/w/f.txt');
  });

  it('saveAs to the active path clears dirty state', async () => {
    vi.mocked(pickFile).mockResolvedValueOnce('/w/f.txt');
    vi.mocked(readUserFile).mockResolvedValueOnce('saved');
    vi.mocked(pickSaveFile).mockResolvedValueOnce('/w/f.txt');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });

    await act(async () => {
      result.current.session.setDocumentText('edited');
    });

    expect(result.current.session.activeDocumentDirty).toBe(true);
    expect(result.current.session.dirtyFilePaths).toEqual(['/w/f.txt']);

    await act(async () => {
      await result.current.session.runFileMenuAction('saveAs');
    });

    expect(pickSaveFile).toHaveBeenCalledWith('/w/f.txt');
    expect(writeUserFile).toHaveBeenCalledWith('/w/f.txt', 'edited');
    expect(result.current.session.activeFilePath).toBe('/w/f.txt');
    expect(result.current.session.activeDocumentDirty).toBe(false);
    expect(result.current.session.dirtyFilePaths).toEqual([]);
  });

  it('closeWorkspace clears session and navigates home', async () => {
    vi.mocked(pickFile).mockResolvedValueOnce('/p/a.txt');
    vi.mocked(readUserFile).mockResolvedValueOnce('z');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('closeWorkspace');
    });

    expect(result.current.loc.pathname).toBe('/');
    expect(result.current.session.activeFilePath).toBeNull();
    expect(result.current.session.documentText).toBe('');
  });

  it('alerts with message when bridge throws', async () => {
    vi.mocked(pickFolder).mockRejectedValueOnce(new Error('tauri failed'));

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.session.runFileMenuAction('openFolder');
    });

    expect(await screen.findByRole('alert')).toHaveTextContent('tauri failed');
  });

  it('openFileFromWorkspace loads file into editor state using workspace root', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('body');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/readme.md');
    });

    expect(readWorkspaceUserFile).toHaveBeenCalledWith('/proj', '/proj/readme.md');
    expect(readUserFile).not.toHaveBeenCalled();
    expect(result.current.activeFilePath).toBe('/proj/readme.md');
    expect(result.current.documentText).toBe('body');
  });

  it('openFileFromWorkspace opens a non-UTF-8 file as a binary tab', async () => {
    vi.mocked(readWorkspaceUserFile).mockRejectedValueOnce(new Error('not utf-8'));
    vi.mocked(getWorkspaceFileSize).mockResolvedValueOnce(4096);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/app.exe');
    });

    expect(result.current.activeFilePath).toBe('/proj/app.exe');
    expect(result.current.activeFileIsBinary).toBe(true);
    expect(result.current.openFilePaths).toContain('/proj/app.exe');
    expect(result.current.documentText).toBe('');
    expect(screen.queryByRole('alert')).toBeNull();
  });

  it('openFileFromWorkspace opens a NUL-carrying file as a binary tab', async () => {
    // Decodes as UTF-8 but holds NUL bytes (firmware dump, UTF-16, …) → binary.
    const withNul = 'MZ' + String.fromCharCode(0) + 'PE' + String.fromCharCode(0) + 'data';
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce(withNul);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/firmware.bin');
    });

    expect(result.current.activeFileIsBinary).toBe(true);
    expect(result.current.documentText).toBe('');
  });

  it('openFileFromWorkspace alerts when the file is unreadable', async () => {
    vi.mocked(readWorkspaceUserFile).mockRejectedValueOnce(new Error('permission denied'));
    vi.mocked(getWorkspaceFileSize).mockRejectedValueOnce(new Error('permission denied'));

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/secret');
    });

    expect(await screen.findByRole('alert')).toHaveTextContent('permission denied');
  });

  it('openFileFromWorkspace alerts when no workspace root in URL', async () => {
    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/readme.md');
    });

    expect(await screen.findByRole('alert')).toHaveTextContent(
      'No workspace is open. Open a folder from the File menu first.',
    );
    expect(readWorkspaceUserFile).not.toHaveBeenCalled();
  });

  it('activateOpenFile switches editor buffer per tab', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('alpha').mockResolvedValueOnce('beta');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/a.txt');
    });
    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/b.txt');
    });

    expect(result.current.documentText).toBe('beta');

    await act(async () => {
      result.current.activateOpenFile('/proj/a.txt');
    });
    expect(result.current.documentText).toBe('alpha');

    await act(async () => {
      result.current.setDocumentText('edited-a');
    });
    await act(async () => {
      result.current.activateOpenFile('/proj/b.txt');
    });
    expect(result.current.documentText).toBe('beta');

    await act(async () => {
      result.current.activateOpenFile('/proj/a.txt');
    });
    expect(result.current.documentText).toBe('edited-a');
  });

  it('openFileFromWorkspace reuses an already-open tab without reading again', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('once');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/x.txt');
    });
    vi.mocked(readWorkspaceUserFile).mockClear();

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/x.txt');
    });

    expect(readWorkspaceUserFile).not.toHaveBeenCalled();
    expect(result.current.activeFilePath).toBe('/proj/x.txt');
  });

  it('closeOpenFile removes a tab and picks an adjacent active file', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('a').mockResolvedValueOnce('b');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/first.txt');
    });
    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/second.txt');
    });

    await act(async () => {
      result.current.closeOpenFile('/proj/second.txt');
    });

    expect(result.current.openFilePaths).toEqual(['/proj/first.txt']);
    expect(result.current.activeFilePath).toBe('/proj/first.txt');
  });

  it('closeOpenFile does not discard a dirty tab when user cancels confirm', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('saved');
    const confirmSpy = vi.spyOn(window, 'confirm').mockReturnValue(false);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/dirty.txt');
    });
    await act(async () => {
      result.current.setDocumentText('unsaved edits');
    });
    await act(async () => {
      result.current.closeOpenFile('/proj/dirty.txt');
    });

    expect(confirmSpy).toHaveBeenCalled();
    expect(result.current.openFilePaths).toEqual(['/proj/dirty.txt']);

    confirmSpy.mockRestore();
  });

  it('closeOpenFile discards a dirty tab when user confirms', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('saved');
    const confirmSpy = vi.spyOn(window, 'confirm').mockReturnValue(true);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/dirty.txt');
    });
    await act(async () => {
      result.current.setDocumentText('discard me');
    });
    await act(async () => {
      result.current.closeOpenFile('/proj/dirty.txt');
    });

    expect(confirmSpy).toHaveBeenCalled();
    expect(result.current.openFilePaths).toEqual([]);
    confirmSpy.mockRestore();
  });

  it('closeOpenFile does not confirm when tab buffer matches saved baseline', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('clean');
    const confirmSpy = vi.spyOn(window, 'confirm').mockReturnValue(false);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/clean.txt');
    });
    await act(async () => {
      result.current.closeOpenFile('/proj/clean.txt');
    });

    expect(confirmSpy).not.toHaveBeenCalled();
    expect(result.current.openFilePaths).toEqual([]);

    confirmSpy.mockRestore();
  });

  it('runFileMenuAction(closeEditorTab) removes the active open tab', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('body');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/a.rs');
    });
    await act(async () => {
      await result.current.runFileMenuAction('closeEditorTab');
    });

    expect(result.current.openFilePaths).toEqual([]);
    expect(result.current.activeFilePath).toBeNull();
  });

  it('runFileMenuAction(closeEditorTab) confirms before clearing scratch-only text', async () => {
    const confirmSpy = vi.spyOn(window, 'confirm').mockReturnValue(false);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide'),
    });

    await act(async () => {
      result.current.setDocumentText('scratch');
    });
    await act(async () => {
      await result.current.runFileMenuAction('closeEditorTab');
    });

    expect(confirmSpy).toHaveBeenCalled();
    expect(result.current.documentText).toBe('scratch');

    confirmSpy.mockRestore();
  });

  it('saveAs merges into an existing tab when chosen path matches another open tab', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('a-body').mockResolvedValueOnce('b-body');
    vi.mocked(pickSaveFile).mockResolvedValueOnce('/proj/a.txt');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/a.txt');
    });
    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/b.txt');
    });

    await act(async () => {
      result.current.setDocumentText('merged-from-b');
    });

    await act(async () => {
      await result.current.runFileMenuAction('saveAs');
    });

    expect(writeUserFile).toHaveBeenCalledWith('/proj/a.txt', 'merged-from-b');
    expect(result.current.openFilePaths).toEqual(['/proj/a.txt']);
    expect(result.current.activeFilePath).toBe('/proj/a.txt');
    expect(result.current.documentText).toBe('merged-from-b');
  });

  it('openFile clears prior tabs when picked file parent differs from workspace root', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('legacy');
    vi.mocked(pickFile).mockResolvedValueOnce('/new-ws/file.txt');
    vi.mocked(readUserFile).mockResolvedValueOnce('fresh');

    const { result } = renderHook(() => useSessionAndLocation(), {
      wrapper: createWrapper('/ide?root=/old-ws'),
    });

    await act(async () => {
      await result.current.session.openFileFromWorkspace('/old-ws/legacy.txt');
    });

    expect(result.current.session.openFilePaths).toEqual(['/old-ws/legacy.txt']);

    await act(async () => {
      await result.current.session.runFileMenuAction('openFile');
    });

    expect(result.current.session.openFilePaths).toEqual(['/new-ws/file.txt']);
    expect(result.current.session.activeFilePath).toBe('/new-ws/file.txt');
    expect(result.current.session.documentText).toBe('fresh');
    expect(decodeURIComponent(result.current.loc.search)).toContain('root=/new-ws');
  });
});

describe('IdeSessionContext editor groups (per-group API)', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    vi.mocked(readWorkspaceUserFile).mockResolvedValue('');
    vi.mocked(loadPreferences).mockResolvedValue(DEFAULT_APP_PREFERENCES);
    vi.mocked(savePreferences).mockResolvedValue(undefined);
  });

  it('exposes a single default group whose projection mirrors the active group', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('a').mockResolvedValueOnce('b');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    expect(result.current.editorGroups).toHaveLength(1);
    expect(result.current.activeGroupId).toBe(result.current.editorGroups[0]!.id);

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/a.txt');
    });
    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/b.txt');
    });

    const g = result.current.editorGroups[0]!;
    expect(g.openTabs).toEqual(['/proj/a.txt', '/proj/b.txt']);
    expect(g.activeFilePath).toBe('/proj/b.txt');
    // The projection matches the top-level active-group fields.
    expect(g.openTabs).toEqual(result.current.openFilePaths);
    expect(g.activeFilePath).toBe(result.current.activeFilePath);
  });

  it('getBuffer / writeBuffer round-trip and writeBuffer mirrors documentText', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('hello');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/a.txt');
    });
    expect(result.current.getBuffer('/proj/a.txt')).toBe('hello');
    expect(result.current.getBuffer('/proj/missing.txt')).toBe('');

    await act(async () => {
      result.current.writeBuffer('/proj/a.txt', 'edited');
    });
    expect(result.current.getBuffer('/proj/a.txt')).toBe('edited');
    // /proj/a.txt is the active group's active file → documentText mirror updates.
    expect(result.current.documentText).toBe('edited');
    expect(result.current.dirtyFilePaths).toEqual(['/proj/a.txt']);
  });

  it('isBinaryPath reflects binary tabs', async () => {
    vi.mocked(readWorkspaceUserFile).mockRejectedValueOnce(new Error('not utf-8'));
    vi.mocked(getWorkspaceFileSize).mockResolvedValueOnce(2048);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/app.exe');
    });

    expect(result.current.isBinaryPath('/proj/app.exe')).toBe(true);
    expect(result.current.isBinaryPath('/proj/text.txt')).toBe(false);
  });

  it('per-group mutators on the active group match their active-group counterparts', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('a').mockResolvedValueOnce('b');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/a.txt');
    });
    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/b.txt');
    });

    const gid = result.current.activeGroupId;

    // activateFileInGroup switches the active file + documentText.
    await act(async () => {
      result.current.activateFileInGroup(gid, '/proj/a.txt');
    });
    expect(result.current.activeFilePath).toBe('/proj/a.txt');
    expect(result.current.documentText).toBe('a');

    // reorderFilesInGroup reorders within the group.
    await act(async () => {
      result.current.reorderFilesInGroup(gid, 0, 1);
    });
    expect(result.current.openFilePaths).toEqual(['/proj/b.txt', '/proj/a.txt']);

    // closeFileInGroup removes the tab (clean buffer → no confirm).
    await act(async () => {
      result.current.closeFileInGroup(gid, '/proj/b.txt');
    });
    expect(result.current.openFilePaths).toEqual(['/proj/a.txt']);
  });

  it('closeFileInGroup honors the unsaved-changes confirm', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('saved');
    const confirmSpy = vi.spyOn(window, 'confirm').mockReturnValue(false);

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/dirty.txt');
    });
    await act(async () => {
      result.current.writeBuffer('/proj/dirty.txt', 'unsaved');
    });
    await act(async () => {
      result.current.closeFileInGroup(result.current.activeGroupId, '/proj/dirty.txt');
    });

    expect(confirmSpy).toHaveBeenCalled();
    expect(result.current.openFilePaths).toEqual(['/proj/dirty.txt']);
    confirmSpy.mockRestore();
  });

  it('focusEditorGroup is a no-op for the already-active single group', async () => {
    vi.mocked(readWorkspaceUserFile).mockResolvedValueOnce('a');

    const { result } = renderHook(() => useIdeSession(), {
      wrapper: createWrapper('/ide?root=/proj'),
    });

    await act(async () => {
      await result.current.openFileFromWorkspace('/proj/a.txt');
    });

    const gid = result.current.activeGroupId;
    const before = result.current.editorGroups;
    await act(async () => {
      result.current.focusEditorGroup(gid);
    });
    expect(result.current.activeGroupId).toBe(gid);
    // Same projection reference — focusing the active group changes nothing.
    expect(result.current.editorGroups).toBe(before);
  });
});
