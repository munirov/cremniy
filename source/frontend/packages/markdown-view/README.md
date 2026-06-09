# @cremniy/markdown-view

A small, **reusable** React Markdown view. Full CommonMark + GitHub-Flavored
Markdown (tables, task lists, strikethrough, autolinks) plus inline HTML, themed
for a dark surface. Built on `react-markdown` + `remark-gfm` + `rehype-raw` — a
spec-grade pipeline, not a hand-rolled subset.

It's a self-contained package: copy the folder into any React project, install
the three runtime deps, import the stylesheet once, and render.

## Use

```tsx
import { MarkdownView } from '@cremniy/markdown-view';
import '@cremniy/markdown-view/style.css';

<MarkdownView source={'# Hi\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n- [x] done\n- [ ] todo'} />;
```

Peer deps: `react`, `react-dom` (≥ 18). Runtime deps: `react-markdown`,
`remark-gfm`, `rehype-raw`.

## Props

| Prop        | Type     | Notes                                            |
|-------------|----------|--------------------------------------------------|
| `source`    | `string` | Markdown to render.                              |
| `className` | `string` | Extra class on the root, composed with the theme.|

## Theming

Override any `--cmv-*` variable on `.cmv-root` (or an ancestor):
`--cmv-fg`, `--cmv-muted`, `--cmv-border`, `--cmv-code-bg`, `--cmv-row-alt`,
`--cmv-link`, `--cmv-mono`. Defaults are a neutral monochrome dark (no blue).

## Security

`rehype-raw` renders inline HTML, so only feed **trusted** markdown. For
untrusted input, add `rehype-sanitize` to the pipeline.
