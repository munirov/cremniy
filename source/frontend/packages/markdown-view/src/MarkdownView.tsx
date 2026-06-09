import type { MouseEvent as ReactMouseEvent } from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import rehypeRaw from 'rehype-raw';

import './markdown-view.css';

export type MarkdownViewProps = {
  /** The Markdown source to render. */
  source: string;
  /** Extra class name applied to the root container (compose with the theme). */
  className?: string;
  /**
   * Map an image `src` to a loadable URL — e.g. resolve a relative path against
   * the document's folder and turn it into an asset/file URL. Called for both
   * Markdown `![](…)` images and inline `<img>` tags. Absolute/remote URLs are
   * passed in unchanged; return them as-is.
   */
  transformImageUrl?: (src: string) => string;
  /**
   * Invoked when a link is clicked. Call `event.preventDefault()` to take over
   * navigation (e.g. open a relative file inside your app). If you don't, the
   * link opens in a new tab (`target="_blank"`).
   */
  onLinkClick?: (href: string, event: ReactMouseEvent<HTMLAnchorElement>) => void;
};

/**
 * A full Markdown renderer for React: CommonMark + GitHub-Flavored Markdown
 * (tables, task lists, strikethrough, autolinks) plus inline HTML, themed for a
 * dark surface. Built on `react-markdown` + `remark-gfm` + `rehype-raw`.
 *
 * Self-contained and reusable: drop the package into any React app
 * (`npm i react-markdown remark-gfm rehype-raw`) and render `<MarkdownView />`.
 * Resolve relative images/links to your project via `transformImageUrl` /
 * `onLinkClick`. Theme via the `--cmv-*` CSS variables.
 *
 * Note: `rehype-raw` renders inline HTML — only feed it TRUSTED markdown. For
 * untrusted input, add `rehype-sanitize` to the pipeline.
 */
export function MarkdownView({
  source,
  className,
  transformImageUrl,
  onLinkClick,
}: MarkdownViewProps) {
  return (
    <div className={className == null ? 'cmv-root' : `cmv-root ${className}`}>
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[rehypeRaw]}
        components={{
          a: ({ node, ...props }) => (
            <a
              target="_blank"
              rel="noreferrer noopener"
              {...props}
              onClick={
                onLinkClick != null
                  ? (e) => onLinkClick(typeof props.href === 'string' ? props.href : '', e)
                  : props.onClick
              }
            />
          ),
          img: ({ node, ...props }) => (
            <img
              {...props}
              src={
                transformImageUrl != null && typeof props.src === 'string'
                  ? transformImageUrl(props.src)
                  : props.src
              }
            />
          ),
        }}
      >
        {source}
      </ReactMarkdown>
    </div>
  );
}
