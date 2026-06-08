import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';

import { Markdown } from './Markdown';

describe('Markdown', () => {
  it('renders headings at the right levels', () => {
    render(<Markdown source={'# One\n\n## Two\n\n### Three'} />);
    expect(screen.getByRole('heading', { level: 1, name: 'One' })).toBeInTheDocument();
    expect(screen.getByRole('heading', { level: 2, name: 'Two' })).toBeInTheDocument();
    expect(screen.getByRole('heading', { level: 3, name: 'Three' })).toBeInTheDocument();
  });

  it('renders inline emphasis, code and links', () => {
    render(
      <Markdown source={'A **bold** and *italic* with `code` and [site](https://example.com).'} />,
    );
    expect(screen.getByText('bold').tagName).toBe('STRONG');
    expect(screen.getByText('italic').tagName).toBe('EM');
    expect(screen.getByText('code').tagName).toBe('CODE');
    const link = screen.getByRole('link', { name: 'site' });
    expect(link).toHaveAttribute('href', 'https://example.com');
    expect(link).toHaveAttribute('target', '_blank');
  });

  it('renders fenced code blocks verbatim', () => {
    render(<Markdown source={'```\nline1\nline2\n```'} />);
    // Both lines live inside one <pre><code> block.
    expect(screen.getByText(/line1/)).toBeInTheDocument();
    expect(screen.getByText(/line2/)).toBeInTheDocument();
  });

  it('renders unordered and ordered lists', () => {
    render(<Markdown source={'- a\n- b\n\n1. one\n2. two'} />);
    const lists = screen.getAllByRole('list');
    expect(lists).toHaveLength(2);
    expect(screen.getAllByRole('listitem')).toHaveLength(4);
  });

  it('renders a blockquote, an hr and an image', () => {
    const { container } = render(
      <Markdown source={'> quoted\n\n---\n\n![alt text](https://example.com/i.png)'} />,
    );
    expect(container.querySelector('blockquote')).not.toBeNull();
    expect(container.querySelector('hr')).not.toBeNull();
    const img = screen.getByRole('img', { name: 'alt text' });
    expect(img).toHaveAttribute('src', 'https://example.com/i.png');
  });

  it('passes unknown / unterminated syntax through as text without throwing', () => {
    render(<Markdown source={'plain *unterminated and `unclosed'} />);
    expect(screen.getByText(/unterminated and/)).toBeInTheDocument();
  });
});
