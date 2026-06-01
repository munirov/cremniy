import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { MemoryRouter, Route, Routes } from 'react-router-dom';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import { DEFAULT_APP_PREFERENCES, type AppPreferences } from '@domain/preferences/appPreferences';
import type { SettingsService } from '@domain/preferences/settingsService';

import ideStyles from './layout/IdeWorkspace.module.css';

vi.mock('@monaco-editor/react', () => ({
  default: ({
    value,
    onChange,
    options,
  }: {
    value?: string;
    onChange?: (v: string | undefined) => void;
    options?: { ariaLabel?: string; wordWrap?: string };
  }) => (
    <textarea
      aria-label={options?.ariaLabel ?? 'Active document'}
      value={value ?? ''}
      data-word-wrap={options?.wordWrap ?? ''}
      onChange={(e) => onChange?.(e.target.value)}
      spellCheck={false}
    />
  ),
}));

vi.mock('@boundary/terminal/TerminalFooterPanel', () => ({
  TerminalFooterPanel: ({ workspaceRoot }: { workspaceRoot: string | null }) => (
    <div data-testid="terminal-panel">Terminal panel {workspaceRoot ?? 'no workspace'}</div>
  ),
}));

import { WorkspaceProvider } from './workspace/WorkspaceContext';
import { RootApp } from './RootApp';

const mockLoadPreferences = vi.fn<SettingsService['loadPreferences']>();
const mockSavePreferences = vi.fn<SettingsService['savePreferences']>();

const settingsService: SettingsService = {
  loadPreferences: mockLoadPreferences,
  savePreferences: mockSavePreferences,
  testObjdumpTool: vi.fn(),
  exportPreferences: vi.fn().mockResolvedValue(null),
  importPreferences: vi.fn().mockResolvedValue(null),
};

function renderIde(initialEntry: string) {
  return render(
    <MemoryRouter initialEntries={[initialEntry]}>
      <WorkspaceProvider>
        <Routes>
          <Route path="/ide" element={<RootApp settingsService={settingsService} />} />
          <Route path="/" element={<main>Welcome</main>} />
        </Routes>
      </WorkspaceProvider>
    </MemoryRouter>,
  );
}

function deferred<T>() {
  let resolve!: (value: T) => void;
  let reject!: (reason?: unknown) => void;
  const promise = new Promise<T>((res, rej) => {
    resolve = res;
    reject = rej;
  });
  return { promise, resolve, reject };
}

