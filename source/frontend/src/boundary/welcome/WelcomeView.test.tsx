import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { MemoryRouter, Route, Routes, useSearchParams } from 'react-router-dom';
import { describe, expect, it, vi, beforeEach } from 'vitest';

import {
  loadPreferences,
  savePreferences,
} from '@infrastructure/preferences/preferencesBridge';
import { pickFolder, pickFile, listDirectoryEntries } from '@infrastructure/tauri/bridge';
import { DEFAULT_APP_PREFERENCES } from '@domain/preferences/appPreferences';

import { WelcomeView } from './WelcomeView';

const MOCK_RECENT_PATHS = [
  'C:/Users/demo/Documents/project-alpha',
  'D:/work/cremniy-sample',
  'C:/Projects/stm32-template',
] as const;

// Safety net for the prune-on-load effect: it re-imports the bridge via dynamic
// `import()` and calls listDirectoryEntries on each recent path to check the
// folder still exists. Vite can hand back an un-mocked bridge instance for those
// concurrent dynamic imports, whose listDirectoryEntries falls through to the
// real invoke; stub invoke so that path resolves instead of throwing and pruning
// every recent away.
vi.mock('@tauri-apps/api/core', () => ({
  invoke: vi.fn(() => Promise.resolve([])),
}));

vi.mock('@infrastructure/tauri/bridge', () => ({
  pickFolder: vi.fn(),
  pickFile: vi.fn(),
  // WelcomeView prunes recent paths whose folder no longer exists by calling
  // listDirectoryEntries on each one (a throw means "gone"). Resolve so all
  // mocked recents survive into the rendered list.
  listDirectoryEntries: vi.fn(() => Promise.resolve([])),
  createCremniyProject: vi.fn(),
  gitClone: vi.fn(),
  gitSaveCredentials: vi.fn(),
}));

vi.mock('@infrastructure/preferences/preferencesBridge', () => ({
  loadPreferences: vi.fn(),
  savePreferences: vi.fn(),
}));

function IdeCapture() {
  const [params] = useSearchParams();
  return <span data-testid="root-param">{params.get('root') ?? ''}</span>;
}

function renderWelcomeWithIdeRoute() {
  return render(
    <MemoryRouter initialEntries={['/']}>
      <Routes>
        <Route path="/" element={<WelcomeView />} />
        <Route path="/ide" element={<IdeCapture />} />
      </Routes>
    </MemoryRouter>,
  );
}

function renderWelcome() {
  return render(
    <MemoryRouter>
      <WelcomeView />
    </MemoryRouter>,
  );
}

