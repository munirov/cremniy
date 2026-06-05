import { useEffect, useRef, useState } from 'react';

import styles from './Select.module.css';

/**
 * Tiny custom dropdown. We use this instead of a native `<select>` because
 * WebView2 renders the option popup with the OS color scheme — even with
 * `color-scheme: dark` set, the selected item gets a washed-out grey
 * highlight that doesn't match anything else in the app. A two-element
 * popup (button + absolute list) is easy enough to keep in our own theme.
 *
 * Keyboard: Enter / Space to open, ↑↓ to move highlight, Enter to commit,
 * Escape to cancel. Click outside to close.
 */
export type SelectOption<T extends string> = { value: T; label: string };

export type SelectProps<T extends string> = {
  id?: string;
  value: T;
  options: readonly SelectOption<T>[];
  onChange: (value: T) => void;
  disabled?: boolean;
  ariaLabel?: string;
};

export function Select<T extends string>({
  id,
  value,
  options,
  onChange,
  disabled = false,
  ariaLabel,
}: SelectProps<T>) {
  const [open, setOpen] = useState(false);
  const [hovered, setHovered] = useState<number>(() =>
    Math.max(
      0,
      options.findIndex((o) => o.value === value),
    ),
  );
  const rootRef = useRef<HTMLDivElement | null>(null);

  // Close on outside click / Escape.
  useEffect(() => {
    if (!open) return;
    const onDown = (ev: PointerEvent) => {
      const root = rootRef.current;
      if (root && !root.contains(ev.target as Node)) setOpen(false);
    };
    const onKey = (ev: KeyboardEvent) => {
      if (ev.key === 'Escape') setOpen(false);
    };
    window.addEventListener('pointerdown', onDown);
    window.addEventListener('keydown', onKey);
    return () => {
      window.removeEventListener('pointerdown', onDown);
      window.removeEventListener('keydown', onKey);
    };
  }, [open]);

  const current = options.find((o) => o.value === value);

  return (
    <div className={styles.root} ref={rootRef}>
      <button
        type="button"
        id={id}
        className={styles.trigger}
        disabled={disabled}
        aria-haspopup="listbox"
        aria-expanded={open}
        aria-label={ariaLabel}
        onClick={() => setOpen((v) => !v)}
        onKeyDown={(e) => {
          if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
            e.preventDefault();
            if (!open) {
              setOpen(true);
              return;
            }
            setHovered((h) => {
              const next =
                e.key === 'ArrowDown'
                  ? Math.min(options.length - 1, h + 1)
                  : Math.max(0, h - 1);
              return next;
            });
          } else if (e.key === 'Enter' && open) {
            e.preventDefault();
            const opt = options[hovered];
            if (opt) {
              onChange(opt.value);
              setOpen(false);
            }
          }
        }}
      >
        <span className={styles.triggerLabel}>{current?.label ?? ''}</span>
        <span className={styles.triggerCaret} aria-hidden>
          ▾
        </span>
      </button>
      {open ? (
        <ul role="listbox" className={styles.menu}>
          {options.map((opt, i) => {
            const isSelected = opt.value === value;
            const isHovered = i === hovered;
            return (
              <li role="option" aria-selected={isSelected} key={opt.value}>
                <button
                  type="button"
                  className={`${styles.option} ${isHovered ? styles.optionHover : ''} ${isSelected ? styles.optionSelected : ''}`}
                  onMouseEnter={() => setHovered(i)}
                  onClick={() => {
                    onChange(opt.value);
                    setOpen(false);
                  }}
                >
                  {opt.label}
                </button>
              </li>
            );
          })}
        </ul>
      ) : null}
    </div>
  );
}
