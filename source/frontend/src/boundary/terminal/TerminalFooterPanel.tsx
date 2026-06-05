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

export type TerminalFooterPanelProps = {
  workspaceRoot: string | null;
};

function formatError(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }
  return String(error);
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
    // localStorage quota / disabled — best effort.
  }
}

export function TerminalFooterPanel({ workspaceRoot }: TerminalFooterPanelProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const termRef = useRef<Terminal | null>(null);
  const fitRef = useRef<FitAddon | null>(null);
  const [session, setSession] = useState<TerminalSession | null>(null);
  const sessionRef = useRef<TerminalSession | null>(null);
  const [status, setStatus] = useState<TerminalStatus>('idle');
  const [error, setError] = useState<string | null>(null);
  const activeSessionIdRef = useRef<string | null>(null);
  const [history, setHistory] = useState<string[]>(() => loadHistory());
  const [historyOpen, setHistoryOpen] = useState(false);
  // Command-in-progress buffer — accumulate non-control bytes between Enters,
  // then push the whole line into persistent history.
  const inputBufferRef = useRef('');

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
      // ignore initial sizing errors before container has dimensions
    }

    // Editor-like clipboard bindings:
    //   Ctrl+C — copy selection; if selection is empty, send Ctrl+Break (0x03 to
    //            current process) so you still have an interrupt path.
    //   Ctrl+V — paste clipboard into the PTY.
    //   Ctrl+Shift+C / Ctrl+Shift+V — always copy / always paste (no SIGINT
    //            fallback).
    //   Ctrl+Break — direct SIGINT, never copies (matches Windows convention).
    // Returning `false` keeps the keystroke out of xterm's default handler so
    // it doesn't also push 0x03 down to the shell.
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
        // No selection → fall through to SIGINT.
        if (current != null) {
          void writeTerminalInput(current.sessionId, '\x03');
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
          void writeTerminalInput(current.sessionId, '\x03');
        }
        return false;
      }
      return true;
    });

    // Real PTY mode — forward every keystroke as raw bytes. The shell does
    // echo, line editing, history, signals. Ctrl+C → 0x03 → SIGINT, Ctrl+D
    // → 0x04 → EOF, arrows → ESC[A/B/C/D handled by the shell's readline,
    // vim/less/htop work because they get a real TTY.
    term.onData((data) => {
      const current = sessionRef.current;
      if (current == null) {
        return;
      }
      // Persistent-history capture. Accumulate printable input; on CR/LF push
      // the buffered line. Backspace pops one char. Escape sequences (arrows /
      // ctrl combos starting with 0x1b or below 0x20 except \b) bypass the
      // buffer so they don't pollute history.
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
          // control byte (ctrl-c, escape sequence prefix, etc.) — discard the
          // in-progress line so we don't record a half-typed command.
          inputBufferRef.current = '';
        }
      }
      void writeTerminalInput(current.sessionId, data);
    });

    // Propagate viewport size to the PTY so applications can lay out output.
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
        // container may briefly have zero size — ignore
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

  const writeLine = useCallback((message: string) => {
    const term = termRef.current;
    if (term == null) {
      return;
    }
    term.writeln(message);
  }, []);

  const writeSystem = useCallback((message: string) => {
    writeLine(`[2;90m${message}[0m`);
  }, [writeLine]);

  // Start / restart a shell session whenever the workspace changes.
  useEffect(() => {
    activeSessionIdRef.current = null;
    sessionRef.current = null;
    setSession(null);
    setError(null);
    if (termRef.current != null) {
      termRef.current.reset();
    }

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
          const activeSessionId = activeSessionIdRef.current;
          if (activeSessionId != null && event.sessionId !== activeSessionId) {
            return;
          }
          const term = termRef.current;
          if (term == null) {
            return;
          }
          if (event.stream === 'exit') {
            writeSystem('\r\n[process exited]');
            setSession(null);
            sessionRef.current = null;
            setStatus('idle');
            setError(null);
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

        setSession(nextSession);
        sessionRef.current = nextSession;
        setStatus('running');
        if (!nextSession.supportsInterrupt) {
          writeSystem('Ctrl+C is unsupported by the current bridge (pipes, not PTY).');
        }
      } catch (e) {
        if (cancelled) {
          return;
        }
        setError(formatError(e));
        setStatus('error');
        writeSystem(`Failed to start terminal: ${formatError(e)}`);
      }
    })();

    return () => {
      cancelled = true;
      activeSessionIdRef.current = null;
      if (unlisten != null) {
        unlisten();
      }
      if (startedSessionId != null) {
        void stopTerminalSession(startedSessionId);
      }
    };
  }, [rootPath, writeSystem]);

  // Toolbar status — kept minimal. The shell + cwd line was just visual noise
  // (the terminal itself shows the same prompt). Only surface text when we
  // have something the user actually needs to know.
  const statusText = useMemo(() => {
    if (rootPath === '') {
      return 'Open a workspace folder to start a terminal session.';
    }
    if (status === 'starting') {
      return 'Starting terminal…';
    }
    if (status === 'error') {
      return error ?? 'Terminal failed to start.';
    }
    return '';
  }, [error, rootPath, status]);

  const requestInterrupt = useCallback(async () => {
    if (session == null) {
      return;
    }
    if (!session.supportsInterrupt) {
      writeSystem('Ctrl+C is unsupported because this bridge uses std::process pipes, not a PTY.');
      return;
    }
    try {
      await interruptTerminalSession(session.sessionId);
    } catch (e) {
      writeSystem(`Terminal interrupt failed: ${formatError(e)}`);
    }
  }, [session, writeSystem]);

  const sendHistoryCommand = useCallback(
    (cmd: string) => {
      const current = sessionRef.current;
      if (current == null) return;
      void writeTerminalInput(current.sessionId, `${cmd}\r`);
      setHistoryOpen(false);
    },
    [],
  );

  return (
    <section
      className={styles.terminalPanel}
      aria-label="Terminal session"
      data-testid="terminal-panel"
      style={{ position: 'relative' }}
    >
      {/* Floating status — only when there's something useful to surface. */}
      {statusText !== '' ? (
        <div className={styles.floatingStatus} role="status" aria-live="polite" title={statusText}>
          {statusText}
        </div>
      ) : null}

      {/* xterm canvas takes the whole panel. */}
      <div ref={containerRef} className={styles.xtermHost} aria-label="Terminal output" />

      {/* History icon — pinned to the top-right corner OF the terminal canvas
          itself (overlay on the xterm output), not in a separate toolbar
          above it. Looks like it belongs inside the terminal, not bolted on. */}
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
        <ul
          role="menu"
          className={styles.historyDropdown}
          onMouseLeave={() => setHistoryOpen(false)}
        >
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
    </section>
  );
}
