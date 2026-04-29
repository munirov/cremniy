import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { MemoryRouter, Route, Routes, useSearchParams } from 'react-router-dom';
import { describe, expect, it, vi, beforeEach } from 'vitest';

import {
  loadPreferences,
  savePreferences,
} from '@infrastructure/preferences/preferencesBridge';
import { pickFolder } from '@infrastructure/tauri/bridge';

import { DEMO_RECENT_WORKSPACE_PATHS, WelcomeView } from './WelcomeView';

vi.mock('@infrastructure/tauri/bridge', () => ({
  pickFolder: vi.fn(),
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
    vi.mocked(loadPreferences).mockReset();
    vi.mocked(savePreferences).mockReset();
    vi.mocked(loadPreferences).mockResolvedValue({
      theme: 'dark',
      recentWorkspacePaths: [...DEMO_RECENT_WORKSPACE_PATHS],
    });
    vi.mocked(savePreferences).mockResolvedValue(undefined);
  });

  it('shows recent workspace paths once preferences load', async () => {
    renderWelcome();
    await waitFor(() => {
      for (const path of DEMO_RECENT_WORKSPACE_PATHS) {
        expect(screen.getByRole('button', { name: path })).toBeInTheDocument();
      }
    });
  });

  it('renders Open, Open..., and Create buttons', async () => {
    renderWelcome();
    await waitFor(() => {
      expect(screen.getByRole('button', { name: /^Open$/ })).toBeInTheDocument();
    });
    expect(screen.getByRole('button', { name: 'Open...' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Create' })).toBeInTheDocument();
  });

  it('renders link to IDE route', async () => {
    renderWelcome();
    await waitFor(() => {
      expect(screen.getByRole('link', { name: /open ide \(mock\)/i })).toBeInTheDocument();
    });
    const link = screen.getByRole('link', { name: /open ide \(mock\)/i });
    expect(link).toHaveAttribute('href', '/ide');
  });

  it('navigates to /ide with decoded root query when Open... resolves a folder path', async () => {
    vi.mocked(pickFolder).mockResolvedValueOnce('D:/workspace/myproject');

    renderWelcomeWithIdeRoute();

    await waitFor(() => {
      expect(screen.getByRole('button', { name: 'Open...' })).toBeInTheDocument();
    });

    fireEvent.click(screen.getByRole('button', { name: 'Open...' }));

    await waitFor(() => {
      expect(screen.getByTestId('root-param')).toHaveTextContent('D:/workspace/myproject');
    });

    expect(pickFolder).toHaveBeenCalledTimes(1);
    expect(savePreferences).toHaveBeenCalled();
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
