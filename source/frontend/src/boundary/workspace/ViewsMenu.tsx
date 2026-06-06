import { useEffect, useRef } from 'react';
import type { ReactNode } from 'react';

import { PinIcon } from './activityBarIcons';

import styles from './ViewsMenu.module.css';

export type ViewMenuRow = {
  id: string;
  label: string;
  icon: ReactNode;
  pinned: boolean;
  active: boolean;
};

/**
 * The chevron panel — every registered view with a pin toggle. NOT a floating
 * popup: it drops down inside the side panel, full block width, overlaying the
 * view content (VS Code's "additional views"). Pinning shows a view's icon in
 * the activity bar; the pin reads filled when pinned, appears on hover otherwise.
 */
export function ViewsMenu({
  rows,
  anchor,
  onSelect,
  onTogglePin,
  onClose,
}: {
  rows: ViewMenuRow[];
  /** The chevron button — excluded from outside-click so it can toggle closed. */
  anchor: HTMLElement;
  onSelect: (id: string) => void;
  onTogglePin: (id: string) => void;
  onClose: () => void;
}) {
  const ref = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    const onPointerDown = (ev: PointerEvent) => {
      const target = ev.target as Node;
      if (ref.current?.contains(target)) return;
      if (anchor.contains(target)) return;
      onClose();
    };
    const onKey = (ev: KeyboardEvent) => {
      if (ev.key === 'Escape') onClose();
    };
    document.addEventListener('pointerdown', onPointerDown, true);
    window.addEventListener('keydown', onKey, true);
    return () => {
      document.removeEventListener('pointerdown', onPointerDown, true);
      window.removeEventListener('keydown', onKey, true);
    };
  }, [anchor, onClose]);

  return (
    <div ref={ref} className={styles.overlay} role="menu" aria-label="Views">
      {rows.map((row) => (
        <div key={row.id} className={`${styles.row} ${row.active ? styles.rowActive : ''}`}>
          <button
            type="button"
            className={styles.rowMain}
            role="menuitemradio"
            aria-checked={row.active}
            onClick={() => onSelect(row.id)}
          >
            <span className={styles.rowIcon}>{row.icon}</span>
            <span className={styles.rowLabel}>{row.label}</span>
          </button>
          <button
            type="button"
            className={`${styles.pinBtn} ${row.pinned ? styles.pinBtnOn : ''}`}
            title={row.pinned ? 'Unpin from activity bar' : 'Pin to activity bar'}
            aria-pressed={row.pinned}
            onClick={() => onTogglePin(row.id)}
          >
            <PinIcon filled={row.pinned} />
          </button>
        </div>
      ))}
    </div>
  );
}
