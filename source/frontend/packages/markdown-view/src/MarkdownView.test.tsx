import { fireEvent, render, screen, within } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { MarkdownView } from './MarkdownView';

describe('MarkdownView', () => {
  it('renders headings, emphasis, inline code and links', () => {
    render(
      <MarkdownView
        source={'# Title\n\nA **bold** and *italic* with `code` and [site](https://example.com).'}
      />,
    );
    expect(screen.getByRole('heading', { level: 1, name: 'Title' })).toBeInTheDocument();
    expect(screen.getByText('bold').tagName).toBe('STRONG');
    expect(screen.getByText('code').tagName).toBe('CODE');
    const link = screen.getByRole('link', { name: 'site' });
    expect(link).toHaveAttribute('href', 'https://example.com');
    expect(link).toHaveAttribute('target', '_blank');
  });

  it('renders a GFM table', () => {
    render(<MarkdownView source={'| Name | Qty |\n|------|-----|\n| Apple | 3 |\n| Pear | 5 |'} />);
    const table = screen.getByRole('table');
    expect(within(table).getByRole('columnheader', { name: 'Name' })).toBeInTheDocument();
    expect(within(table).getByRole('cell', { name: 'Apple' })).toBeInTheDocument();
    expect(within(table).getByRole('cell', { name: '5' })).toBeInTheDocument();
  });

  it('renders GFM task lists with checkboxes', () => {
    render(<MarkdownView source={'- [x] done\n- [ ] todo'} />);
    const boxes = screen.getAllByRole('checkbox');
    expect(boxes).toHaveLength(2);
    expect(boxes[0]).toBeChecked();
    expect(boxes[1]).not.toBeChecked();
  });

  it('renders GFM strikethrough', () => {
    render(<MarkdownView source={'this is ~~gone~~ now'} />);
    expect(screen.getByText('gone').tagName).toBe('DEL');
  });

  it('renders fenced code blocks', () => {
    render(<MarkdownView source={'```\nline1\nline2\n```'} />);
    expect(screen.getByText(/line1/).closest('pre')).not.toBeNull();
  });

  it('renders inline HTML (rehype-raw)', () => {
    render(<MarkdownView source={'<details><summary>More</summary>hidden body</details>'} />);
    expect(screen.getByText('More').tagName).toBe('SUMMARY');
    expect(screen.getByText('hidden body')).toBeInTheDocument();
  });

  it('maps image sources through transformImageUrl (markdown + inline HTML)', () => {
    render(
      <MarkdownView
        source={'![logo](img/logo.png)\n\n<img src="logos/x.svg" alt="x">'}
        transformImageUrl={(s) => `asset://r/${s}`}
      />,
    );
    expect(screen.getByRole('img', { name: 'logo' })).toHaveAttribute(
      'src',
      'asset://r/img/logo.png',
    );
    expect(screen.getByRole('img', { name: 'x' })).toHaveAttribute('src', 'asset://r/logos/x.svg');
  });

  it('calls onLinkClick with the href when a link is clicked', () => {
    const onLinkClick = vi.fn();
    render(<MarkdownView source={'see [the file](docs/readme.md)'} onLinkClick={onLinkClick} />);
    fireEvent.click(screen.getByRole('link', { name: 'the file' }));
    expect(onLinkClick).toHaveBeenCalledTimes(1);
    expect(onLinkClick.mock.calls[0]?.[0]).toBe('docs/readme.md');
  });

  it('gives headings GitHub-style slug ids (rehype-slug)', () => {
    render(<MarkdownView source={'## Install Guide\n\n### Pull Requests'} />);
    expect(screen.getByRole('heading', { level: 2, name: 'Install Guide' })).toHaveAttribute(
      'id',
      'install-guide',
    );
    expect(screen.getByRole('heading', { level: 3, name: 'Pull Requests' })).toHaveAttribute(
      'id',
      'pull-requests',
    );
  });

  it('scrolls to the heading for an in-page anchor instead of opening a tab', () => {
    const scrollIntoView = vi.fn();
    Element.prototype.scrollIntoView = scrollIntoView;
    const onLinkClick = vi.fn();
    render(
      <MarkdownView source={'[jump](#install-guide)\n\n## Install Guide'} onLinkClick={onLinkClick} />,
    );
    const link = screen.getByRole('link', { name: 'jump' });
    // Anchor links are in-page: no new-tab target, and the host is NOT asked to
    // navigate — the view handles the scroll itself.
    expect(link).not.toHaveAttribute('target', '_blank');
    fireEvent.click(link);
    expect(scrollIntoView).toHaveBeenCalledTimes(1);
    expect(onLinkClick).not.toHaveBeenCalled();
  });
});
