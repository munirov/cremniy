import { useMemo, useState } from 'react';

import { evalIntExpression } from '@domain/math/expressionEval';

import styles from './DataConverterDialog.module.css';

type Unit = { label: string; bytesPerUnit: bigint; color: string };

// Binary unit scale (1024^n) — matches Qt's storage-unit converter. SI users
// can divide by 1000 themselves; binary is the useful default for low-level
// work where you actually allocate/touch 1024-byte pages.
const UNITS: readonly Unit[] = [
  { label: 'Bits', bytesPerUnit: 0n, color: '#ce9178' }, // special — 8 bits per byte
  { label: 'Bytes', bytesPerUnit: 1n, color: '#9cdcfe' },
  { label: 'KiB', bytesPerUnit: 1024n, color: '#b5cea8' },
  { label: 'MiB', bytesPerUnit: 1024n ** 2n, color: '#dcdcaa' },
  { label: 'GiB', bytesPerUnit: 1024n ** 3n, color: '#c586c0' },
  { label: 'TiB', bytesPerUnit: 1024n ** 4n, color: '#4ec9b0' },
  { label: 'PiB', bytesPerUnit: 1024n ** 5n, color: '#569cd6' },
  { label: 'EiB', bytesPerUnit: 1024n ** 6n, color: '#d7ba7d' },
  { label: 'ZiB', bytesPerUnit: 1024n ** 7n, color: '#f97583' },
  { label: 'YiB', bytesPerUnit: 1024n ** 8n, color: '#d19a66' },
];

function formatBig(num: bigint, denominator: bigint): string {
  if (denominator === 0n) return '∞';
  if (denominator === 1n) return num.toString();
  // Show up to 6 decimal places without floating-point loss for the integer
  // part. Tail-zeros trimmed.
  const whole = num / denominator;
  const rem = num % denominator;
  if (rem === 0n) return whole.toString();
  const scale = 1_000_000n;
  const fractionScaled = (rem * scale) / denominator;
  const fractionStr = fractionScaled.toString().padStart(6, '0').replace(/0+$/, '');
  return fractionStr === '' ? whole.toString() : `${whole.toString()}.${fractionStr}`;
}

export type DataConverterDialogProps = {
  onClose: () => void;
};

export function DataConverterDialog({ onClose }: DataConverterDialogProps) {
  const [input, setInput] = useState('1024');
  const [unitLabel, setUnitLabel] = useState<Unit['label']>('Bytes');

  const parsed = useMemo<{ ok: true; bytes: bigint } | { ok: false; error: string }>(
    () => {
      try {
        const value = evalIntExpression(input);
        if (value < 0n) return { ok: false, error: 'Value must be non-negative' };
        const u = UNITS.find((x) => x.label === unitLabel)!;
        const bytes = u.label === 'Bits' ? value / 8n : value * u.bytesPerUnit;
        return { ok: true, bytes };
      } catch (e) {
        return { ok: false, error: e instanceof Error ? e.message : String(e) };
      }
    },
    [input, unitLabel],
  );

  const rows = useMemo(() => {
    if (!parsed.ok) return [];
    return UNITS.map((u) => {
      const valueStr =
        u.label === 'Bits'
          ? (parsed.bytes * 8n).toString()
          : formatBig(parsed.bytes, u.bytesPerUnit);
      return { ...u, valueStr };
    });
  }, [parsed]);

  const copyAll = () => {
    const all = rows.map((r) => `${r.label}: ${r.valueStr}`).join('\n');
    void navigator.clipboard.writeText(all);
  };

  return (
    <div className={styles.root} aria-label="Data Converter">
      <header className={styles.head}>
        <h2 className={styles.title}>Data Converter</h2>
        <button type="button" className={styles.btn} onClick={onClose} aria-label="Close">
          ✕
        </button>
      </header>

      <div className={styles.row}>
        <label className={styles.label}>Value</label>
        <input
          className={styles.input}
          value={input}
          onChange={(e) => setInput(e.currentTarget.value)}
          placeholder="e.g. 1024 or 0x4000 or 2 * 1024 + 7"
          autoFocus
        />
      </div>
      <div className={styles.row}>
        <label className={styles.label}>Unit</label>
        <select
          className={styles.input}
          value={unitLabel}
          onChange={(e) => setUnitLabel(e.currentTarget.value as Unit['label'])}
        >
          {UNITS.map((u) => (
            <option key={u.label} value={u.label}>
              {u.label}
            </option>
          ))}
        </select>
      </div>

      {!parsed.ok ? (
        <p className={styles.error} role="alert">
          {parsed.error}
        </p>
      ) : null}

      <div style={{ marginTop: '0.5rem' }}>
        {rows.map((r) => (
          <div
            key={r.label}
            style={{
              display: 'grid',
              gridTemplateColumns: '5rem 1fr auto',
              gap: '0.5rem',
              alignItems: 'center',
              padding: '0.25rem 0',
              borderBottom: '1px solid rgba(255,255,255,0.06)',
              fontFamily: 'var(--font-family-mono)',
              fontSize: 13,
            }}
          >
            <span style={{ color: r.color, fontWeight: 600 }}>{r.label}</span>
            <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{r.valueStr}</span>
            <button
              type="button"
              className={styles.btn}
              onClick={() => void navigator.clipboard.writeText(r.valueStr)}
              title={`Copy ${r.label}`}
            >
              Copy
            </button>
          </div>
        ))}
      </div>

      <div className={styles.actions}>
        <button type="button" className={styles.btn} onClick={copyAll}>
          Copy all
        </button>
        <button type="button" className={styles.btn} onClick={onClose}>
          Close
        </button>
      </div>
    </div>
  );
}
