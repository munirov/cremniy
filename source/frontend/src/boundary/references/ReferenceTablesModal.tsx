import type { ReferencesMenuActionId } from '@domain/menu/referencesMenu';
import { ASCII_REFERENCE_ROWS, SCANCODE_REFERENCE_ROWS } from '@domain/references/referenceTables';

import styles from './ReferenceTablesModal.module.css';

export type ReferenceTablesModalProps = {
  kind: ReferencesMenuActionId;
  onClose: () => void;
};

export function ReferenceTablesModal({ kind, onClose }: ReferenceTablesModalProps) {
  const title = kind === 'asciiTable' ? 'ASCII reference' : 'Keyboard scancodes';

  return (
    <div className={styles.backdrop} role="presentation" onMouseDown={(e) => e.target === e.currentTarget && onClose()}>
      <div
        className={styles.panel}
        role="dialog"
        aria-modal="true"
        aria-labelledby="ref-table-title"
        onMouseDown={(e) => e.stopPropagation()}
      >
        <h2 id="ref-table-title" className={styles.title}>
          {title}
        </h2>
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
              {ASCII_REFERENCE_ROWS.map((row) => (
                <tr key={row.dec}>
                  <td className={styles.td}>{row.dec}</td>
                  <td className={styles.td}>{row.hex}</td>
                  <td className={styles.td}>{row.char}</td>
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
              {SCANCODE_REFERENCE_ROWS.map((row) => (
                <tr key={row.key}>
                  <td className={styles.td}>{row.key}</td>
                  <td className={styles.td}>{row.make}</td>
                  <td className={styles.td}>{row.break}</td>
                  <td className={styles.td}>{row.notes}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
        <div className={styles.actions}>
          <button type="button" className={styles.btn} onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
