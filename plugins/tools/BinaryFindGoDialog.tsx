import { useCallback, useEffect, useId, useMemo, useState } from 'react';

import {
  findOccurrence,
  parseFloatSearch,
  parseHexByteSequence,
  parseIntSearch,
  parseOffsetInput,
  parseTextSearch,
  replaceRange,
  type Endian,
  type FloatWidth,
  type IntWidth,
  type SearchDirection,
} from '@domain/hexView/hexBufferSearch';

import styles from './BinaryFindGoDialog.module.css';

type SearchMode = 'text' | 'hex' | 'int' | 'float';

export type BinaryFindGoDialogProps = {
  buffer: Uint8Array;
  cursorOffset: number;
  onClose: () => void;
  /** Move the caret/selection to the byte at `offset`. */
  onGoToOffset: (offset: number) => void;
  /** Move the caret to `offset` and select `length` bytes. Returns offset (for chained Find Next). */
  onSelectRange: (offset: number, length: number) => void;
  /** Apply a buffer replacement (one occurrence) — used by Replace / Replace All. */
  onReplace?: (offset: number, oldLength: number, newBytes: Uint8Array) => void;
};

export function BinaryFindGoDialog({
  buffer,
  cursorOffset,
  onClose,
  onGoToOffset,
  onSelectRange,
  onReplace,
}: BinaryFindGoDialogProps) {
  const titleId = useId();
  const bufferLength = buffer.length;

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

  // --- Go to offset section ---
  const [offsetRadix, setOffsetRadix] = useState<'hex' | 'decimal'>('hex');
  const [offsetInput, setOffsetInput] = useState('');

  // --- Find / Replace section ---
  const [mode, setMode] = useState<SearchMode>('text');
  const [needleInput, setNeedleInput] = useState('');
  const [replaceInput, setReplaceInput] = useState('');
  const [direction, setDirection] = useState<SearchDirection>('forward');
  const [caseInsensitive, setCaseInsensitive] = useState(false);
  const [intWidth, setIntWidth] = useState<IntWidth>(32);
  const [intSigned, setIntSigned] = useState(true);
  const [floatWidth, setFloatWidth] = useState<FloatWidth>(32);
  const [endian, setEndian] = useState<Endian>('little');

  const [status, setStatus] = useState('');
  const [lastFound, setLastFound] = useState<{ offset: number; length: number } | null>(null);

  const parseNeedle = useCallback(
    (text: string) => {
      switch (mode) {
        case 'text':
          return parseTextSearch(text);
        case 'hex':
          return parseHexByteSequence(text);
        case 'int':
          return parseIntSearch(text, intWidth, endian, intSigned);
        case 'float':
          return parseFloatSearch(text, floatWidth, endian);
      }
    },
    [endian, floatWidth, intSigned, intWidth, mode],
  );

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
    const parsed = parseNeedle(needleInput);
    if (!parsed.ok) {
      setStatus(parsed.message);
      return;
    }
    if (bufferLength === 0) {
      setStatus('File is empty.');
      return;
    }
    const startFrom =
      lastFound != null && direction === 'forward'
        ? lastFound.offset + 1
        : lastFound != null && direction === 'backward'
        ? lastFound.offset
        : cursorOffset;
    const found = findOccurrence(
      buffer,
      parsed.bytes,
      startFrom,
      direction,
      mode === 'text' && caseInsensitive,
    );
    if (found < 0) {
      setStatus('Not found.');
      setLastFound(null);
      return;
    }
    setStatus('');
    setLastFound({ offset: found, length: parsed.bytes.length });
    onSelectRange(found, parsed.bytes.length);
  }, [
    buffer,
    bufferLength,
    caseInsensitive,
    cursorOffset,
    direction,
    lastFound,
    mode,
    needleInput,
    onSelectRange,
    parseNeedle,
  ]);

  const handleReplace = useCallback(() => {
    if (onReplace == null) {
      setStatus('Replace not supported here.');
      return;
    }
    if (lastFound == null) {
      setStatus('Find a match first.');
      return;
    }
    const newBytes = parseNeedle(replaceInput);
    if (!newBytes.ok) {
      setStatus(`Replacement: ${newBytes.message}`);
      return;
    }
    onReplace(lastFound.offset, lastFound.length, newBytes.bytes);
    setStatus(`Replaced 1 match at 0x${lastFound.offset.toString(16)}.`);
    // After replace, advance past the replacement so Find Next does not re-hit it.
    setLastFound({ offset: lastFound.offset + newBytes.bytes.length - 1, length: 0 });
  }, [lastFound, onReplace, parseNeedle, replaceInput]);

  const handleReplaceAll = useCallback(() => {
    if (onReplace == null) {
      setStatus('Replace not supported here.');
      return;
    }
    const needleParsed = parseNeedle(needleInput);
    if (!needleParsed.ok) {
      setStatus(needleParsed.message);
      return;
    }
    const replParsed = parseNeedle(replaceInput);
    if (!replParsed.ok) {
      setStatus(`Replacement: ${replParsed.message}`);
      return;
    }
    let working = buffer;
    let count = 0;
    let offset = 0;
    while (offset <= working.length - needleParsed.bytes.length) {
      const hit = findOccurrence(
        working,
        needleParsed.bytes,
        offset,
        'forward',
        mode === 'text' && caseInsensitive,
      );
      if (hit < 0) {
        break;
      }
      working = replaceRange(working, hit, needleParsed.bytes.length, replParsed.bytes);
      count += 1;
      offset = hit + replParsed.bytes.length;
    }
    if (count === 0) {
      setStatus('Not found.');
      return;
    }
    // Apply the cumulative result as a single replacement of the whole buffer.
    onReplace(0, buffer.length, working);
    setStatus(`Replaced ${count} match${count === 1 ? '' : 'es'}.`);
    setLastFound(null);
  }, [buffer, caseInsensitive, mode, needleInput, onReplace, parseNeedle, replaceInput]);

  const previewNeedleBytes = useMemo(() => {
    if (needleInput === '') {
      return '';
    }
    const parsed = parseNeedle(needleInput);
    if (!parsed.ok) {
      return '';
    }
    return [...parsed.bytes].map((b) => b.toString(16).padStart(2, '0')).join(' ');
  }, [needleInput, parseNeedle]);

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
          Find / replace / go to offset
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
          <legend className={styles.legend}>Find / replace</legend>

          <div className={styles.radixRow} role="group" aria-label="Search mode">
            {(['text', 'hex', 'int', 'float'] as const).map((m) => (
              <label key={m} className={styles.radioLabel}>
                <input
                  type="radio"
                  name="search-mode"
                  checked={mode === m}
                  onChange={() => {
                    setMode(m);
                    setLastFound(null);
                  }}
                />
                {m === 'text' ? 'Text' : m === 'hex' ? 'Hex' : m === 'int' ? 'Int' : 'Float'}
              </label>
            ))}
          </div>

          <div className={styles.row}>
            <label className={styles.label} htmlFor="hex-find-input">
              Find
            </label>
            <input
              id="hex-find-input"
              className={styles.inputWide}
              type="text"
              value={needleInput}
              onChange={(e) => {
                setNeedleInput(e.target.value);
                setLastFound(null);
              }}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  e.preventDefault();
                  handleFind();
                }
              }}
              placeholder={placeholderForMode(mode)}
            />
          </div>

          {onReplace != null ? (
            <div className={styles.row}>
              <label className={styles.label} htmlFor="hex-replace-input">
                Replace
              </label>
              <input
                id="hex-replace-input"
                className={styles.inputWide}
                type="text"
                value={replaceInput}
                onChange={(e) => setReplaceInput(e.target.value)}
                placeholder={placeholderForMode(mode)}
              />
            </div>
          ) : null}

          {mode === 'int' ? (
            <div className={styles.radixRow} role="group" aria-label="Integer options">
              <label className={styles.radioLabel}>
                Width
                <select
                  value={intWidth}
                  onChange={(e) => setIntWidth(Number(e.target.value) as IntWidth)}
                  style={{ marginLeft: 4 }}
                >
                  <option value={8}>8</option>
                  <option value={16}>16</option>
                  <option value={32}>32</option>
                  <option value={64}>64</option>
                </select>
              </label>
              <label className={styles.radioLabel}>
                <input
                  type="checkbox"
                  checked={intSigned}
                  onChange={(e) => setIntSigned(e.target.checked)}
                />
                Signed
              </label>
              <label className={styles.radioLabel}>
                Endian
                <select
                  value={endian}
                  onChange={(e) => setEndian(e.target.value as Endian)}
                  style={{ marginLeft: 4 }}
                >
                  <option value="little">LE</option>
                  <option value="big">BE</option>
                </select>
              </label>
            </div>
          ) : null}

          {mode === 'float' ? (
            <div className={styles.radixRow} role="group" aria-label="Float options">
              <label className={styles.radioLabel}>
                Width
                <select
                  value={floatWidth}
                  onChange={(e) => setFloatWidth(Number(e.target.value) as FloatWidth)}
                  style={{ marginLeft: 4 }}
                >
                  <option value={32}>32</option>
                  <option value={64}>64</option>
                </select>
              </label>
              <label className={styles.radioLabel}>
                Endian
                <select
                  value={endian}
                  onChange={(e) => setEndian(e.target.value as Endian)}
                  style={{ marginLeft: 4 }}
                >
                  <option value="little">LE</option>
                  <option value="big">BE</option>
                </select>
              </label>
            </div>
          ) : null}

          {mode === 'text' ? (
            <div className={styles.radixRow}>
              <label className={styles.radioLabel}>
                <input
                  type="checkbox"
                  checked={caseInsensitive}
                  onChange={(e) => setCaseInsensitive(e.target.checked)}
                />
                Case-insensitive (ASCII)
              </label>
            </div>
          ) : null}

          <div className={styles.radixRow} role="group" aria-label="Direction">
            <label className={styles.radioLabel}>
              <input
                type="radio"
                name="search-dir"
                checked={direction === 'forward'}
                onChange={() => setDirection('forward')}
              />
              Forward
            </label>
            <label className={styles.radioLabel}>
              <input
                type="radio"
                name="search-dir"
                checked={direction === 'backward'}
                onChange={() => setDirection('backward')}
              />
              Backward
            </label>
            <button type="button" className={styles.btnPrimary} onClick={handleFind}>
              Find next
            </button>
            {onReplace != null ? (
              <>
                <button type="button" className={styles.btn} onClick={handleReplace}>
                  Replace
                </button>
                <button type="button" className={styles.btn} onClick={handleReplaceAll}>
                  Replace all
                </button>
              </>
            ) : null}
          </div>

          {previewNeedleBytes !== '' ? (
            <p className={styles.status} style={{ color: 'inherit', opacity: 0.65 }}>
              Bytes: {previewNeedleBytes}
            </p>
          ) : null}
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

function placeholderForMode(mode: SearchMode): string {
  switch (mode) {
    case 'text':
      return 'e.g. Hello world';
    case 'hex':
      return 'e.g. 48 65 6c or 48656c';
    case 'int':
      return 'e.g. 256 or 0xFF or -1';
    case 'float':
      return 'e.g. 3.14 or 1e-9';
  }
}
