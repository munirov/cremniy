import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

describe('theme tokens', () => {
  it('tokens.css defines the essential design tokens', () => {
    const path = join(process.cwd(), 'src/shared/theme/tokens.css');
    const css = readFileSync(path, 'utf-8');

    const required = [
      // Surfaces + text
      '--color-bg-base',
      '--color-bg-panel',
      '--color-text-primary',
      // The single colored accent + semantics
      '--color-cta',
      '--color-success',
      '--color-error',
      // Git decorations
      '--git-modified',
      '--git-untracked',
      // Typography
      '--font-family-mono',
    ];

    for (const token of required) {
      expect(css, `missing token: ${token}`).toContain(token);
    }
  });
});
