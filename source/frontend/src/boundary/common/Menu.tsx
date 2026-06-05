import { useEffect, useRef } from 'react';

import styles from './Menu.module.css';

/**
 * One actionable row. The same shape backs context menus (right-click) and
 * button dropdowns (menu bar, panel toolbars) — anywhere a grouped action list
 * is needed.
 */
export type MenuItem = {
  label: string;
  onClick: () => void;
  /** Red, destructive styling (e.g. "Close all"). */
  danger?: boolean;
  /** Greyed out and non-interactive. */
  disabled?: boolean;
  /** Renders a check mark and the menuitemcheckbox role (toggle items). */
  checked?: boolean;
};

/** A run of items rendered together; groups are separated by a thin divider. */
export type MenuGroup = MenuItem[];

/** Where the popup anchors: a screen point (context menu) or under a trigger
    element (button dropdown). The trigger is excluded from outside-click so it
    can toggle the menu closed. */
export type MenuPosition =
  | { kind: 'point'; x: number; y: number }
  | { kind: 'anchor'; el: HTMLElement };

export type MenuProps = {
  /** Action groups; empty groups are dropped so callers can build them freely. */
  groups: MenuGroup[];
  /** Close requested — outside click, Escape, or an item was chosen. */
  onClose: () => void;
  position: MenuPosition;
  /** Accessible name for the menu. */
  label?: string;
};

/**
 * Menu — the single floating action list used across the app. Owns its own
 * dismissal (outside-click + Escape) and positioning; callers just hand it
 * grouped items and a close handler. Rendered fixed so it escapes overflow
 * clipping of whatever opened it.
 */
export function Menu({ groups, onClose, position, label }: MenuProps) {
  const ref = useRef<HTMLUListElement | null>(null);
  // Read position via a ref so the dismissal effect doesn't re-bind every render
  // (callers pass a fresh point object each time).
  const positionRef = useRef(position);
  positionRef.current = position;

  useEffect(() => {
    const onPointerDown = (ev: PointerEvent) => {
      const target = ev.target as Node;
      const el = ref.current;
      if (el != null && el.contains(target)) {
        return;
      }
      const pos = positionRef.current;
      // A click on the trigger button isn't "outside" — let it toggle us closed.
      if (pos.kind === 'anchor' && pos.el.contains(target)) {
        return;
      }
      onClose();
    };
    const onKey = (ev: KeyboardEvent) => {
      if (ev.key === 'Escape') {
        onClose();
      }
    };
    // Capture phase so we win the dismissal before other handlers act on it.
    document.addEventListener('pointerdown', onPointerDown, true);
    window.addEventListener('keydown', onKey, true);
    return () => {
      document.removeEventListener('pointerdown', onPointerDown, true);
      window.removeEventListener('keydown', onKey, true);
    };
  }, [onClose]);

  const style =
    position.kind === 'point'
      ? { left: position.x, top: position.y }
      : (() => {
          const rect = position.el.getBoundingClientRect();
          return { left: rect.left, top: rect.bottom };
        })();

  const visibleGroups = groups.filter((group) => group.length > 0);

  return (
    <ul ref={ref} className={styles.menu} role="menu" aria-label={label} style={style}>
      {visibleGroups.map((group, gi) => (
        <li role="none" key={gi} className={gi > 0 ? styles.group : undefined}>
          <ul role="none" className={styles.groupList}>
            {group.map((item, ii) => (
              <li role="none" key={ii}>
                <button
                  type="button"
                  role={item.checked != null ? 'menuitemcheckbox' : 'menuitem'}
                  aria-checked={item.checked}
                  disabled={item.disabled}
                  className={`${styles.item} ${item.danger ? styles.itemDanger : ''}`}
                  onClick={() => {
                    if (item.disabled) return;
                    item.onClick();
                    onClose();
                  }}
                >
                  <span className={styles.itemLabel}>{item.label}</span>
                  {item.checked ? <span className={styles.itemCheck}>✓</span> : null}
                </button>
              </li>
            ))}
          </ul>
        </li>
      ))}
    </ul>
  );
}
