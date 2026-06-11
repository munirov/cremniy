/**
 * Drop-band math for the editor-group edge drop-zones (Step 4 tab drag).
 *
 * When a tab is dragged over a group's editor body, the pointer's horizontal
 * position picks one of three actions, VS Code-style:
 *
 *   ├──── left ────┼──────── center ────────┼──── right ────┤
 *   0%            25%                       75%            100%
 *
 *   - left  band → split the file into a NEW group on the left
 *   - right band → split the file into a NEW group on the right
 *   - center     → move / append the file into this group
 *
 * This is the only non-trivial bit of the drag UI, so it lives here as a pure
 * function and is unit-tested; the drop handlers just call it and dispatch.
 */

export type DropBand = 'left' | 'right' | 'center';

/** Edge bands take the outer quarter each; the center half is everything between. */
const LEFT_EDGE = 0.25;
const RIGHT_EDGE = 0.75;

/**
 * Map a pointer X (in the same coordinate space as `rectLeft`/`rectWidth`, e.g.
 * `clientX` against a `getBoundingClientRect()`) to a drop band.
 *
 * Boundaries: the left band is `[0%, 25%)`, the right band is `[75%, 100%]`, and
 * the center is `[25%, 75%)`. So a pointer at exactly 25% is center, at exactly
 * 75% is right, and the far-left / far-right edges resolve to left / right.
 * Pointers outside the rect clamp to the nearer edge band. A non-positive width
 * (an unmeasured / collapsed body) degenerates to 'center' so a drop is never
 * lost.
 */
export function dropBand(pointerX: number, rectLeft: number, rectWidth: number): DropBand {
  if (!(rectWidth > 0)) {
    return 'center';
  }
  const ratio = (pointerX - rectLeft) / rectWidth;
  if (ratio < LEFT_EDGE) {
    return 'left';
  }
  if (ratio >= RIGHT_EDGE) {
    return 'right';
  }
  return 'center';
}
