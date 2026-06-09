import { render, screen, within } from '@testing-library/react';
import { describe, expect, it } from 'vitest';

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
});
