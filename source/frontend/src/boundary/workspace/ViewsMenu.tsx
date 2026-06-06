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
 * The chevron dropdown — every registered view with a pin toggle. Pinning shows
 * the view's icon in the activity bar; the pin reads filled when pinned and
 * appears on row hover otherwise (VS Code's "additional views" menu).
 */
export function ViewsMenu({
  rows,
  anchor,
  onSelect,
  onTogglePin,
  onClose,
}: {
  rows: ViewMenuRow[];
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

  const rect = anchor.getBoundingClientRect();
  const width = 232;
  const style = {
    top: rect.bottom + 4,
    left: Math.max(8, Math.min(rect.left, window.innerWidth - width - 8)),
    width,
  };

  return (
    <div ref={ref} className={styles.menu} style={style} role="menu" aria-label="Views">
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
