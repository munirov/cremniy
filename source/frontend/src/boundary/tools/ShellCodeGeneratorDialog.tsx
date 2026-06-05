import { useState } from 'react';

import { assembleWithNasm, type ShellcodeResult } from '@infrastructure/tauri/bridge';

import styles from './DataConverterDialog.module.css';

type Format = 'cArray' | 'cppVector' | 'raw';

function formatBytes(bytes: number[], fmt: Format): string {
  const pairs = bytes.map((b) => b.toString(16).padStart(2, '0'));
  switch (fmt) {
    case 'cArray':
      return `const unsigned char shellcode[] = { ${pairs.map((p) => `0x${p}`).join(', ')} };\n// length: ${bytes.length}`;
    case 'cppVector':
      return `std::vector<uint8_t> shellcode = { ${pairs.map((p) => `0x${p}`).join(', ')} };\n// length: ${bytes.length}`;
    case 'raw':
      return pairs.join(' ');
  }
}

export type ShellCodeGeneratorDialogProps = {
  onClose: () => void;
};

export function ShellCodeGeneratorDialog({ onClose }: ShellCodeGeneratorDialogProps) {
  const [source, setSource] = useState(
    'xor eax, eax\nmov al, 60\nxor edi, edi\nsyscall',
  );
  const [bits, setBits] = useState<16 | 32 | 64>(64);
  const [format, setFormat] = useState<Format>('cArray');
  const [result, setResult] = useState<ShellcodeResult | null>(null);
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);

  const handleAssemble = async () => {
    setBusy(true);
    setError('');
    setResult(null);
    try {
      const r = await assembleWithNasm(source, bits);
      setResult(r);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className={styles.root} style={{ maxWidth: '50rem', minWidth: '40rem' }} aria-label="Shellcode Generator">
      <header className={styles.head}>
        <h2 className={styles.title}>Shellcode Generator</h2>
        <button type="button" className={styles.btn} onClick={onClose} aria-label="Close">
          ✕
        </button>
      </header>

      <div className={styles.row}>
        <label className={styles.label}>Bits</label>
        <select
          className={styles.input}
          value={bits}
          onChange={(e) => setBits(Number(e.currentTarget.value) as 16 | 32 | 64)}
        >
          <option value={16}>16-bit</option>
          <option value={32}>32-bit</option>
          <option value={64}>64-bit</option>
        </select>
      </div>

      <div className={styles.row}>
        <label className={styles.label}>Format</label>
        <select
          className={styles.input}
          value={format}
          onChange={(e) => setFormat(e.currentTarget.value as Format)}
        >
          <option value="cArray">C array</option>
          <option value="cppVector">C++ vector</option>
          <option value="raw">Raw hex</option>
        </select>
      </div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: '0.5rem', marginTop: '0.5rem' }}>
        <label style={{ fontSize: '0.85rem', opacity: 0.8 }}>NASM source</label>
        <textarea
          value={source}
          onChange={(e) => setSource(e.currentTarget.value)}
          rows={8}
          spellCheck={false}
          style={{
            fontFamily: 'var(--font-family-mono)',
            fontSize: 13,
            padding: '0.4rem',
            background: 'var(--color-bg-base)',
            color: 'inherit',
            border: '1px solid var(--color-border-muted)',
            borderRadius: 3,
            resize: 'vertical',
          }}
        />
      </div>

      <div className={styles.actions}>
        <button type="button" className={styles.btn} disabled={busy} onClick={handleAssemble}>
          {busy ? 'Assembling…' : 'Assemble'}
        </button>
      </div>

      {error !== '' ? (
        <pre className={styles.error} role="alert" style={{ whiteSpace: 'pre-wrap' }}>
          {error}
        </pre>
      ) : null}

      {result != null ? (
        <>
          <div style={{ marginTop: '0.5rem', fontSize: '0.85rem', opacity: 0.8 }}>
            {result.bytes.length} bytes from {result.nasmPath}
          </div>
          <textarea
            value={formatBytes(result.bytes, format)}
            readOnly
            rows={8}
            style={{
              width: '100%',
              fontFamily: 'var(--font-family-mono)',
              fontSize: 12,
              padding: '0.4rem',
              background: 'var(--color-bg-base)',
              color: 'inherit',
              border: '1px solid var(--color-border-muted)',
              borderRadius: 3,
              marginTop: '0.25rem',
            }}
          />
          <div className={styles.actions}>
            <button
              type="button"
              className={styles.btn}
              onClick={() => void navigator.clipboard.writeText(formatBytes(result.bytes, format))}
            >
              Copy output
            </button>
          </div>
        </>
      ) : null}
    </div>
  );
}
