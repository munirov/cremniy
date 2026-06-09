import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import rehypeRaw from 'rehype-raw';

import './markdown-view.css';

export type MarkdownViewProps = {
  /** The Markdown source to render. */
  source: string;
  /** Extra class name applied to the root container (compose with the theme). */
  className?: string;
};

/**
 * A full Markdown renderer for React: CommonMark + GitHub-Flavored Markdown
 * (tables, task lists, strikethrough, autolinks) plus inline HTML, themed for a
 * dark surface. Built on `react-markdown` + `remark-gfm` + `rehype-raw` — a
 * spec-grade pipeline, not a hand-rolled subset.
 *
 * Self-contained and reusable: drop the package into any React app
 * (`npm i react-markdown remark-gfm rehype-raw`), import the stylesheet once, and
 * render `<MarkdownView source={md} />`. Theme via the `--cmv-*` CSS variables.
 *
 * Note: `rehype-raw` renders inline HTML — only feed it TRUSTED markdown. For
 * untrusted input, add `rehype-sanitize` to the pipeline.
 */
export function MarkdownView({ source, className }: MarkdownViewProps) {
  return (
    <div className={className == null ? 'cmv-root' : `cmv-root ${className}`}>
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[rehypeRaw]}
        components={{
          // Links open in the host's default browser, not inside the app webview.
          a: ({ node, ...props }) => <a target="_blank" rel="noreferrer noopener" {...props} />,
        }}
      >
        {source}
      </ReactMarkdown>
    </div>
  );
}
