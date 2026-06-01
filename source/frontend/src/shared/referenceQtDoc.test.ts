import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

describe('REFERENCE_QT.md', () => {
  it('exists under inner repo documentation/ and retains Qt parity pointers', () => {
    const testDir = dirname(fileURLToPath(import.meta.url));
    const path = join(testDir, '..', '..', '..', '..', 'documentation', 'REFERENCE_QT.md');
    const md = readFileSync(path, 'utf-8');

    expect(md).toContain('cremniy-main');
    expect(md).toContain('pre-qt-removal-2026-05-01');
    expect(md).toMatch(/Canonical path/i);
    expect(md).toContain('github.com/igmunv/cremniy');
  });
});
