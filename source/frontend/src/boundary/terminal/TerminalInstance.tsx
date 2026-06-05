import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import '@xterm/xterm/css/xterm.css';

import type { TerminalOutputEvent, TerminalSession } from '@domain/terminal/terminalSession';
import {
  interruptTerminalSession,
  listenTerminalOutput,
  resizeTerminalSession,
  startTerminalSession,
  stopTerminalSession,
  writeTerminalInput,
} from '@infrastructure/tauri/bridge';
import { FitAddon } from '@xterm/addon-fit';
import { Terminal } from '@xterm/xterm';

import styles from './TerminalFooterPanel.module.css';

type TerminalStatus = 'idle' | 'starting' | 'running' | 'error';

export type TerminalInstanceProps = {
  workspaceRoot: string | null;
  /** Whether this instance is the visible one — drives a fit() on show. */
  active: boolean;
  /** Whether the panel is minimised to its tab strip (all instances hidden). */
  collapsed?: boolean;
  /** Reports the live shell name (e.g. "powershell") so the owning tab can
      label itself; null when idle / exited. */
  onShellChange?: (shell: string | null) => void;
};

function formatError(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

// Cremniy terminal theme — JetBrains/VSCode-like dark.
const TERMINAL_THEME = {
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

const HISTORY_STORAGE_KEY = 'cremniy.terminalHistory.v1';
const MAX_HISTORY = 200;

function loadHistory(): string[] {
  try {
    const raw = window.localStorage.getItem(HISTORY_STORAGE_KEY);
    if (raw == null) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed.filter((x): x is string => typeof x === 'string') : [];
  } catch {
    return [];
  }
}

function saveHistory(items: string[]) {
  try {
    window.localStorage.setItem(HISTORY_STORAGE_KEY, JSON.stringify(items.slice(0, MAX_HISTORY)));
  } catch {
    // best effort
  }
}

/**
 * A single terminal — owns one xterm canvas and one PTY session. Multiple of
 * these live side by side under TerminalFooterPanel; only the `active` one is
 * visible. Inactive instances stay mounted so their session and scrollback
 * survive tab switches.
 */
export function TerminalInstance({
  workspaceRoot,
  active,
  collapsed = false,
  onShellChange,
}: TerminalInstanceProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const termRef = useRef<Terminal | null>(null);
  const fitRef = useRef<FitAddon | null>(null);
  const sessionRef = useRef<TerminalSession | null>(null);
  const [status, setStatus] = useState<TerminalStatus>('idle');
  const [error, setError] = useState<string | null>(null);
  const activeSessionIdRef = useRef<string | null>(null);
  const [history, setHistory] = useState<string[]>(() => loadHistory());
  const [historyOpen, setHistoryOpen] = useState(false);
  const inputBufferRef = useRef('');
  // Keep the shell-report callback in a ref so it never re-triggers the
  // session effect (an inline parent closure would otherwise restart the PTY
  // on every parent render).
  const onShellRef = useRef(onShellChange);
  onShellRef.current = onShellChange;

  const rootPath = workspaceRoot?.trim() ?? '';

  // Create the xterm instance once.
  useEffect(() => {
    const container = containerRef.current;
    if (container == null || termRef.current != null) {
      return;
    }
    const term = new Terminal({
      theme: TERMINAL_THEME,
      fontFamily: 'Consolas, "Cascadia Code", "Menlo", "Liberation Mono", monospace',
      fontSize: 13,
      lineHeight: 1.2,
      cursorBlink: true,
      cursorStyle: 'bar',
      scrollback: 5000,
      convertEol: true,
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

    const sendInterrupt = (current: TerminalSession) => {
      if (current.supportsInterrupt) {
        void interruptTerminalSession(current.sessionId).catch(() => undefined);
      } else {
        void writeTerminalInput(current.sessionId, '\x03');
      }
    };

    term.attachCustomKeyEventHandler((ev) => {
      if (ev.type !== 'keydown') {
        return true;
      }
      const mod = ev.ctrlKey || ev.metaKey;
      if (!mod) {
        return true;
      }
      const key = ev.key.toLowerCase();
      const current = sessionRef.current;

      if (key === 'c' && !ev.shiftKey) {
        const sel = term.getSelection();
        if (sel.length > 0) {
          void navigator.clipboard.writeText(sel).catch(() => undefined);
          term.clearSelection();
          return false;
        }
        if (current != null) {
          sendInterrupt(current);
        }
        return false;
      }
      if (key === 'c' && ev.shiftKey) {
        const sel = term.getSelection();
        if (sel.length > 0) {
          void navigator.clipboard.writeText(sel).catch(() => undefined);
          term.clearSelection();
        }
        return false;
      }
      if (key === 'v') {
        if (current != null) {
          void navigator.clipboard
            .readText()
            .then((text) => writeTerminalInput(current.sessionId, text))
            .catch(() => undefined);
        }
        return false;
      }
      if (ev.key === 'Pause' || ev.key === 'Break') {
        if (current != null) {
          sendInterrupt(current);
        }
        return false;
      }
      return true;
    });

    term.onData((data) => {
      const current = sessionRef.current;
      if (current == null) {
        return;
      }
      // Persistent-history capture: accumulate printable input, push on CR/LF.
      for (const ch of data) {
        const code = ch.charCodeAt(0);
        if (ch === '\r' || ch === '\n') {
          const cmd = inputBufferRef.current.trim();
          inputBufferRef.current = '';
          if (cmd !== '') {
            setHistory((prev) => {
              const next = [cmd, ...prev.filter((x) => x !== cmd)].slice(0, MAX_HISTORY);
              saveHistory(next);
              return next;
            });
          }
        } else if (ch === '\b' || code === 0x7f) {
          inputBufferRef.current = inputBufferRef.current.slice(0, -1);
        } else if (code >= 0x20 && code !== 0x7f) {
          inputBufferRef.current += ch;
        } else {
          inputBufferRef.current = '';
        }
      }
      void writeTerminalInput(current.sessionId, data);
    });

    term.onResize(({ rows, cols }) => {
      const current = sessionRef.current;
      if (current == null) {
        return;
      }
      void resizeTerminalSession(current.sessionId, rows, cols).catch(() => undefined);
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
  }, []);

  // Re-fit AND focus when this instance becomes visible. Switching/opening a
  // tab moves focus to the tab button, so without this the active xterm never
  // receives keystrokes — Ctrl+C / Ctrl+V (handled by xterm's key handler)
  // would silently do nothing until you clicked into the terminal.
  useEffect(() => {
    if (!active || collapsed) return;
    const fit = fitRef.current;
    // Defer to next frame so the display:block has taken effect.
    const t = window.setTimeout(() => {
      try {
        fit?.fit();
      } catch {
        // ignore
      }
      termRef.current?.focus();
    }, 0);
    return () => window.clearTimeout(t);
  }, [active, collapsed]);

  const writeSystem = useCallback((message: string) => {
    termRef.current?.writeln(`[2;90m${message}[0m`);
  }, []);

  // Start a shell session when the workspace is known / changes.
  useEffect(() => {
    activeSessionIdRef.current = null;
    sessionRef.current = null;
    setError(null);
    onShellRef.current?.(null);
    termRef.current?.reset();

    if (rootPath === '') {
      setStatus('idle');
      return;
    }

    let cancelled = false;
    let startedSessionId: string | null = null;
    let unlisten: (() => void) | null = null;
    setStatus('starting');

    void (async () => {
      try {
        unlisten = await listenTerminalOutput((event: TerminalOutputEvent) => {
          if (activeSessionIdRef.current != null && event.sessionId !== activeSessionIdRef.current) {
            return;
          }
          const term = termRef.current;
          if (term == null) return;
          if (event.stream === 'exit') {
            writeSystem('\r\n[process exited]');
            sessionRef.current = null;
            setStatus('idle');
            onShellRef.current?.(null);
            return;
          }
          term.write(event.data);
        });
        if (cancelled) {
          unlisten();
          unlisten = null;
          return;
        }
        const nextSession = await startTerminalSession(rootPath);
        startedSessionId = nextSession.sessionId;
        activeSessionIdRef.current = nextSession.sessionId;
        if (cancelled) {
          await stopTerminalSession(nextSession.sessionId).catch(() => undefined);
          return;
        }
        sessionRef.current = nextSession;
        setStatus('running');
        onShellRef.current?.(nextSession.shell);
      } catch (e) {
        if (cancelled) return;
        setError(formatError(e));
        setStatus('error');
        writeSystem(`Failed to start terminal: ${formatError(e)}`);
      }
    })();

    return () => {
      cancelled = true;
      activeSessionIdRef.current = null;
      unlisten?.();
      if (startedSessionId != null) {
        void stopTerminalSession(startedSessionId);
      }
    };
  }, [rootPath, writeSystem]);

  const statusText = useMemo(() => {
    if (rootPath === '') return 'Open a workspace folder to start a terminal session.';
    if (status === 'starting') return 'Starting terminal…';
    if (status === 'error') return error ?? 'Terminal failed to start.';
    return '';
  }, [error, rootPath, status]);

  const sendHistoryCommand = useCallback((cmd: string) => {
    const current = sessionRef.current;
    if (current == null) return;
    void writeTerminalInput(current.sessionId, `${cmd}\r`);
    setHistoryOpen(false);
  }, []);

  return (
    <div className={styles.instance} style={{ position: 'relative' }}>
      {statusText !== '' ? (
        <div className={styles.floatingStatus} role="status" aria-live="polite" title={statusText}>
          {statusText}
        </div>
      ) : null}

      <div ref={containerRef} className={styles.xtermHost} aria-label="Terminal output" />

      {history.length > 0 ? (
        <button
          type="button"
          className={styles.historyOverlayBtn}
          onClick={() => setHistoryOpen((v) => !v)}
          title={`Command history (${history.length})`}
          aria-label={`Command history (${history.length})`}
        >
          <svg
            aria-hidden
            width="14"
            height="14"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="1.6"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M3 12a9 9 0 1 0 3-6.7" />
            <path d="M3 4v5h5" />
            <path d="M12 8v5l3 2" />
          </svg>
        </button>
      ) : null}

      {historyOpen && history.length > 0 ? (
        <ul role="menu" className={styles.historyDropdown} onMouseLeave={() => setHistoryOpen(false)}>
          {history.map((cmd, i) => (
            <li role="none" key={`${cmd}-${i}`}>
              <button
                type="button"
                role="menuitem"
                className={styles.historyItem}
                onClick={() => sendHistoryCommand(cmd)}
                title={`Run: ${cmd}`}
              >
                {cmd}
              </button>
            </li>
          ))}
          <li role="none" className={styles.historySeparator}>
            <button
              type="button"
              role="menuitem"
              className={styles.historyClear}
              onClick={() => {
                setHistory([]);
                saveHistory([]);
                setHistoryOpen(false);
              }}
            >
              Clear history
            </button>
          </li>
        </ul>
      ) : null}
    </div>
  );
}
