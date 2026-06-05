import { Children, Fragment, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { ReactNode } from 'react';

import styles from './SplitContainer.module.css';

export type SplitDirection = 'horizontal' | 'vertical';

export type SplitContainerProps = {
  direction: SplitDirection;
  /**
   * Initial child weights. Children with no entry default to 1. The array
   * length should match the number of children for full control.
   */
  defaultSizes?: number[];
  /**
   * Persisted weights (overrides defaultSizes). Re-renders when this changes
   * so callers can drive sizes from preferences.
   */
  sizes?: number[];
  /** Fires after a drag commits, with the new weights. */
  onSizesChange?: (sizes: number[]) => void;
  /** Minimum visible size for each child, in pixels. */
  minSize?: number;
  /** Handle thickness in pixels. Defaults to 6. */
  handleSize?: number;
  /**
   * Per-child collapse flags. A collapsed child sizes to its own content
   * (ignores its weight) and the handle adjacent to it is hidden — used to pin
   * a pane to a thin strip (e.g. the terminal minimised to its tab bar).
   */
  collapsed?: boolean[];
  children: ReactNode;
};

/**
 * Lightweight VSCode-style split layout. Children are laid out along
 * `direction`, separated by draggable handles. Sizes are stored as weights
 * (any non-negative numbers — only their ratio matters), so the layout
 * survives container resizes without snapping to the cached pixel count.
 */
export function SplitContainer({
  direction,
  defaultSizes,
  sizes: controlledSizes,
  onSizesChange,
  minSize = 80,
  handleSize = 1,
  collapsed,
  children,
}: SplitContainerProps) {
  const childArray = useMemo(() => Children.toArray(children), [children]);
  const childCount = childArray.length;

  const [internalSizes, setInternalSizes] = useState<number[]>(() =>
    buildInitialSizes(childCount, controlledSizes ?? defaultSizes),
  );

  // Sync external `sizes` (when it actually changes length / values).
  useEffect(() => {
    if (controlledSizes == null) {
      return;
    }
    setInternalSizes(buildInitialSizes(childCount, controlledSizes));
  }, [childCount, controlledSizes]);

  // Re-balance when child count changes (e.g. terminal gets re-added).
  useEffect(() => {
    setInternalSizes((prev) => {
      if (prev.length === childCount) {
        return prev;
      }
      return buildInitialSizes(childCount, prev);
    });
  }, [childCount]);

  const containerRef = useRef<HTMLDivElement | null>(null);
  const dragRef = useRef<{
    index: number;
    startCoord: number;
    startSizes: number[];
    containerExtent: number;
  } | null>(null);

  const onSizesChangeRef = useRef(onSizesChange);
  onSizesChangeRef.current = onSizesChange;

  const beginDrag = useCallback(
    (index: number, e: React.PointerEvent<HTMLDivElement>) => {
      const container = containerRef.current;
      if (container == null) {
        return;
      }
      e.preventDefault();
      const rect = container.getBoundingClientRect();
      const extent = direction === 'horizontal' ? rect.width : rect.height;
      dragRef.current = {
        index,
        startCoord: direction === 'horizontal' ? e.clientX : e.clientY,
        startSizes: internalSizes.slice(),
        containerExtent: extent,
      };
      (e.target as HTMLElement).setPointerCapture(e.pointerId);
    },
    [direction, internalSizes],
  );

  const onPointerMove = useCallback(
    (e: React.PointerEvent<HTMLDivElement>) => {
      const drag = dragRef.current;
      if (drag == null) {
        return;
      }
      const coord = direction === 'horizontal' ? e.clientX : e.clientY;
      const deltaPx = coord - drag.startCoord;
      if (deltaPx === 0) {
        return;
      }
      const weightSum = drag.startSizes.reduce((a, b) => a + b, 0) || 1;
      const pxPerWeight = drag.containerExtent / weightSum;
      const deltaWeight = deltaPx / pxPerWeight;

      const next = drag.startSizes.slice();
      const minWeight = (minSize / drag.containerExtent) * weightSum;
      const left = next[drag.index] ?? 0;
      const right = next[drag.index + 1] ?? 0;
      let newLeft = left + deltaWeight;
      let newRight = right - deltaWeight;
      if (newLeft < minWeight) {
        newRight -= minWeight - newLeft;
        newLeft = minWeight;
      }
      if (newRight < minWeight) {
        newLeft -= minWeight - newRight;
        newRight = minWeight;
      }
      next[drag.index] = Math.max(newLeft, minWeight);
      next[drag.index + 1] = Math.max(newRight, minWeight);
      setInternalSizes(next);
    },
    [direction, minSize],
  );

  const endDrag = useCallback(
    (e: React.PointerEvent<HTMLDivElement>) => {
      const drag = dragRef.current;
      if (drag == null) {
        return;
      }
      try {
        (e.target as HTMLElement).releasePointerCapture(e.pointerId);
      } catch {
        // pointer capture might have already been released
      }
      dragRef.current = null;
      onSizesChangeRef.current?.(internalSizes);
    },
    [internalSizes],
  );

  const isHorizontal = direction === 'horizontal';
  const handleClass = isHorizontal ? styles.handleH : styles.handleV;

  return (
    <div
      ref={containerRef}
      className={`${styles.split} ${isHorizontal ? styles.splitH : styles.splitV}`}
    >
      {childArray.map((child, i) => {
        const weight = internalSizes[i] ?? 1;
        const isCollapsed = collapsed?.[i] ?? false;
        const prevCollapsed = collapsed?.[i - 1] ?? false;
        // The divider is always a 1px line; it's only draggable when neither
        // adjacent pane is collapsed (a collapsed pane can't be resized).
        const draggable = i > 0 && !isCollapsed && !prevCollapsed;
        return (
          <Fragment key={`split-${i}`}>
            {i > 0 ? (
              <div
                key={`h-${i}`}
                className={`${handleClass}${draggable ? '' : ` ${styles.handleStatic}`}`}
                style={
                  isHorizontal
                    ? { width: handleSize }
                    : { height: handleSize }
                }
                onPointerDown={draggable ? (e) => beginDrag(i - 1, e) : undefined}
                onPointerMove={draggable ? onPointerMove : undefined}
                onPointerUp={draggable ? endDrag : undefined}
                onPointerCancel={draggable ? endDrag : undefined}
                role="separator"
                aria-orientation={isHorizontal ? 'vertical' : 'horizontal'}
              />
            ) : null}
            <div
              key={`c-${i}`}
              className={styles.cell}
              style={isCollapsed ? { flex: '0 0 auto' } : { flex: `${weight} ${weight} 0` }}
            >
              {child}
            </div>
          </Fragment>
        );
      })}
    </div>
  );
}

function buildInitialSizes(childCount: number, hint: number[] | undefined): number[] {
  if (childCount === 0) {
    return [];
  }
  const sizes = new Array<number>(childCount);
  for (let i = 0; i < childCount; i++) {
    const v = hint?.[i];
    sizes[i] = typeof v === 'number' && Number.isFinite(v) && v > 0 ? v : 1;
  }
  return sizes;
}