describe('RootApp', () => {
  beforeEach(() => {
    mockLoadPreferences.mockReset();
    mockSavePreferences.mockReset();
    mockLoadPreferences.mockResolvedValue({
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [],
      terminalPanelVisible: true,
    });
    mockSavePreferences.mockResolvedValue(undefined);
  });

  it('navigates to Welcome when Close workspace is activated', () => {
    renderIde('/ide');

    fireEvent.click(screen.getByRole('button', { name: /close workspace/i }));

    expect(screen.getByText('Welcome')).toBeInTheDocument();
  });

  it('renders document editor shell', () => {
    renderIde('/ide');
    expect(screen.getByRole('textbox', { name: /active document/i })).toBeInTheDocument();
    expect(screen.getByTestId('ide-status-strip')).toBeInTheDocument();
    expect(screen.getByText('Encoding: —')).toBeInTheDocument();
    expect(screen.getByText(/Ln —, Col —/)).toBeInTheDocument();
  });

  it('shows workspace path from root query in Files sidebar', () => {
    const rootPath = 'C:\\Projects\\demo-app';
    renderIde(`/ide?root=${encodeURIComponent(rootPath)}`);

    expect(screen.getByText(/Workspace:/)).toHaveTextContent(`Workspace: ${rootPath}`);
  });

  it('shows placeholder workspace label when root query is absent', () => {
    renderIde('/ide');

    expect(screen.getByText(/Workspace:/)).toHaveTextContent('Workspace: —');
  });

  it('exposes shell landmarks (banner, main, Terminal panel region)', () => {
    const { container } = renderIde('/ide');
    expect(screen.getByRole('banner')).toBeInTheDocument();
    expect(screen.getByRole('main')).toBeInTheDocument();
    expect(screen.getByTestId('ide-terminal-footer')).toBeInTheDocument();

    expect(container.querySelectorAll('header')).toHaveLength(1);
    expect(container.querySelectorAll('main')).toHaveLength(1);
    expect(container.querySelectorAll('footer')).toHaveLength(1);
    expect(container.querySelectorAll('aside')).toHaveLength(2);
  });

  it('includes expected semantic shell regions via section label', () => {
    renderIde('/ide');
    expect(screen.getByRole('region', { name: /editor area/i })).toBeInTheDocument();
  });

  it('renders IDE regions (Files sidebar, tabs, editor)', async () => {
    renderIde('/ide');

    const filesAside = screen.getByRole('complementary', { name: /^files$/i });
    expect(filesAside).toHaveTextContent(/no workspace folder open/i);

    expect(filesAside).toHaveClass(ideStyles.filesSidebar);

    expect(screen.getByRole('region', { name: /document tabs/i })).toBeInTheDocument();
    expect(screen.getByRole('complementary', { name: /tool tabs/i })).toBeInTheDocument();
    expect(await screen.findByTestId('terminal-panel')).toBeInTheDocument();
  });

  it('toggles the live terminal panel from View without closing Settings', async () => {
    const rootPath = 'C:\\Projects\\demo-app';
    renderIde(`/ide?root=${encodeURIComponent(rootPath)}`);

    expect(await screen.findByTestId('terminal-panel')).toHaveTextContent(rootPath);

    fireEvent.click(screen.getByRole('button', { name: /^file$/i }));
    fireEvent.click(screen.getByRole('menuitem', { name: /preferences/i }));
    expect(screen.getByRole('dialog', { name: /preferences/i })).toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    const checkedTerminalToggle = screen.getByRole('menuitemcheckbox', { name: /terminal panel/i });
    expect(checkedTerminalToggle).toHaveAttribute('aria-checked', 'true');
    fireEvent.click(checkedTerminalToggle);

    await waitFor(() => {
      expect(screen.queryByTestId('terminal-panel')).not.toBeInTheDocument();
    });
    expect(screen.getByRole('dialog', { name: /preferences/i })).toBeInTheDocument();
    expect(mockSavePreferences).toHaveBeenLastCalledWith({
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [],
      terminalPanelVisible: false,
    });

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    const uncheckedTerminalToggle = screen.getByRole('menuitemcheckbox', { name: /terminal panel/i });
    expect(uncheckedTerminalToggle).toHaveAttribute('aria-checked', 'false');
    fireEvent.click(uncheckedTerminalToggle);

    expect(await screen.findByTestId('terminal-panel')).toHaveTextContent(rootPath);
    expect(screen.getByRole('dialog', { name: /preferences/i })).toBeInTheDocument();
    expect(mockSavePreferences).toHaveBeenLastCalledWith({
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [],
      terminalPanelVisible: true,
    });
  });

  it('keeps the terminal panel hidden from saved preferences until View toggles it on', async () => {
    const preferences = deferred<AppPreferences>();
    mockLoadPreferences.mockReturnValueOnce(preferences.promise);
    const hiddenPreferences: AppPreferences = {
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [],
      terminalPanelVisible: false,
    };
    const rootPath = 'C:\\Projects\\demo-app';
    renderIde(`/ide?root=${encodeURIComponent(rootPath)}`);

    expect(screen.queryByTestId('terminal-panel')).not.toBeInTheDocument();
    await waitFor(() => {
      expect(mockLoadPreferences).toHaveBeenCalledTimes(1);
    });

    preferences.resolve(hiddenPreferences);
    await waitFor(() => {
      expect(screen.queryByTestId('terminal-panel')).not.toBeInTheDocument();
    });

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    const terminalToggle = screen.getByRole('menuitemcheckbox', { name: /terminal panel/i });
    expect(terminalToggle).toHaveAttribute('aria-checked', 'false');
    fireEvent.click(terminalToggle);

    expect(await screen.findByTestId('terminal-panel')).toHaveTextContent(rootPath);
    expect(mockSavePreferences).toHaveBeenLastCalledWith({
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [],
      terminalPanelVisible: true,
    });
  });

  it('toggles Monaco word wrap from the View menu and persists preference', async () => {
    renderIde('/ide');

    const editor = screen.getByRole('textbox', { name: /active document/i });
    expect(editor).toHaveAttribute('data-word-wrap', 'on');

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    fireEvent.click(screen.getByRole('menuitemcheckbox', { name: /word wrap/i }));

    await waitFor(() => {
      expect(editor).toHaveAttribute('data-word-wrap', 'off');
    });

    await waitFor(() => {
      expect(mockSavePreferences).toHaveBeenLastCalledWith(
        expect.objectContaining({ editorWordWrap: false }),
      );
    });
  });
});
