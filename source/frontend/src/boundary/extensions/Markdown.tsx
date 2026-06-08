import type { ReactNode } from 'react';

import styles from './Markdown.module.css';

/**
 * A small, dependency-free Markdown → React renderer, sized for plugin readmes
 * (the details page body). It is deliberately a pragmatic subset — not a spec
 * implementation — covering what the bundled readmes use:
 *
 *   headings  `#` / `##` / `###`
 *   emphasis  `**bold**`, `*italic*`, `` `inline code` ``
 *   code      fenced ```…``` blocks
 *   links     `[text](url)` (external)   images `![alt](url)`
 *   lists     `-` / `*` bullets, `1.` ordered
 *   blocks    paragraphs, `>` blockquote, `---` horizontal rule
 *
 * Block parsing is line-based; inline parsing is a single left-to-right scan so
 * the markers compose (e.g. a **bold** word inside a list item). Unknown syntax
 * is passed through as literal text rather than throwing — a readme never breaks
 * the panel.
 */

// ── inline parsing ─────────────────────────────────────────────────────────

/** Split inline text into React nodes, honouring code / bold / italic / links. */
function renderInline(text: string, keyPrefix: string): ReactNode[] {
  const out: ReactNode[] = [];
  let buf = '';
  let i = 0;
  let k = 0;
  const flush = () => {
    if (buf !== '') {
      out.push(buf);
      buf = '';
    }
  };

  while (i < text.length) {
    const ch = text[i]!;
    const rest = text.slice(i);

    // `inline code` — highest precedence, no nested markup inside.
    if (ch === '`') {
      const end = text.indexOf('`', i + 1);
      if (end !== -1) {
        flush();
        out.push(
          <code key={`${keyPrefix}-c${k++}`} className={styles.code}>
            {text.slice(i + 1, end)}
          </code>,
        );
        i = end + 1;
        continue;
      }
    }

    // image ![alt](url) — must be tested before link (shares the `(url)` tail).
    if (ch === '!' && text[i + 1] === '[') {
      const m = /^!\[([^\]]*)\]\(([^)\s]+)\)/.exec(rest);
      if (m) {
        flush();
        out.push(
          <img key={`${keyPrefix}-img${k++}`} className={styles.img} src={m[2]} alt={m[1]} />,
        );
        i += m[0].length;
        continue;
      }
    }

    // link [text](url)
    if (ch === '[') {
      const m = /^\[([^\]]+)\]\(([^)\s]+)\)/.exec(rest);
      if (m) {
        flush();
        out.push(
          <a
            key={`${keyPrefix}-a${k++}`}
            className={styles.link}
            href={m[2]}
            target="_blank"
            rel="noreferrer noopener"
          >
            {renderInline(m[1]!, `${keyPrefix}-a${k}`)}
          </a>,
        );
        i += m[0].length;
        continue;
      }
    }

    // **bold**
    if (ch === '*' && text[i + 1] === '*') {
      const end = text.indexOf('**', i + 2);
      if (end !== -1) {
        flush();
        out.push(
          <strong key={`${keyPrefix}-b${k++}`} className={styles.strong}>
            {renderInline(text.slice(i + 2, end), `${keyPrefix}-b${k}`)}
          </strong>,
        );
        i = end + 2;
        continue;
      }
    }

    // *italic* (single star, not part of **)
    if (ch === '*') {
      const end = text.indexOf('*', i + 1);
      if (end !== -1 && end !== i + 1) {
        flush();
        out.push(
          <em key={`${keyPrefix}-i${k++}`} className={styles.em}>
            {renderInline(text.slice(i + 1, end), `${keyPrefix}-i${k}`)}
          </em>,
        );
        i = end + 1;
        continue;
      }
    }

    buf += ch;
    i += 1;
  }
  flush();
  return out;
}

// ── block parsing ──────────────────────────────────────────────────────────

type Block =
  | { kind: 'heading'; level: 1 | 2 | 3; text: string }
  | { kind: 'paragraph'; text: string }
  | { kind: 'code'; text: string }
  | { kind: 'ul'; items: string[] }
  | { kind: 'ol'; items: string[] }
  | { kind: 'hr' }
  | { kind: 'quote'; text: string };

