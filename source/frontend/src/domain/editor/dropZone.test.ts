import { describe, expect, it } from 'vitest';

import { dropBand } from './dropZone';

// A body 200px wide starting at x=100 → 25% = x150, 75% = x250.
const LEFT = 100;
const WIDTH = 200;

describe('dropBand', () => {
  it('left band for the first quarter', () => {
    expect(dropBand(100, LEFT, WIDTH)).toBe('left'); // 0%
    expect(dropBand(120, LEFT, WIDTH)).toBe('left'); // 10%
    expect(dropBand(149, LEFT, WIDTH)).toBe('left'); // just under 25%
  });

  it('center band for the middle half', () => {
    expect(dropBand(150, LEFT, WIDTH)).toBe('center'); // exactly 25% → center
    expect(dropBand(200, LEFT, WIDTH)).toBe('center'); // 50%
    expect(dropBand(249, LEFT, WIDTH)).toBe('center'); // just under 75%
  });

  it('right band for the last quarter', () => {
    expect(dropBand(250, LEFT, WIDTH)).toBe('right'); // exactly 75% → right
    expect(dropBand(280, LEFT, WIDTH)).toBe('right'); // 90%
    expect(dropBand(300, LEFT, WIDTH)).toBe('right'); // 100% (far edge)
  });

  it('25% boundary belongs to center, not left', () => {
    expect(dropBand(150, LEFT, WIDTH)).toBe('center');
    expect(dropBand(149.999, LEFT, WIDTH)).toBe('left');
  });

  it('75% boundary belongs to right, not center', () => {
    expect(dropBand(250, LEFT, WIDTH)).toBe('right');
    expect(dropBand(249.999, LEFT, WIDTH)).toBe('center');
  });

  it('clamps pointers outside the rect to the nearer edge band', () => {
    expect(dropBand(50, LEFT, WIDTH)).toBe('left'); // left of the rect
    expect(dropBand(400, LEFT, WIDTH)).toBe('right'); // right of the rect
  });

  it('works with a zero left origin', () => {
    expect(dropBand(10, 0, 100)).toBe('left'); // 10%
    expect(dropBand(50, 0, 100)).toBe('center'); // 50%
    expect(dropBand(90, 0, 100)).toBe('right'); // 90%
  });

  it('degenerates to center for a non-positive width', () => {
    expect(dropBand(10, 0, 0)).toBe('center');
    expect(dropBand(10, 0, -5)).toBe('center');
  });
});
