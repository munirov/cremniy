import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { MemoryRouter, Route, Routes, useSearchParams } from 'react-router-dom';
import { describe, expect, it, vi, beforeEach } from 'vitest';

import {
  loadPreferences,
  savePreferences,
} from '@infrastructure/preferences/preferencesBridge';
import { pickFolder, createProjectFolder } from '@infrastructure/tauri/bridge';
import { DEFAULT_APP_PREFERENCES } from '@domain/preferences/appPreferences';

import { WelcomeView } from './WelcomeView';

const MOCK_RECENT_PATHS = [
  'C:/Users/demo/Documents/project-alpha',
  'D:/work/cremniy-sample',
  'C:/Projects/stm32-template',
] as const;

vi.mock('@infrastructure/tauri/bridge', () => ({
  pickFolder: vi.fn(),
  createProjectFolder: vi.fn(),
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
    vi.mocked(createProjectFolder).mockReset();
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

  it('shows recent workspace paths once preferences load', async () => {
    renderWelcome();
    await waitFor(() => {
      for (const path of MOCK_RECENT_PATHS) {
        expect(screen.getByRole('option', { name: path })).toBeInTheDocument();
      }
    });
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
      expect(screen.getByText(/no recent workspaces yet/i)).toBeInTheDocument();
    });
    expect(screen.queryByRole('option')).not.toBeInTheDocument();
  });

  it('renders Open, Open..., and Create buttons', async () => {
    renderWelcome();
    await waitFor(() => {
      expect(screen.getByRole('button', { name: /^Open$/ })).toBeInTheDocument();
    });
    expect(screen.getByRole('button', { name: 'Open...' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Create' })).toBeInTheDocument();
  });

  it('navigates to /ide with decoded root query when Open... resolves a folder path', async () => {
    const user = userEvent.setup();
    vi.mocked(pickFolder).mockResolvedValueOnce('D:/workspace/myproject');

    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Open...' })).toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: 'Open...' }));

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent('D:/workspace/myproject');
    });

    expect(pickFolder).toHaveBeenCalledTimes(1);
    expect(savePreferences).toHaveBeenCalled();
  });

  it('navigates to /ide with root when Open is used on a selected recent path', async () => {
    const user = userEvent.setup();
    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('option', { name: MOCK_RECENT_PATHS[0] })).toBeInTheDocument();
    });

    await user.click(screen.getByRole('option', { name: MOCK_RECENT_PATHS[0] }));
    await user.click(screen.getByRole('button', { name: /^Open$/ }));

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent(MOCK_RECENT_PATHS[0]);
    });

    expect(savePreferences).toHaveBeenCalled();
  });

  it('opens workspace on double-click of a recent path', async () => {
    const user = userEvent.setup();
    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('option', { name: MOCK_RECENT_PATHS[1] })).toBeInTheDocument();
    });

    await user.dblClick(screen.getByRole('option', { name: MOCK_RECENT_PATHS[1] }));

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent(MOCK_RECENT_PATHS[1]);
    });

    expect(savePreferences).toHaveBeenCalled();
  });

  it('opens first workspace when listbox is focused and ArrowDown then Enter are used', async () => {
    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('listbox', { name: /recent workspaces/i })).toBeInTheDocument();
    });

    const list = screen.getByRole('listbox', { name: /recent workspaces/i });
    list.focus();
    fireEvent.keyDown(list, { key: 'ArrowDown' });
    fireEvent.keyDown(list, { key: 'Enter' });

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent(MOCK_RECENT_PATHS[0]);
    });
  });

  it('does not navigate when folder picker returns null', async () => {
    vi.mocked(pickFolder).mockResolvedValueOnce(null);

    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Open...' })).toBeInTheDocument();
    });

    fireEvent.click(screen.getByRole('button', { name: 'Open...' }));

    await waitFor(() => {
      expect(pickFolder).toHaveBeenCalled();
    });

    expect(screen.queryByTestId('root-param')).not.toBeInTheDocument();
    expect(screen.getByRole('heading', { name: /cremniy/i })).toBeInTheDocument();
  });
});
