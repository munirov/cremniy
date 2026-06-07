import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import '@xterm/xterm/css/xterm.css';

import {
  listenSshExit,
  listenSshOutput,
  sshClose,
  sshOpen,
  sshWrite,
  type SshExitEvent,
  type SshOutputEvent,
} from '@infrastructure/tauri/bridge';
import { Menu } from '@boundary/common/Menu';
import { FitAddon } from '@xterm/addon-fit';
import { Terminal } from '@xterm/xterm';

import styles from './TerminalFooterPanel.module.css';

type SshStatus = 'connecting' | 'connected' | 'error' | 'closed';

export type SshInstanceProps = {
  /** Stable session id (one per tab) — keys the backend SSH session and the
      output stream filter. */
  sessionId: string;
  address: string;
  port: number;
  username: string;
  password?: string | null;
  /** Whether this instance is the visible one — drives a fit() on show. */
  active: boolean;
  /** Whether the panel is minimised to its tab strip (all instances hidden). */
  collapsed?: boolean;
  /** Reports connection status so the owning tab can reflect it (dot / title). */
  onStatusChange?: (status: SshStatus) => void;
};

function formatError(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

// Same dark palette as the PTY terminal — ANSI colors are device output, not UI
// chrome, so the design-canon "no blue" rule doesn't apply to them.
const SSH_THEME = {
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
 * A single SSH console — owns one xterm canvas and one open SSH shell.
 * Deliberately separate from {@link TerminalInstance} and {@link SerialInstance}
 * (per the "fewer abstractions" preference): SSH has a remote PTY that echoes
 * for us, so input is sent as typed (no local echo), and a distinct I/O path
 * (ssh_open / ssh://output / ssh_write / ssh://exit).
 */
export function SshInstance({
  sessionId,
  address,
  port,
  username,
  password,
  active,
  collapsed = false,
  onStatusChange,
}: SshInstanceProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const termRef = useRef<Terminal | null>(null);
  const fitRef = useRef<FitAddon | null>(null);
  const [status, setStatus] = useState<SshStatus>('connecting');
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
      theme: SSH_THEME,
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
    // Ctrl+C reports ev.key='с' — still copies instead of leaking to the host.
    term.attachCustomKeyEventHandler((ev) => {
      if (ev.type !== 'keydown') return true;
      const mod = ev.ctrlKey || ev.metaKey;
      if (!mod) return true;
      if (ev.code === 'KeyC') {
        const sel = term.getSelection();
        if (sel.length > 0) {
          void navigator.clipboard.writeText(sel).catch(() => undefined);
          term.clearSelection();
          return false;
        }
        // No selection → let Ctrl+C through as SIGINT to the remote shell.
        return true;
      }
      if (ev.code === 'KeyV') {
        void navigator.clipboard
          .readText()
          .then((text) => sshWrite(sessionId, text))
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
      void sshWrite(sessionId, data).catch(() => undefined);
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

  // Open the SSH shell + stream its output. One session per mount; closes on
  // unmount (tab close).
  useEffect(() => {
    let cancelled = false;
    let unlistenOutput: (() => void) | null = null;
    let unlistenExit: (() => void) | null = null;
    setStatus('connecting');
    onStatusRef.current?.('connecting');
    setError(null);

    void (async () => {
      try {
        unlistenOutput = await listenSshOutput((event: SshOutputEvent) => {
          if (event.sessionId !== sessionId) return;
          termRef.current?.write(event.data);
        });
        unlistenExit = await listenSshExit((event: SshExitEvent) => {
          if (event.sessionId !== sessionId) return;
          setStatus('closed');
          onStatusRef.current?.('closed');
          termRef.current?.writeln('\x1b[2;90m\r\nConnection closed.\x1b[0m');
        });
        if (cancelled) {
          unlistenOutput();
          unlistenExit();
          unlistenOutput = null;
          unlistenExit = null;
          return;
        }
        await sshOpen(sessionId, address, port, username, password);
        if (cancelled) {
          await sshClose(sessionId).catch(() => undefined);
          return;
        }
        setStatus('connected');
        onStatusRef.current?.('connected');
      } catch (e) {
        if (cancelled) return;
        setError(formatError(e));
        setStatus('error');
        onStatusRef.current?.('error');
        termRef.current?.writeln(
          `\x1b[2;90mFailed to connect to ${username}@${address}: ${formatError(e)}\x1b[0m`,
        );
      }
    })();

    return () => {
      cancelled = true;
      unlistenOutput?.();
      unlistenExit?.();
      void sshClose(sessionId).catch(() => undefined);
    };
  }, [sessionId, address, port, username, password]);

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
    if (status === 'connecting') return `Connecting to ${username}@${address}…`;
    if (status === 'error') return error ?? `Could not connect to ${username}@${address}.`;
    if (status === 'closed') return `${username}@${address} closed.`;
    return '';
  }, [status, error, username, address]);

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
      .then((text) => sshWrite(sessionId, text))
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

      <div
        ref={containerRef}
        className={styles.xtermHost}
        aria-label={`SSH console ${username}@${address}`}
      />

      {ctxMenu != null ? (
        <Menu
          position={{ kind: 'point', x: ctxMenu.x, y: ctxMenu.y }}
          onClose={() => setCtxMenu(null)}
          label="SSH actions"
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
