import { useEffect, useState } from 'react';

import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { fileNameFromPath } from '@domain/workspace/paths';
import { getWorkspaceFileSize } from '@infrastructure/tauri/bridge';

import styles from './BinaryFilePlaceholder.module.css';

function formatBytes(n: number): string {
  if (n < 1024) {
    return `${n} B`;
  }
  const units = ['KB', 'MB', 'GB', 'TB'];
  let value = n / 1024;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  return `${value.toFixed(value < 10 ? 1 : 0)} ${units[unit]}`;
}

// The byte tools that make sense as a first stop for an unknown binary. Every
// rail tool stays reachable from the ToolRail — these are just the shortcuts.
const ENTRY_TOOLS: ReadonlyArray<{ id: string; label: string }> = [
  { id: 'binary', label: 'Hex editor' },
  { id: 'disassembler', label: 'Disassembler' },
  { id: 'strings', label: 'Strings' },
  { id: 'symbols', label: 'Symbols' },
];

/**
 * Shown in the editor slot when the active tab is a binary file. The code editor
 * can't render bytes as text, so instead of mojibake we explain what the file is
 * and hand off to the byte tools (which read it straight from disk by path).
 */
export function BinaryFilePlaceholder({ filePath }: { filePath: string | null }) {
  const { setActiveToolTab } = useToolDock();
  const workspaceRoot = useWorkspaceRoot();
  const root = workspaceRoot?.path?.trim() ?? '';
  const [size, setSize] = useState<number | null>(null);

  useEffect(() => {
    if (filePath == null || filePath === '' || root === '') {
      setSize(null);
      return;
    }
    let cancelled = false;
    void getWorkspaceFileSize(root, filePath).then(
      (n) => {
        if (!cancelled) setSize(n);
      },
      () => {
        if (!cancelled) setSize(null);
      },
    );
    return () => {
      cancelled = true;
    };
  }, [filePath, root]);

  const name = filePath != null ? fileNameFromPath(filePath) : '';

  return (
    <div className={styles.root} role="status" aria-label="Binary file">
      <div className={styles.card}>
        <svg
          className={styles.glyph}
          viewBox="0 0 24 24"
          width="40"
          height="40"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.3"
          strokeLinecap="round"
          strokeLinejoin="round"
          aria-hidden
        >
          <path d="M14 3H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z" />
          <path d="M14 3v6h6" />
          <path d="M8.5 13h1m5 0h1M8.5 16.5h1m5 0h1" />
        </svg>
        <h2 className={styles.name} title={filePath ?? undefined}>
          {name}
        </h2>
        <p className={styles.note}>
          Binary file{size != null ? ` · ${formatBytes(size)}` : ''} — not shown as
          text. Open it with a byte tool:
        </p>
        <div className={styles.actions}>
          {ENTRY_TOOLS.map((tool) => (
            <button
              key={tool.id}
              type="button"
              className={styles.button}
              onClick={() => setActiveToolTab(tool.id)}
            >
              {tool.label}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}