const BULLET = /^\s*[-*]\s+(.*)$/;
const ORDERED = /^\s*\d+\.\s+(.*)$/;
const HEADING = /^(#{1,3})\s+(.*)$/;

/** Parse the source into a flat list of blocks (line-based). */
function parseBlocks(src: string): Block[] {
  const lines = src.replace(/\r\n?/g, '\n').split('\n');
  const blocks: Block[] = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i]!;

    // blank → block separator
    if (line.trim() === '') {
      i += 1;
      continue;
    }

    // fenced code block ```…```
    if (line.trim().startsWith('```')) {
      const body: string[] = [];
      i += 1;
      while (i < lines.length && !lines[i]!.trim().startsWith('```')) {
        body.push(lines[i]!);
        i += 1;
      }
      i += 1; // consume closing fence (or run off the end)
      blocks.push({ kind: 'code', text: body.join('\n') });
      continue;
    }

    // horizontal rule
    if (/^\s*(---|\*\*\*|___)\s*$/.test(line)) {
      blocks.push({ kind: 'hr' });
      i += 1;
      continue;
    }

    // heading
    const h = HEADING.exec(line);
    if (h) {
      blocks.push({ kind: 'heading', level: h[1]!.length as 1 | 2 | 3, text: h[2]! });
      i += 1;
      continue;
    }

    // blockquote (consecutive `>` lines)
    if (/^\s*>/.test(line)) {
      const body: string[] = [];
      while (i < lines.length && /^\s*>/.test(lines[i]!)) {
        body.push(lines[i]!.replace(/^\s*>\s?/, ''));
        i += 1;
      }
      blocks.push({ kind: 'quote', text: body.join('\n') });
      continue;
    }

    // unordered list (consecutive bullet lines)
    if (BULLET.test(line)) {
      const items: string[] = [];
      while (i < lines.length && BULLET.test(lines[i]!)) {
        items.push(BULLET.exec(lines[i]!)![1]!);
        i += 1;
      }
      blocks.push({ kind: 'ul', items });
      continue;
    }

    // ordered list (consecutive `1.` lines)
    if (ORDERED.test(line)) {
      const items: string[] = [];
      while (i < lines.length && ORDERED.test(lines[i]!)) {
        items.push(ORDERED.exec(lines[i]!)![1]!);
        i += 1;
      }
      blocks.push({ kind: 'ol', items });
      continue;
    }

    // paragraph — gather until a blank line or a line that starts a new block
    const para: string[] = [];
    while (i < lines.length) {
      const l = lines[i]!;
      if (
        l.trim() === '' ||
        l.trim().startsWith('```') ||
        HEADING.test(l) ||
        BULLET.test(l) ||
        ORDERED.test(l) ||
        /^\s*>/.test(l) ||
        /^\s*(---|\*\*\*|___)\s*$/.test(l)
      ) {
        break;
      }
      para.push(l.trim());
      i += 1;
    }
    blocks.push({ kind: 'paragraph', text: para.join(' ') });
  }

  return blocks;
}

function renderBlock(block: Block, key: string): ReactNode {
  switch (block.kind) {
    case 'heading': {
      const cls = block.level === 1 ? styles.h1 : block.level === 2 ? styles.h2 : styles.h3;
      const Tag = (`h${block.level}` as 'h1' | 'h2' | 'h3');
      return (
        <Tag key={key} className={cls}>
          {renderInline(block.text, key)}
        </Tag>
      );
    }
    case 'paragraph':
      return (
        <p key={key} className={styles.p}>
          {renderInline(block.text, key)}
        </p>
      );
    case 'code':
      return (
        <pre key={key} className={styles.pre}>
          <code>{block.text}</code>
        </pre>
      );
    case 'ul':
      return (
        <ul key={key} className={styles.ul}>
          {block.items.map((it, n) => (
            <li key={`${key}-${n}`} className={styles.li}>
              {renderInline(it, `${key}-${n}`)}
            </li>
          ))}
        </ul>
      );
    case 'ol':
      return (
        <ol key={key} className={styles.ol}>
          {block.items.map((it, n) => (
            <li key={`${key}-${n}`} className={styles.li}>
              {renderInline(it, `${key}-${n}`)}
            </li>
          ))}
        </ol>
      );
    case 'hr':
      return <hr key={key} className={styles.hr} />;
    case 'quote':
      return (
        <blockquote key={key} className={styles.quote}>
          {parseBlocks(block.text).map((b, n) => renderBlock(b, `${key}-q${n}`))}
        </blockquote>
      );
    default: {
      const unreachable: never = block;
      return unreachable;
    }
  }
}

/** Render a Markdown string as styled React (see module doc for the subset). */
export function Markdown({ source }: { source: string }) {
  const blocks = parseBlocks(source);
  return <div className={styles.root}>{blocks.map((b, n) => renderBlock(b, `b${n}`))}</div>;
}
