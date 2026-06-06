import type { WorkspaceDirectoryEntry } from '@domain/workspace/directoryEntry';

/**
 * File nesting (VS Code "Explorer: File Nesting" parity). Purely visual: child
 * files are tucked under a parent file in the tree and revealed by expanding it.
 * Two sources feed the same model — automatic pattern matches (this module) and
 * (later) manual drag overrides. The renderer only sees the resolved result.
 */

/** parentPattern → space/comma-separated child patterns. Patterns support `*`
 *  (wildcard, captured) plus `${capture}`/`${basename}`/`${extname}` tokens in
 *  the child patterns — same surface as VS Code's `explorer.fileNesting.patterns`. */
export type NestingPatterns = Record<string, string>;

export type NestingResult = {
  /** Entries to render at this directory level, in input order. A root may be a
   *  file that now owns nested children (→ render it expandable). */
  roots: WorkspaceDirectoryEntry[];
  /** parent.path → its nested child entries, in input order. */
  childrenOf: Map<string, WorkspaceDirectoryEntry[]>;
};

/**
 * Sensible defaults, trimmed from VS Code's shipped list to the languages this
 * IDE actually touches (TS/JS, Rust, manifests, docs). Kept here until a
 * Settings surface lets the user edit them.
 */
export const DEFAULT_NESTING_PATTERNS: NestingPatterns = {
  '*.ts': '${capture}.js, ${capture}.d.ts, ${capture}.js.map, ${capture}.*.ts',
  '*.tsx': '${capture}.js, ${capture}.*.tsx, ${capture}.css, ${capture}.module.css',
  '*.js': '${capture}.js.map, ${capture}.min.js, ${capture}.d.ts',
  '*.jsx': '${capture}.js',
  'package.json':
    'package-lock.json, yarn.lock, pnpm-lock.yaml, .npmrc, .nvmrc, npm-shrinkwrap.json, bun.lockb',
  'tsconfig.json': 'tsconfig.*.json',
  'cargo.toml': 'cargo.lock',
  'dockerfile': 'docker-compose.*, .dockerignore, compose.*',
  '.gitignore': '.gitattributes, .gitmodules, .mailmap',
  'readme*':
    'authors*, backers*, changelog*, citation*, code_of_conduct*, contributing*, contributors*, copying*, credits*, governance*, history*, license*, licence*, maintainers*, security*, sponsors*',
};

function escapeRegex(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function stripExt(name: string): string {
  const i = name.lastIndexOf('.');
  return i <= 0 ? name : name.slice(0, i);
}

function extOf(name: string): string {
  const i = name.lastIndexOf('.');
  return i <= 0 ? '' : name.slice(i + 1);
}

/** A parent pattern → anchored case-insensitive regex; each `*` becomes a
 *  capture group so the matched text is available to child patterns. */
function parentRegex(pattern: string): RegExp {
  let out = '';
  for (const part of pattern.split(/(\*)/g)) {
    if (part === '') continue;
    out += part === '*' ? '(.*)' : escapeRegex(part);
  }
  return new RegExp(`^${out}$`, 'i');
}

type ChildCtx = { capture: string; basename: string; extname: string };

/** A child pattern → anchored regex, substituting `${…}` tokens (from the
 *  matched parent) and turning bare `*` into a wildcard. Token values are
 *  escaped so dots in real filenames stay literal. */
function childRegex(pattern: string, ctx: ChildCtx): RegExp {
  const parts = pattern.split(/(\$\{capture\}|\$\{basename\}|\$\{extname\}|\*)/g);
  let out = '';
  for (const part of parts) {
    if (part === '') continue;
    if (part === '*') out += '.*';
    else if (part === '${capture}') out += escapeRegex(ctx.capture);
    else if (part === '${basename}') out += escapeRegex(ctx.basename);
    else if (part === '${extname}') out += escapeRegex(ctx.extname);
    else out += escapeRegex(part);
  }
  return new RegExp(`^${out}$`, 'i');
}

function splitChildSpec(spec: string): string[] {
  return spec.split(/[,\s]+/).filter((s) => s !== '');
}

/**
 * Manual nesting overrides for one directory, keyed by the child's file name:
 *   - a parent name  → force this file to nest under that sibling;
 *   - `null`         → force this file to stay at the top level (detach it from
 *                      whatever an auto pattern would otherwise do).
 * Manual decisions always win over the automatic patterns.
 */
export type ManualNestingForDir = Record<string, string | null>;

/**
 * Resolve nesting for one directory's (already filtered + sorted) entries.
 * Directories are never nested. Manual overrides apply first, then automatic
 * patterns fill in the rest. A file matches at most one parent and nesting is
 * one level deep — a file that is itself nested cannot also be a parent.
 */
export function computeNesting(
  entries: WorkspaceDirectoryEntry[],
  patterns: NestingPatterns,
  manual: ManualNestingForDir = {},
): NestingResult {
  const files = entries.filter((e) => !e.isDirectory);
  const byName = new Map(files.map((f) => [f.name, f] as const));
  const parentOf = new Map<string, string>(); // childPath → parentPath
  const detached = new Set<string>(); // childPath forced to top level

  // 1. Manual overrides win. A target that no longer exists falls through to
  //    "leave at top level" (we still skip auto for an explicitly-placed file).
  for (const f of files) {
    if (!(f.name in manual)) continue;
    const target = manual[f.name];
    if (target == null) {
      detached.add(f.path);
      continue;
    }
    const parent = byName.get(target);
    if (parent != null && parent.path !== f.path) {
      parentOf.set(f.path, parent.path);
    }
  }

  // 2. Automatic patterns fill in every file the user hasn't decided manually.
  const patternEntries = Object.entries(patterns);

  for (const parent of files) {
    const childMatchers: RegExp[] = [];
    for (const [pp, spec] of patternEntries) {
      const m = parentRegex(pp).exec(parent.name);
      if (m == null) continue;
      const ctx: ChildCtx = {
        capture: m[1] ?? parent.name,
        basename: stripExt(parent.name),
        extname: extOf(parent.name),
      };
      for (const cp of splitChildSpec(spec)) {
        childMatchers.push(childRegex(cp, ctx));
      }
    }
    if (childMatchers.length === 0) continue;
    for (const child of files) {
      if (child.path === parent.path || child.name === parent.name) continue;
      if (parentOf.has(child.path) || detached.has(child.path)) continue;
      if (child.name in manual) continue; // user decided this one explicitly
      if (childMatchers.some((re) => re.test(child.name))) {
        parentOf.set(child.path, parent.path);
      }
    }
  }

  // One level: if a parent is itself nested, release its children to top level.
  for (const [childPath, parentPath] of [...parentOf]) {
    if (parentOf.has(parentPath)) {
      parentOf.delete(childPath);
    }
  }

  const childrenOf = new Map<string, WorkspaceDirectoryEntry[]>();
  for (const child of files) {
    const pp = parentOf.get(child.path);
    if (pp == null) continue;
    const arr = childrenOf.get(pp);
    if (arr == null) childrenOf.set(pp, [child]);
    else arr.push(child);
  }

  const roots = entries.filter((e) => !parentOf.has(e.path));
  return { roots, childrenOf };
}
