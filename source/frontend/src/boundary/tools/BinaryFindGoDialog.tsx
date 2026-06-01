import { useCallback, useEffect, useId, useState } from 'react';

import {
  parseHexByteSequence,
  parseOffsetInput,
} from '@domain/hexView/hexBufferSearch';

import styles from './BinaryFindGoDialog.module.css';

export type BinaryFindGoDialogProps = {
  bufferLength: number;
  onClose: () => void;
  /** Return false when there is no match (dialog shows “Not found”). */
  onFindBytes: (needle: Uint8Array) => boolean;
  onGoToOffset: (offset: number) => void;
};

export function BinaryFindGoDialog({
  bufferLength,
  onClose,
  onFindBytes,
  onGoToOffset,
}: BinaryFindGoDialogProps) {
  const titleId = useId();

  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.preventDefault();
        onClose();
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [onClose]);

  const [offsetRadix, setOffsetRadix] = useState<'hex' | 'decimal'>('hex');
  const [offsetInput, setOffsetInput] = useState('');
  const [findInput, setFindInput] = useState('');
  const [status, setStatus] = useState('');

  const handleGo = useCallback(() => {
    const parsed = parseOffsetInput(offsetInput, offsetRadix);
    if (!parsed.ok) {
      setStatus(parsed.message);
      return;
    }
    if (bufferLength === 0) {
      setStatus('File is empty.');
      return;
    }
    if (parsed.value >= bufferLength) {
      setStatus('Offset is past end of file.');
      return;
    }
    setStatus('');
    onGoToOffset(parsed.value);
  }, [bufferLength, offsetInput, offsetRadix, onGoToOffset]);

  const handleFind = useCallback(() => {
    const parsed = parseHexByteSequence(findInput);
    if (!parsed.ok) {
      setStatus(parsed.message);
      return;
    }
    if (bufferLength === 0) {
      setStatus('File is empty.');
      return;
    }
    const found = onFindBytes(parsed.bytes);
    setStatus(found ? '' : 'Not found.');
  }, [bufferLength, findInput, onFindBytes]);

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
        aria-labelledby={titleId}
        onMouseDown={(e) => e.stopPropagation()}
      >
        <h2 id={titleId} className={styles.title}>
          Find / go to offset
        </h2>

        <fieldset className={styles.fieldset}>
          <legend className={styles.legend}>Go to offset</legend>
          <div className={styles.row}>
            <label className={styles.label} htmlFor="hex-go-offset">
              Offset
            </label>
            <input
              id="hex-go-offset"
              className={styles.input}
              type="text"
              value={offsetInput}
              onChange={(e) => setOffsetInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  e.preventDefault();
                  handleGo();
                }
              }}
              placeholder={offsetRadix === 'hex' ? 'e.g. 100 or 0x100' : 'e.g. 256'}
            />
          </div>
          <div className={styles.radixRow} role="group" aria-label="Offset number base">
            <label className={styles.radioLabel}>
              <input
                type="radio"
                name="offset-radix"
                checked={offsetRadix === 'hex'}
                onChange={() => setOffsetRadix('hex')}
              />
              Hex
            </label>
            <label className={styles.radioLabel}>
              <input
                type="radio"
                name="offset-radix"
                checked={offsetRadix === 'decimal'}
                onChange={() => setOffsetRadix('decimal')}
              />
              Decimal
            </label>
            <button type="button" className={styles.btnPrimary} onClick={handleGo}>
              Go
            </button>
          </div>
        </fieldset>

        <fieldset className={styles.fieldset}>
          <legend className={styles.legend}>Find bytes</legend>
          <div className={styles.row}>
            <label className={styles.label} htmlFor="hex-find-bytes">
              Pattern
            </label>
            <input
              id="hex-find-bytes"
              className={styles.inputWide}
              type="text"
              value={findInput}
              onChange={(e) => setFindInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  e.preventDefault();
                  handleFind();
                }
              }}
              placeholder="e.g. 48 65 6c 6c 6f or 48656c6c6f"
            />
          </div>
          <div className={styles.btnRowLeft}>
            <button type="button" className={styles.btnPrimary} onClick={handleFind}>
              Find
            </button>
          </div>
        </fieldset>

        <p className={styles.status} role="status">
          {status}
        </p>

        <div className={styles.actions}>
          <button type="button" className={styles.btn} onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
