import { useCallback, useMemo, useState } from 'react';

import {
  formatBin,
  formatHex,
  maskToWidth,
  parseNumericInput,
  swapEndian,
  toSigned,
} from '@domain/reverseCalculator/reverseCalculator';

import styles from './ReverseCalculatorDialog.module.css';

const BIT_OPTIONS = [8, 16, 32, 64] as const;

export type ReverseCalculatorDialogProps = {
  onClose: () => void;
};

export function ReverseCalculatorDialog({ onClose }: ReverseCalculatorDialogProps) {
  const [input, setInput] = useState('');
  const [bits, setBits] = useState<number>(32);
  const [showSigned, setShowSigned] = useState(true);

  const parsed = useMemo(() => parseNumericInput(input), [input]);
  const empty = input.trim() === '';

  const display = useMemo(() => {
    if (empty) {
      return { ok: true as const, status: '', hex: '-', decU: '-', decS: '-', bin: '-' };
    }
    if (!parsed.ok) {
      return { ok: false as const, status: 'Invalid input', hex: '-', decU: '-', decS: '-', bin: '-' };
    }
    const v = maskToWidth(parsed.value, bits);
    return {
      ok: true as const,
      status: '',
      hex: formatHex(parsed.value, bits),
      decU: v.toString(10),
      decS: toSigned(v, bits).toString(10),
      bin: formatBin(parsed.value, bits),
    };
  }, [bits, empty, parsed]);

  const handleSwap = useCallback(() => {
    if (!parsed.ok) {
      return;
    }
    const masked = maskToWidth(parsed.value, bits);
    const swapped = swapEndian(masked, bits);
    setInput(formatHex(swapped, bits));
  }, [bits, parsed]);

  const copyText = useCallback(async (text: string) => {
    if (text === '-' || text === '') {
      return;
    }
    try {
      await navigator.clipboard.writeText(text);
    } catch {
      window.alert('Clipboard unavailable');
    }
  }, []);

  return (
    <div className={styles.root} role="dialog" aria-modal="true" aria-labelledby="rev-calc-title">
      <h2 id="rev-calc-title" className={styles.title}>
        Reverse Calculator
      </h2>
      <div className={styles.topRow}>
        <input
          className={styles.input}
          type="text"
          placeholder="Enter value: 1234, -1, 0xDEADBEEF, 0b1010"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          aria-label="Numeric input"
        />
        <span className={styles.bitsLabel}>Bits:</span>
        <select
          className={styles.select}
          value={bits}
          onChange={(e) => setBits(Number(e.target.value))}
          aria-label="Bit width"
        >
            {BIT_OPTIONS.map((b) => (
              <option key={b} value={b}>
                {b} bits
              </option>
            ))}
          </select>
        <label className={styles.checkLabel}>
          <input type="checkbox" checked={showSigned} onChange={(e) => setShowSigned(e.target.checked)} />
          Show signed
        </label>
      </div>
      <p className={styles.status} role="status">
        {display.status}
      </p>
      <div className={styles.grid}>
        <span className={styles.gridLabel}>Hex</span>
        <p className={styles.hexValue}>{display.hex}</p>
        <span className={styles.gridLabel}>Dec (unsigned)</span>
        <p className={styles.gridValue}>{display.decU}</p>
        {showSigned ? (
          <>
            <span className={styles.gridLabel}>Dec (signed)</span>
            <p className={styles.signedValue}>{display.decS}</p>
          </>
        ) : null}
        <span className={styles.gridLabel}>Bin</span>
        <p className={styles.binValue}>{display.bin}</p>
      </div>
      <div className={styles.btnRow}>
        <button type="button" className={styles.btn} onClick={() => void copyText(display.hex)}>
          Copy hex
        </button>
        <button type="button" className={styles.btn} onClick={() => void copyText(display.decU)}>
          Copy dec
        </button>
        <button type="button" className={styles.btn} onClick={() => void copyText(display.bin)}>
          Copy bin
        </button>
        <button type="button" className={styles.btn} onClick={handleSwap}>
          Swap endian
        </button>
        <button type="button" className={styles.btn} onClick={onClose}>
          Close
        </button>
      </div>
    </div>
  );
}
