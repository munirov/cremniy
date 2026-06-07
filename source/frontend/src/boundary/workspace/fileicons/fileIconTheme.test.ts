import { describe, expect, it } from 'vitest';

import { fileIconUrl, folderIconUrl } from './fileIconTheme';

describe('fileIconTheme resolver', () => {
  it('resolves known extensions to a (distinct) icon', () => {
    const ts = fileIconUrl('main.ts');
    const css = fileIconUrl('styles.css');
    expect(ts).not.toBeNull();
    expect(css).not.toBeNull();
    expect(ts).not.toBe(css); // different types → different icons
  });

  it('matches an exact filename (.gitignore) before falling back', () => {
    expect(fileIconUrl('.gitignore')).not.toBeNull();
    // distinct from a generic unknown file
    expect(fileIconUrl('.gitignore')).not.toBe(fileIconUrl('whatever.zzz'));
  });

  it('falls back to the default file icon for unknown extensions', () => {
    // unknown extension and a no-extension file both resolve to the default.
    expect(fileIconUrl('weird.zzzz')).toBe(fileIconUrl('plainfilename'));
    expect(fileIconUrl('weird.zzzz')).not.toBeNull();
  });

  it('resolves a folder icon', () => {
    expect(folderIconUrl('src', false)).not.toBeNull();
  });
});
