import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import '@xterm/xterm/css/xterm.css';

import {
  listenSerialOutput,
  serialClose,
  serialOpen,
  serialWrite,
  type SerialOutputEvent,
} from '@infrastructure/tauri/bridge';
import { Menu } from '@boundary/common/Menu';
import { FitAddon } from '@xterm/addon-fit';
import { Terminal } from '@xterm/xterm';

import styles from './TerminalFooterPanel.module.css';

type SerialStatus = 'connecting' | 'connected' | 'error' | 'closed';

export type SerialInstanceProps = {
  /** Stable session id (one per tab) — keys the backend serial session and the
      output stream filter. */
  sessionId: string;
  port: string;
  baud: number;
  /** Whether this instance is the visible one — drives a fit() on show. */
  active: boolean;
  /** Whether the panel is minimised to its tab strip (all instances hidden). */
  collapsed?: boolean;
  /** Reports connection status so the owning tab can reflect it (dot / title). */
  onStatusChange?: (status: SerialStatus) => void;
};

function formatError(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

// Same dark palette as the PTY terminal — ANSI colors are device output, not UI
// chrome, so the design-canon "no blue" rule doesn't apply to them.
const SERIAL_THEME = {
  background: '#1e1e1e',
  foreground: '#d4d4d4',
  cursor: '#d4d4d4',
  cursorAccent: '#1e1e1e',
  selectionBackground: 'rgba(255,255,255,0.18)',
  black: '#3c3c3c',
  red: '#f44747',
  green: '#6a9955',
  yellow: '#d7ba7d',
  blue: '#569cd6',
  magenta: '#c586c0',
  cyan: '#4ec9b0',
  white: '#d4d4d4',
  brightBlack: '#808080',
  brightRed: '#ff5f56',
  brightGreen: '#b5cea8',
  brightYellow: '#ffd700',
  brightBlue: '#9cdcfe',
  brightMagenta: '#dcdcaa',
  brightCyan: '#9cdcfe',
  brightWhite: '#ffffff',
};

/**
 * A single serial console — owns one xterm canvas and one open serial port.
 * Deliberately separate from {@link TerminalInstance}: serial has no shell, no
 * ConPTY repaints, no command history / Cyrillic rescue, and a different I/O
 * path (serial_open / serial://output / serial_write). Keeping it apart leaves
 * the PTY terminal untouched (per the "fewer abstractions" preference).
 *
 * Raw monitor by default: input is sent as typed (no local echo) — STM32 / MCU
 * consoles usually echo themselves, and echoing here would double every char.
 */
export function SerialInstance({
  sessionId,
  port,
  baud,
  active,
  collapsed = false,
  onStatusChange,
}: SerialInstanceProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const termRef = useRef<Terminal | null>(null);
  const fitRef = useRef<FitAddon | null>(null);
  const [status, setStatus] = useState<SerialStatus>('connecting');
  const [error, setError] = useState<string | null>(null);
  const [ctxMenu, setCtxMenu] = useState<{ x: number; y: number } | null>(null);
  const onStatusRef = useRef(onStatusChange);
  onStatusRef.current = onStatusChange;

  // Create the xterm instance once.
  useEffect(() => {
    const container = containerRef.current;
    if (container == null || termRef.current != null) {
      return;
    }
    const term = new Terminal({
      theme: SERIAL_THEME,
      fontFamily: 'Consolas, "Cascadia Code", "Menlo", "Liberation Mono", monospace',
      fontSize: 13,
      lineHeight: 1.2,
      cursorBlink: true,
      cursorStyle: 'bar',
      scrollback: 5000,
      convertEol: false,
      allowTransparency: false,
    });
    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(container);
    try {
      fit.fit();
    } catch {
      // container may have zero size before layout
    }

    // Copy / paste on the physical key (ev.code), so a Cyrillic layout — where
    // Ctrl+C reports ev.key='с' — still copies instead of leaking to the device.
    term.attachCustomKeyEventHandler((ev) => {
      if (ev.type !== 'keydown') return true;
      const mod = ev.ctrlKey || ev.metaKey;
      if (!mod) return true;
      if (ev.code === 'KeyC') {
        const sel = term.getSelection();
        if (sel.length > 0) {
          void navigator.clipboard.writeText(sel).catch(() => undefined);
          term.clearSelection();
        }
        return false;
      }
      if (ev.code === 'KeyV') {
        void navigator.clipboard
          .readText()
          .then((text) => serialWrite(sessionId, text))
          .catch(() => undefined);
        return false;
      }
      if (ev.code === 'KeyA') {
        term.selectAll();
        return false;
      }
      return true;
    });

    term.onData((data) => {
      void serialWrite(sessionId, data).catch(() => undefined);
    });

    termRef.current = term;
    fitRef.current = fit;

    const handleResize = () => {
      try {
        fit.fit();
      } catch {
        // ignore transient zero-size
      }
    };
    window.addEventListener('resize', handleResize);
    const observer = new ResizeObserver(handleResize);
    observer.observe(container);

    return () => {
      window.removeEventListener('resize', handleResize);
      observer.disconnect();
      term.dispose();
      termRef.current = null;
      fitRef.current = null;
    };
  }, [sessionId]);

  // Open the serial port + stream its output. One session per mount; closes on
  // unmount (tab close).
  useEffect(() => {
    let cancelled = false;
    let unlisten: (() => void) | null = null;
    setStatus('connecting');
    onStatusRef.current?.('connecting');
    setError(null);

    void (async () => {
      try {
        unlisten = await listenSerialOutput((event: SerialOutputEvent) => {
          if (event.sessionId !== sessionId) return;
          termRef.current?.write(event.data);
        });
        if (cancelled) {
          unlisten();
          unlisten = null;
          return;
        }
        await serialOpen(sessionId, port, baud);
        if (cancelled) {
          await serialClose(sessionId).catch(() => undefined);
          return;
        }
        setStatus('connected');
        onStatusRef.current?.('connected');
      } catch (e) {
        if (cancelled) return;
        setError(formatError(e));
        setStatus('error');
        onStatusRef.current?.('error');
        termRef.current?.writeln(`\x1b[2;90mFailed to open ${port}: ${formatError(e)}\x1b[0m`);
      }
    })();

    return () => {
      cancelled = true;
      unlisten?.();
      void serialClose(sessionId).catch(() => undefined);
    };
  }, [sessionId, port, baud]);

  // Re-fit + focus when this instance becomes visible.
  useEffect(() => {
    if (!active || collapsed) return;
    const t = window.setTimeout(() => {
      try {
        fitRef.current?.fit();
      } catch {
        // ignore
      }
      termRef.current?.focus();
    }, 0);
    return () => window.clearTimeout(t);
  }, [active, collapsed]);

  const statusText = useMemo(() => {
    if (status === 'connecting') return `Opening ${port} @ ${baud}…`;
    if (status === 'error') return error ?? `Could not open ${port}.`;
    if (status === 'closed') return `${port} closed.`;
    return '';
  }, [status, error, port, baud]);

  const copySelection = useCallback(() => {
    const term = termRef.current;
    const sel = term?.getSelection() ?? '';
    if (sel.length > 0) {
      void navigator.clipboard.writeText(sel).catch(() => undefined);
      term?.clearSelection();
    }
  }, []);

  const pasteClipboard = useCallback(() => {
    void navigator.clipboard
      .readText()
      .then((text) => serialWrite(sessionId, text))
      .catch(() => undefined);
  }, [sessionId]);

  return (
    <div
      className={styles.instance}
      style={{ position: 'relative' }}
      onContextMenu={(e) => {
        e.preventDefault();
        setCtxMenu({ x: e.clientX, y: e.clientY });
      }}
    >
      {statusText !== '' ? (
        <div className={styles.floatingStatus} role="status" aria-live="polite" title={statusText}>
          {statusText}
        </div>
      ) : null}

      <div ref={containerRef} className={styles.xtermHost} aria-label={`Serial console ${port}`} />

      {ctxMenu != null ? (
        <Menu
          position={{ kind: 'point', x: ctxMenu.x, y: ctxMenu.y }}
          onClose={() => setCtxMenu(null)}
          label="Serial actions"
          groups={[
            [
              {
                label: 'Copy',
                onClick: copySelection,
                disabled: !(termRef.current?.hasSelection() ?? false),
              },
              { label: 'Paste', onClick: pasteClipboard },
            ],
          ]}
        />
      ) : null}
    </div>
  );
}
