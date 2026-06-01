import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

describe('theme tokens', () => {
  it('tokens.css exists and mirrors Qt QSS palette', () => {
    const path = join(process.cwd(), 'src/shared/theme/tokens.css');
    const css = readFileSync(path, 'utf-8');
    expect(css).toContain('--color-bg-base');
    expect(css).toContain('#262626');
    expect(css).toContain('#1f1f1f');
    expect(css).toContain('#2626d5');
    expect(css).toContain('--font-family-mono');
  });
});
