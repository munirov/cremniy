import { useEffect, useState } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import { subscribeSessionPatches, type SessionPatchEntry } from '@domain/hexView/sessionPatchStore';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

import styles from './BinaryToolPanel.module.css';

function describeCmd(entry: SessionPatchEntry): { kind: string; bytes: string; size: number } {
  if (entry.kind === 'insert') {
    return { kind: 'INSERT', bytes: hexOf(entry.bytes), size: entry.bytes.length };
  }
  if (entry.kind === 'remove') {
    return { kind: 'REMOVE', bytes: hexOf(entry.removed), size: entry.removed.length };
  }
  return {
    kind: 'REPLACE',
    bytes: `${hexOf(entry.oldBytes)} → ${hexOf(entry.newBytes)}`,
    size: entry.newBytes.length,
  };
}

function hexOf(bytes: Uint8Array): string {
  if (bytes.length === 0) {
    return '∅';
  }
  const slice = bytes.length > 16 ? bytes.subarray(0, 16) : bytes;
  const text = Array.from(slice, (b) => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
  return bytes.length > 16 ? `${text}…` : text;
}

function kindColor(kind: string): string {
  if (kind === 'INSERT') return '#6cd97a';
  if (kind === 'REMOVE') return '#f44747';
  return '#ffd864';
}

/**
 * Patches Tool Tab (Qt parity: dialogs/patches). Subscribes to the
 * session-wide patch store and shows every command applied to the active file
 * since it was last saved. Save clears the list (Reset on the binary panel
 * does the same).
 */
export function PatchesToolPanel() {
  const { activeFilePath, openPanel } = useIdeSession();
  const [patches, setPatches] = useState<readonly SessionPatchEntry[]>([]);

  useEffect(() => {
    if (activeFilePath == null || activeFilePath === '') {
      setPatches([]);
      return;
    }
    return subscribeSessionPatches(activeFilePath, setPatches);
  }, [activeFilePath]);

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a file to track its patches.</p>
      </section>
    );
  }

  return (
    <section className={styles.root} aria-label="Patches">
      <p className={styles.subtitle} title={activeFilePath}>
        {fileNameFromPath(activeFilePath)}
      </p>
      <div className={styles.toolbar}>
        <button
          type="button"
          className={styles.toolbarBtn}
          onClick={() => openPanel('binary')}
          title="Back to hex view"
        >
          Open HEX
        </button>
        <span className={styles.dirtyStatus}>
          {patches.length === 0 ? 'No unsaved patches' : `${patches.length} patch(es)`}
        </span>
      </div>
      <div
        style={{
          flex: 1,
          minHeight: 0,
          overflow: 'auto',
          fontFamily: 'var(--font-family-mono)',
          fontSize: 13,
        }}
      >
        {patches.map((p) => {
          const d = describeCmd(p);
          return (
            <div
              key={p.id}
              style={{
                display: 'grid',
                gridTemplateColumns: '5rem 9rem 4rem 1fr',
                gap: '0.5rem',
                padding: '4px 0.5rem',
                borderBottom: '1px solid rgba(255,255,255,0.04)',
                whiteSpace: 'nowrap',
              }}
              title={`#${p.id} ${d.kind} @0x${p.offset.toString(16)} (${d.size} bytes)`}
            >
              <span style={{ color: kindColor(d.kind), fontWeight: 600 }}>{d.kind}</span>
              <span style={{ opacity: 0.85 }}>0x{p.offset.toString(16).padStart(8, '0')}</span>
              <span style={{ opacity: 0.6 }}>{d.size}b</span>
              <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{d.bytes}</span>
            </div>
          );
        })}
      </div>
    </section>
  );
}