describe('WelcomeView', () => {
  beforeEach(() => {
    vi.mocked(pickFolder).mockReset();
    vi.mocked(pickFile).mockReset();
    vi.mocked(listDirectoryEntries).mockReset();
    vi.mocked(listDirectoryEntries).mockResolvedValue([]);
    vi.mocked(loadPreferences).mockReset();
    vi.mocked(savePreferences).mockReset();
    vi.mocked(loadPreferences).mockResolvedValue({
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [...MOCK_RECENT_PATHS],
      terminalPanelVisible: true,
    });
    vi.mocked(savePreferences).mockResolvedValue(undefined);
  });

  it('renders the hero and the action cards', async () => {
    renderWelcome();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Open project' })).toBeInTheDocument();
    });
    expect(screen.getByText('CREMNIY')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'New project' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Open file' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Clone from Git' })).toBeInTheDocument();
  });

  it('shows recent workspace paths once preferences load', async () => {
    renderWelcome();

    await waitFor(() => {
      expect(screen.getAllByRole('option')).toHaveLength(MOCK_RECENT_PATHS.length);
    });
    // Each recent row shows the folder name plus its parent path (not the full
    // path as the accessible name).
    expect(screen.getByText('project-alpha')).toBeInTheDocument();
    expect(screen.getByText('cremniy-sample')).toBeInTheDocument();
    expect(screen.getByText('stm32-template')).toBeInTheDocument();
    expect(screen.getByText('C:/Users/demo/Documents')).toBeInTheDocument();
  });

  it('shows empty state when there are no recent workspaces', async () => {
    vi.mocked(loadPreferences).mockResolvedValueOnce({
      ...DEFAULT_APP_PREFERENCES,
      theme: 'dark',
      recentWorkspacePaths: [],
      terminalPanelVisible: true,
    });
    renderWelcome();

    await waitFor(() => {
      expect(screen.getByText(/no recent projects yet/i)).toBeInTheDocument();
    });
    expect(screen.queryByRole('option')).not.toBeInTheDocument();
  });

  it('opens the folder picker and navigates to /ide with the chosen root on "Open project"', async () => {
    const user = userEvent.setup();
    vi.mocked(pickFolder).mockResolvedValueOnce('D:/workspace/myproject');

    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Open project' })).toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: 'Open project' }));

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent('D:/workspace/myproject');
    });

    expect(pickFolder).toHaveBeenCalledTimes(1);
    expect(savePreferences).toHaveBeenCalled();
  });

  it('opens the file picker and navigates to the file\'s parent folder on "Open file"', async () => {
    const user = userEvent.setup();
    vi.mocked(pickFile).mockResolvedValueOnce('D:/workspace/myproject/main.c');

    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Open file' })).toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: 'Open file' }));

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent('D:/workspace/myproject');
    });

    expect(pickFile).toHaveBeenCalledTimes(1);
    expect(savePreferences).toHaveBeenCalled();
  });

  it('opens a recent workspace on double-click', async () => {
    const user = userEvent.setup();
    renderWelcomeWithIdeRoute();

    // Wait for the prefs-load + prune pass to settle into the full recent list
    // (all rows present, not a transient pruned subset) before interacting.
    await waitFor(() => {
      expect(screen.getAllByRole('option')).toHaveLength(MOCK_RECENT_PATHS.length);
    });
    const options = screen.getAllByRole('option');
    expect(options[1]).toHaveTextContent('cremniy-sample');

    await user.dblClick(options[1]);

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent(MOCK_RECENT_PATHS[1]);
    });

    expect(savePreferences).toHaveBeenCalled();
  });

  it('selects a recent path on single click without navigating', async () => {
    const user = userEvent.setup();
    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByText('project-alpha')).toBeInTheDocument();
    });

    const options = screen.getAllByRole('option');
    await user.click(options[0]);

    expect(options[0]).toHaveAttribute('aria-selected', 'true');
    // A single click only selects; it must not leave the welcome screen.
    expect(screen.queryByTestId('root-param')).not.toBeInTheDocument();
  });

  it('opens the selected workspace when the listbox is focused and ArrowDown then Enter are used', async () => {
    const user = userEvent.setup();
    renderWelcomeWithIdeRoute();

    // The ArrowDown handler bails when the recent list is still empty, so wait
    // for the prefs-load + prune pass to populate every row before keying. If
    // we key too early the selection never lands and the test flakes.
    await waitFor(() => {
      expect(screen.getAllByRole('option')).toHaveLength(MOCK_RECENT_PATHS.length);
    });

    const list = screen.getByRole('listbox', { name: /recent workspaces/i });
    list.focus();

    // ArrowDown highlights the first row; Enter reads that index off a ref that
    // only refreshes on the next render. Wait for the selection to land (same
    // render that refreshes the ref) before pressing Enter.
    await user.keyboard('{ArrowDown}');
    await waitFor(() => {
      expect(screen.getAllByRole('option')[0]).toHaveAttribute('aria-selected', 'true');
    });

    await user.keyboard('{Enter}');

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent(MOCK_RECENT_PATHS[0]);
    });
  });

  it('does nothing when the folder picker returns null', async () => {
    vi.mocked(pickFolder).mockResolvedValueOnce(null);

    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Open project' })).toBeInTheDocument();
    });

    fireEvent.click(screen.getByRole('button', { name: 'Open project' }));

    await waitFor(() => {
      expect(pickFolder).toHaveBeenCalled();
    });

    expect(screen.queryByTestId('root-param')).not.toBeInTheDocument();
    // Still on the welcome screen.
    expect(screen.getByText('CREMNIY')).toBeInTheDocument();
  });

  it('switches to the create form on "New project" and can return via Back', async () => {
    const user = userEvent.setup();
    renderWelcome();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'New project' })).toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: 'New project' }));

    expect(screen.getByText('NEW PROJECT')).toBeInTheDocument();
    expect(screen.getByLabelText('Project name')).toBeInTheDocument();

    await user.click(screen.getByRole('button', { name: 'Back' }));

    expect(screen.getByText('CREMNIY')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Open project' })).toBeInTheDocument();
  });

  it('switches to the clone form on "Clone from Git" and can return via Back', async () => {
    const user = userEvent.setup();
    renderWelcome();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Clone from Git' })).toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: 'Clone from Git' }));

    expect(screen.getByText('CLONE REPOSITORY')).toBeInTheDocument();
    expect(screen.getByLabelText('URL')).toBeInTheDocument();

    await user.click(screen.getByRole('button', { name: 'Back' }));

    expect(screen.getByText('CREMNIY')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Clone from Git' })).toBeInTheDocument();
  });
});
