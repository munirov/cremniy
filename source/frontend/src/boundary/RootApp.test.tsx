import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { MemoryRouter, Route, Routes } from 'react-router-dom';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import { DEFAULT_APP_PREFERENCES, type AppPreferences } from '@domain/preferences/appPreferences';
import type { SettingsService } from '@domain/preferences/settingsService';

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

// The terminal panel mounts real xterm instances (canvas), which jsdom can't
// drive. Stub it with a marker div so we can assert the panel's presence and
// the workspace root it receives without paying the xterm cost.
vi.mock('@boundary/terminal/TerminalFooterPanel', () => ({
  TerminalFooterPanel: ({ workspaceRoot }: { workspaceRoot: string | null }) => (
    <div data-testid="terminal-panel">Terminal panel {workspaceRoot ?? 'no workspace'}</div>
  ),
}));

// The real Files tree (rendered inside the dock) reads excluded-file patterns
// from preferences over the Tauri bridge on mount. Tauri's `invoke` isn't
// available under jsdom, so stub the bridge to resolve defaults — otherwise it
// throws an unhandled rejection. RootApp drives its own prefs through the
// injected settingsService mock, so this only affects the file tree.
vi.mock('@infrastructure/preferences/preferencesBridge', async () => {
  const { DEFAULT_APP_PREFERENCES: defaults } = await import(
    '@domain/preferences/appPreferences'
  );
  return {
    loadPreferences: vi.fn().mockResolvedValue(defaults),
    savePreferences: vi.fn().mockResolvedValue(undefined),
  };
});

import { MenuSlotProvider, useMenuSlot } from './chrome/MenuSlotContext';
import { TitleBar } from './chrome/TitleBar';
import { NotificationProvider } from './notifications/NotificationContext';
import { WorkspaceProvider } from './workspace/WorkspaceContext';
import { RootApp } from './RootApp';
import toolsPlugin from '@plugins/tools';
import { registerPlugin } from '@shared/plugins/registry';

const mockLoadPreferences = vi.fn<SettingsService['loadPreferences']>();
const mockSavePreferences = vi.fn<SettingsService['savePreferences']>();

const settingsService: SettingsService = {
  loadPreferences: mockLoadPreferences,
  savePreferences: mockSavePreferences,
  testObjdumpTool: vi.fn(),
  exportPreferences: vi.fn().mockResolvedValue(null),
  importPreferences: vi.fn().mockResolvedValue(null),
};

/**
 * Mirrors App.tsx's ChromeShell: the menu RootApp publishes into the MenuSlot
 * is rendered by the TitleBar one level above the route, and the titlebar gear
 * is wired to RootApp's settings action. Without this, the menu lives nowhere
 * (the menu bar moved out of RootApp into the global titlebar).
 */
function ChromeShell({ initialEntry }: { initialEntry: string }) {
  const { menu, settingsAction } = useMenuSlot();
  return (
    <div>
      <TitleBar menu={menu} onOpenSettings={settingsAction ?? undefined} />
      <Routes>
        <Route path="/ide" element={<RootApp settingsService={settingsService} />} />
        <Route path="/" element={<main>Welcome</main>} />
      </Routes>
      <span data-testid="route-probe">{initialEntry}</span>
    </div>
  );
}

