import type { MouseEvent as ReactMouseEvent } from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import rehypeRaw from 'rehype-raw';
import rehypeSlug from 'rehype-slug';

import './markdown-view.css';

/**
 * In-page anchor (`#heading`) click → scroll the matching slugged heading into
 * view within this rendered document, instead of letting the browser navigate
 * (which, with `target="_blank"`, would pop a blank tab). Headings get their ids
 * from `rehype-slug` (GitHub's slug algorithm), so a README's table-of-contents
 * links just work. A bare `#` or an unknown target falls through to the default.
 */
function scrollToAnchor(href: string, event: ReactMouseEvent<HTMLAnchorElement>): void {
  const id = decodeURIComponent(href.slice(1));
  if (id === '') {
    return;
  }
  const root = event.currentTarget.closest('.cmv-root');
  const target = root?.querySelector(`#${CSS.escape(id)}`) ?? null;
  if (target == null) {
    return;
  }
  event.preventDefault();
  if (typeof (target as HTMLElement).scrollIntoView === 'function') {
    (target as HTMLElement).scrollIntoView({ behavior: 'smooth', block: 'start' });
  }
}

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
        rehypePlugins={[rehypeRaw, rehypeSlug]}
        components={{
          a: ({ node, ...props }) => {
            const href = typeof props.href === 'string' ? props.href : '';
            // In-page anchors stay inside the view (scroll); everything else
            // opens in a new tab unless the host intercepts via onLinkClick.
            const isAnchor = href.startsWith('#');
            return (
              <a
                target={isAnchor ? props.target : '_blank'}
                rel={isAnchor ? props.rel : 'noreferrer noopener'}
                {...props}
                onClick={(e) => {
                  if (isAnchor) {
                    scrollToAnchor(href, e);
                    return;
                  }
                  if (onLinkClick != null) {
                    onLinkClick(href, e);
                  } else {
                    props.onClick?.(e);
                  }
                }}
              />
            );
          },
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
