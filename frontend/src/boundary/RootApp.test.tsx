import { fireEvent, render, screen } from '@testing-library/react';
import { MemoryRouter, Route, Routes } from 'react-router-dom';
import { describe, expect, it } from 'vitest';

import ideStyles from './layout/IdeWorkspace.module.css';
import { WorkspaceProvider } from './workspace/WorkspaceContext';
import { RootApp } from './RootApp';

function renderIde(initialEntry: string) {
  return render(
    <MemoryRouter initialEntries={[initialEntry]}>
      <WorkspaceProvider>
        <Routes>
          <Route path="/ide" element={<RootApp />} />
          <Route path="/" element={<main>Welcome</main>} />
        </Routes>
      </WorkspaceProvider>
    </MemoryRouter>,
  );
}

describe('RootApp', () => {
  it('navigates to Welcome when Close workspace is activated', () => {
    renderIde('/ide');

    fireEvent.click(screen.getByRole('button', { name: /close workspace/i }));

    expect(screen.getByText('Welcome')).toBeInTheDocument();
  });

  it('renders Cremniy title', () => {
    renderIde('/ide');
    expect(screen.getByRole('heading', { level: 1 })).toHaveTextContent('Cremniy UI');
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
    expect(screen.getByRole('contentinfo', { name: /terminal/i })).toBeInTheDocument();

    expect(container.querySelectorAll('header')).toHaveLength(1);
    expect(container.querySelectorAll('main')).toHaveLength(1);
    expect(container.querySelectorAll('footer')).toHaveLength(1);
    expect(container.querySelectorAll('aside')).toHaveLength(1);
  });

  it('includes expected semantic shell regions via section label', () => {
    renderIde('/ide');
    expect(screen.getByRole('region', { name: /editor area/i })).toBeInTheDocument();
  });

  it('renders IDE regions (Files sidebar, tabs, editor)', () => {
    renderIde('/ide');

    const filesAside = screen.getByRole('complementary', { name: /^files$/i });
    expect(filesAside).toHaveTextContent(/files tree placeholder/i);

    expect(filesAside).toHaveClass(ideStyles.filesSidebar);

    expect(screen.getByRole('region', { name: /open tabs placeholder/i })).toBeInTheDocument();
    expect(screen.getByRole('region', { name: /^editor$/i })).toBeInTheDocument();
    expect(screen.getByRole('contentinfo', { name: /terminal/i })).toHaveTextContent(/terminal placeholder/i);
  });
});