function renderIde(initialEntry: string) {
  return render(
    <MemoryRouter initialEntries={[initialEntry]}>
      <NotificationProvider>
        <WorkspaceProvider>
          <MenuSlotProvider>
            <ChromeShell initialEntry={initialEntry} />
          </MenuSlotProvider>
        </WorkspaceProvider>
      </NotificationProvider>
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
    // The right-edge tool rail only renders when a tool plugin contributes
    // icons; register Binary Tools so the shell-regions test sees the rail.
    registerPlugin(toolsPlugin);
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

  it('renders the IDE shell regions (editor area, Files pane, tool rail, terminal)', async () => {
    renderIde('/ide');

    // RootLayout wraps the dock in a labelled "Editor area" region; the dock
    // builds a Files pane and the right-edge tool selector rail.
    expect(screen.getByRole('region', { name: /editor area/i })).toBeInTheDocument();
    expect(screen.getByRole('region', { name: /^files$/i })).toBeInTheDocument();
    expect(screen.getByRole('tablist', { name: /tool selector/i })).toBeInTheDocument();
    expect(await screen.findByTestId('terminal-panel')).toBeInTheDocument();

    // The main menu bar RootApp publishes is now hosted by the titlebar slot.
    expect(screen.getByRole('navigation', { name: /main menu/i })).toBeInTheDocument();
  });

  it('shows the idle editor welcome card when no file is open', () => {
    renderIde('/ide');

    // No active file + empty scratch buffer → IdeMonacoEditor renders the idle
    // card (quick-action shortcut rows), not a Monaco surface.
    expect(screen.getByText('Open file')).toBeInTheDocument();
    expect(screen.getByText('Open folder')).toBeInTheDocument();
    // Falls back to the empty Files state until a workspace is opened.
    expect(screen.getByText(/no workspace folder open/i)).toBeInTheDocument();
  });

  it('shows the workspace path in the Files pane header from the root query', async () => {
    const rootPath = 'C:\\Projects\\demo-app';
    renderIde(`/ide?root=${encodeURIComponent(rootPath)}`);

    // The Files pane is titled "Files — <root>" (its aria-label), and the
    // mocked terminal panel (visible once prefs load) receives the same root.
    expect(
      screen.getByRole('region', {
        name: new RegExp(`files — ${rootPath.replace(/\\/g, '\\\\')}`, 'i'),
      }),
    ).toBeInTheDocument();
    expect(await screen.findByTestId('terminal-panel')).toHaveTextContent(rootPath);
  });

  it('navigates to Welcome when File → Close workspace is chosen', () => {
    renderIde('/ide');

    fireEvent.click(screen.getByRole('button', { name: /^file$/i }));
    fireEvent.click(screen.getByRole('menuitem', { name: /close workspace/i }));

    expect(screen.getByText('Welcome')).toBeInTheDocument();
  });

  it('opens Settings as a center tab from the titlebar gear', async () => {
    renderIde('/ide');
    // Let the prefs load settle so RootApp has published its titlebar settings
    // action (the gear is wired in an effect after the first paint).
    await screen.findByTestId('terminal-panel');

    // The titlebar gear opens Settings in the generic center "tab space"
    // (a tab, not a modal). The tab strip only appears once a panel is open.
    fireEvent.click(screen.getByRole('button', { name: /^settings$/i }));

    expect(await screen.findByRole('tab', { name: /^settings$/i })).toBeInTheDocument();
  });

  it('toggles the live terminal panel from View and persists the preference', async () => {
    const rootPath = 'C:\\Projects\\demo-app';
    renderIde(`/ide?root=${encodeURIComponent(rootPath)}`);

    expect(await screen.findByTestId('terminal-panel')).toHaveTextContent(rootPath);

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    const checkedToggle = screen.getByRole('menuitemcheckbox', { name: /terminal panel/i });
    expect(checkedToggle).toHaveAttribute('aria-checked', 'true');
    fireEvent.click(checkedToggle);

    await waitFor(() => {
      expect(screen.queryByTestId('terminal-panel')).not.toBeInTheDocument();
    });
    expect(mockSavePreferences).toHaveBeenLastCalledWith({
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [],
      terminalPanelVisible: false,
    });

    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    const uncheckedToggle = screen.getByRole('menuitemcheckbox', { name: /terminal panel/i });
    expect(uncheckedToggle).toHaveAttribute('aria-checked', 'false');
    fireEvent.click(uncheckedToggle);

    expect(await screen.findByTestId('terminal-panel')).toHaveTextContent(rootPath);
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

  it('toggles word wrap from the View menu and persists the preference', async () => {
    renderIde('/ide');

    // Word wrap defaults on; the View toggle reflects that and persists the flip.
    fireEvent.click(screen.getByRole('button', { name: /^view$/i }));
    const wrapToggle = screen.getByRole('menuitemcheckbox', { name: /word wrap/i });
    expect(wrapToggle).toHaveAttribute('aria-checked', 'true');
    fireEvent.click(wrapToggle);

    await waitFor(() => {
      expect(mockSavePreferences).toHaveBeenLastCalledWith(
        expect.objectContaining({ editorWordWrap: false }),
      );
    });
  });
});
