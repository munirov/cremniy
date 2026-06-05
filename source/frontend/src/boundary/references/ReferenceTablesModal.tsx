import { useMemo, useState } from 'react';

import type { ReferencesMenuActionId } from '@domain/menu/referencesMenu';
import { ASCII_REFERENCE_ROWS, SCANCODE_REFERENCE_ROWS } from '@domain/references/referenceTables';

import styles from './ReferenceTablesModal.module.css';

export type ReferenceTablesModalProps = {
  kind: ReferencesMenuActionId;
  onClose: () => void;
};

export function ReferenceTablesModal({ kind, onClose }: ReferenceTablesModalProps) {
  const title = kind === 'asciiTable' ? 'ASCII reference' : 'Keyboard scancodes';
  const [filter, setFilter] = useState('');
  const q = filter.trim().toLowerCase();

  const asciiRows = useMemo(() => {
    if (q === '') return ASCII_REFERENCE_ROWS;
    return ASCII_REFERENCE_ROWS.filter(
      (r) =>
        r.dec.toString().includes(q) ||
        r.hex.toLowerCase().includes(q) ||
        r.char.toLowerCase().includes(q) ||
        r.description.toLowerCase().includes(q),
    );
  }, [q]);

  const scancodeRows = useMemo(() => {
    if (q === '') return SCANCODE_REFERENCE_ROWS;
    return SCANCODE_REFERENCE_ROWS.filter(
      (r) =>
        r.key.toLowerCase().includes(q) ||
        r.make.toLowerCase().includes(q) ||
        r.break.toLowerCase().includes(q) ||
        r.notes.toLowerCase().includes(q),
    );
  }, [q]);

  const copyCell = (value: string) => {
    void navigator.clipboard.writeText(value);
  };

  return (
    <div
      className={styles.backdrop}
      role="presentation"
      onMouseDown={(e) => e.target === e.currentTarget && onClose()}
    >
      <div
        className={styles.panel}
        role="dialog"
        aria-modal="true"
        aria-labelledby="ref-table-title"
        onMouseDown={(e) => e.stopPropagation()}
        style={{ maxHeight: '85vh', display: 'flex', flexDirection: 'column' }}
      >
        <h2 id="ref-table-title" className={styles.title}>
          {title}
        </h2>
        <input
          type="search"
          placeholder="Filter…"
          value={filter}
          onChange={(e) => setFilter(e.currentTarget.value)}
          autoFocus
          style={{
            margin: '0 0 0.5rem 0',
            padding: '0.3rem 0.5rem',
            background: 'rgba(255,255,255,0.04)',
            border: '1px solid rgba(255,255,255,0.1)',
            borderRadius: 3,
            color: 'inherit',
            font: 'inherit',
          }}
        />
        <div style={{ overflow: 'auto', flex: 1, minHeight: 0 }}>
          {kind === 'asciiTable' ? (
            <table className={styles.table}>
              <thead>
                <tr>
                  <th className={styles.th}>Dec</th>
                  <th className={styles.th}>Hex</th>
                  <th className={styles.th}>Char</th>
                  <th className={styles.th}>Description</th>
                </tr>
              </thead>
              <tbody>
                {asciiRows.map((row) => (
                  <tr key={row.dec}>
                    <td
                      className={styles.td}
                      onClick={() => copyCell(String(row.dec))}
                      title="Click to copy decimal"
                      style={{ cursor: 'pointer' }}
                    >
                      {row.dec}
                    </td>
                    <td
                      className={styles.td}
                      onClick={() => copyCell(row.hex)}
                      title="Click to copy hex"
                      style={{ cursor: 'pointer' }}
                    >
                      {row.hex}
                    </td>
                    <td
                      className={styles.td}
                      onClick={() => copyCell(row.char)}
                      title="Click to copy character"
                      style={{ cursor: 'pointer' }}
                    >
                      {row.char}
                    </td>
                    <td className={styles.td}>{row.description}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <table className={styles.table}>
              <thead>
                <tr>
                  <th className={styles.th}>Key</th>
                  <th className={styles.th}>Make</th>
                  <th className={styles.th}>Break</th>
                  <th className={styles.th}>Notes</th>
                </tr>
              </thead>
              <tbody>
                {scancodeRows.map((row) => (
                  <tr key={row.key}>
                    <td className={styles.td}>{row.key}</td>
                    <td
                      className={styles.td}
                      onClick={() => copyCell(row.make)}
                      title="Click to copy make code"
                      style={{ cursor: 'pointer' }}
                    >
                      {row.make}
                    </td>
                    <td
                      className={styles.td}
                      onClick={() => copyCell(row.break)}
                      title="Click to copy break code"
                      style={{ cursor: 'pointer' }}
                    >
                      {row.break}
                    </td>
                    <td className={styles.td}>{row.notes}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
        <div className={styles.actions}>
          <button type="button" className={styles.btn} onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
