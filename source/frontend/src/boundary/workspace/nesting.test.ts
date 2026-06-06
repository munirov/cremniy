import { describe, expect, it } from 'vitest';

import type { WorkspaceDirectoryEntry } from '@domain/workspace/directoryEntry';

import { DEFAULT_NESTING_PATTERNS, computeNesting } from './nesting';

function file(name: string): WorkspaceDirectoryEntry {
  return { name, path: `/w/${name}`, isDirectory: false };
}
function dir(name: string): WorkspaceDirectoryEntry {
  return { name, path: `/w/${name}`, isDirectory: true };
}

const names = (entries: WorkspaceDirectoryEntry[]) => entries.map((e) => e.name);
const childNames = (r: ReturnType<typeof computeNesting>, parent: string) =>
  (r.childrenOf.get(`/w/${parent}`) ?? []).map((e) => e.name);

describe('computeNesting', () => {
  it('nests ${capture} children under a *.ts parent', () => {
    const entries = [file('app.ts'), file('app.js'), file('app.d.ts'), file('other.ts')];
    const r = computeNesting(entries, DEFAULT_NESTING_PATTERNS);
    expect(names(r.roots)).toEqual(['app.ts', 'other.ts']);
    expect(childNames(r, 'app.ts').sort()).toEqual(['app.d.ts', 'app.js']);
  });

  it('nests lockfiles under package.json (literal parent)', () => {
    const entries = [file('package.json'), file('package-lock.json'), file('.npmrc')];
    const r = computeNesting(entries, DEFAULT_NESTING_PATTERNS);
    expect(names(r.roots)).toEqual(['package.json']);
    expect(childNames(r, 'package.json').sort()).toEqual(['.npmrc', 'package-lock.json']);
  });

  it('never nests directories', () => {
    const entries = [dir('src'), file('package.json'), file('package-lock.json')];
    const r = computeNesting(entries, DEFAULT_NESTING_PATTERNS);
    expect(r.roots).toContainEqual(dir('src'));
  });

  it('returns everything as a root when nothing matches', () => {
    const entries = [file('a.txt'), file('b.png'), dir('docs')];
    const r = computeNesting(entries, DEFAULT_NESTING_PATTERNS);
    expect(names(r.roots)).toEqual(['a.txt', 'b.png', 'docs']);
    expect(r.childrenOf.size).toBe(0);
  });

  it('keeps nesting one level deep (a nested child is not also a parent)', () => {
    // app.d.ts matches *.ts (could be a parent) but is itself nested under app.ts.
    const entries = [file('app.ts'), file('app.d.ts'), file('app.d.js')];
    const r = computeNesting(entries, DEFAULT_NESTING_PATTERNS);
    // app.d.ts is nested under app.ts; app.d.js must not hide under app.d.ts.
    expect(names(r.roots)).toContain('app.d.js');
    expect(childNames(r, 'app.d.ts')).toEqual([]);
  });

  it('does not nest a file under itself', () => {
    const entries = [file('tsconfig.json')];
    const r = computeNesting(entries, DEFAULT_NESTING_PATTERNS);
    expect(names(r.roots)).toEqual(['tsconfig.json']);
  });

  it('treats dots in filenames literally (no regex bleed)', () => {
    // `${capture}.js` for parent `a.ts` must match `a.js`, not `axjs`.
    const entries = [file('a.ts'), file('axjs')];
    const r = computeNesting(entries, DEFAULT_NESTING_PATTERNS);
    expect(names(r.roots)).toEqual(['a.ts', 'axjs']);
  });
});
