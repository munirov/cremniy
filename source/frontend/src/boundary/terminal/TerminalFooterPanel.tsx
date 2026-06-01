import { useCallback, useEffect, useMemo, useRef, useState, type FormEvent, type KeyboardEvent } from 'react';

import type { TerminalOutputEvent, TerminalSession } from '@domain/terminal/terminalSession';
import { appendHistoryEntry } from '@domain/terminal/terminalHistory';
import {
  interruptTerminalSession,
  listenTerminalOutput,
  startTerminalSession,
  stopTerminalSession,
  writeTerminalInput,
} from '@infrastructure/tauri/bridge';
import {
  loadTerminalHistory,
  saveTerminalHistory,
} from '@infrastructure/terminal/terminalHistoryStore';

import styles from './TerminalFooterPanel.module.css';

const MAX_OUTPUT_CHARS = 50_000;

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

function appendBoundedOutput(previous: string, event: TerminalOutputEvent): string {
  const prefix = event.stream === 'stderr' ? '[stderr] ' : '';
  const next = `${previous}${prefix}${event.data}`;
  if (next.length <= MAX_OUTPUT_CHARS) {
    return next;
  }
  return next.slice(next.length - MAX_OUTPUT_CHARS);
}

export function TerminalFooterPanel({ workspaceRoot }: TerminalFooterPanelProps) {
  const [session, setSession] = useState<TerminalSession | null>(null);
  const [status, setStatus] = useState<TerminalStatus>('idle');
  const [output, setOutput] = useState('');
  const [inputValue, setInputValue] = useState('');
  const [commandHistory, setCommandHistory] = useState<string[]>([]);
  const [historyIndex, setHistoryIndex] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);
  const activeSessionIdRef = useRef<string | null>(null);

  const rootPath = workspaceRoot?.trim() ?? '';

  // Restore persisted command history once (Qt parity: terminal_history.txt).
  useEffect(() => {
    let cancelled = false;
    void loadTerminalHistory().then((history) => {
      if (!cancelled && history.length > 0) {
        setCommandHistory(history);
      }
    });
    return () => {
      cancelled = true;
    };
  }, []);

  const appendSystemOutput = useCallback((message: string) => {
    setOutput((previous) =>
      appendBoundedOutput(previous, {
        sessionId: activeSessionIdRef.current ?? 'terminal',
        stream: 'system',
        data: message.endsWith('\n') ? message : `${message}\n`,
      }),
    );
  }, []);

  useEffect(() => {
    activeSessionIdRef.current = null;
    setSession(null);
    setError(null);
    setOutput('');
    setInputValue('');
    setHistoryIndex(null);

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
        unlisten = await listenTerminalOutput((event) => {
          const activeSessionId = activeSessionIdRef.current;
          if (activeSessionId != null && event.sessionId !== activeSessionId) {
            return;
          }
          setOutput((previous) => appendBoundedOutput(previous, event));
          if (event.stream === 'exit') {
            setSession(null);
            setStatus('idle');
            setError(null);
          }
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
        setStatus('running');
        if (!nextSession.supportsInterrupt) {
          appendSystemOutput('Ctrl+C is not supported by the current terminal bridge.');
        }
      } catch (e) {
        if (cancelled) {
          return;
        }
        setError(formatError(e));
        setStatus('error');
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
  }, [appendSystemOutput, rootPath]);

  const canSendInput = session != null && status === 'running';

  const statusText = useMemo(() => {
    if (rootPath === '') {
      return 'Open a workspace folder to start a terminal session.';
    }
    if (status === 'starting') {
      return `Starting terminal in ${rootPath}`;
    }
    if (status === 'running' && session != null) {
      return `${session.shell} - ${session.cwd}`;
    }
    if (status === 'error') {
      return error ?? 'Terminal failed to start.';
    }
    return 'Terminal is not running.';
  }, [error, rootPath, session, status]);

  const sendInput = useCallback(async () => {
    if (session == null || inputValue === '') {
      return;
    }
    const payload = inputValue.endsWith('\n') ? inputValue : `${inputValue}\n`;
    const command = inputValue.trim();
    setInputValue('');
    setHistoryIndex(null);
    if (command !== '') {
      setCommandHistory((previous) => {
        const next = appendHistoryEntry(previous, command);
        if (next !== previous && next[next.length - 1] === command) {
          void saveTerminalHistory(next);
        }
        return next;
      });
    }
    try {
      await writeTerminalInput(session.sessionId, payload);
    } catch (e) {
      appendSystemOutput(`Terminal input failed: ${formatError(e)}`);
    }
  }, [appendSystemOutput, inputValue, session]);

  const handleSubmit = useCallback(
    (event: FormEvent<HTMLFormElement>) => {
      event.preventDefault();
      void sendInput();
    },
    [sendInput],
  );

  const requestInterrupt = useCallback(async () => {
    if (session == null) {
      return;
    }
    if (!session.supportsInterrupt) {
      appendSystemOutput('Ctrl+C is unsupported because this bridge uses std::process pipes, not a PTY.');
      return;
    }
    try {
      await interruptTerminalSession(session.sessionId);
    } catch (e) {
      appendSystemOutput(`Terminal interrupt failed: ${formatError(e)}`);
    }
  }, [appendSystemOutput, session]);

  const handleInputKeyDown = useCallback(
    (event: KeyboardEvent<HTMLInputElement>) => {
      if (event.key === 'ArrowUp') {
        if (commandHistory.length === 0) {
          return;
        }
        event.preventDefault();
        const nextIndex = historyIndex == null ? commandHistory.length - 1 : Math.max(historyIndex - 1, 0);
        setHistoryIndex(nextIndex);
        setInputValue(commandHistory[nextIndex] ?? '');
        return;
      }
      if (event.key === 'ArrowDown') {
        if (historyIndex == null) {
          return;
        }
        event.preventDefault();
        const nextIndex = historyIndex + 1;
        if (nextIndex >= commandHistory.length) {
          setHistoryIndex(null);
          setInputValue('');
          return;
        }
        setHistoryIndex(nextIndex);
        setInputValue(commandHistory[nextIndex] ?? '');
        return;
      }
      if (event.key.toLowerCase() !== 'c' || !event.ctrlKey) {
        return;
      }
      event.preventDefault();
      void requestInterrupt();
    },
    [commandHistory, historyIndex, requestInterrupt],
  );

  return (
    <section className={styles.terminalPanel} aria-label="Terminal session" data-testid="terminal-panel">
      <div className={styles.toolbar}>
        <span className={styles.status} title={statusText} role="status" aria-live="polite">
          {statusText}
        </span>
        <button
          type="button"
          className={styles.interruptButton}
          onClick={() => void requestInterrupt()}
          disabled={session == null}
          title={session?.supportsInterrupt ? 'Send Ctrl+C' : 'Ctrl+C is unsupported without PTY support'}
        >
          {session?.supportsInterrupt ? 'Ctrl+C' : 'Ctrl+C unsupported'}
        </button>
      </div>
      <pre className={styles.output} aria-label="Terminal output">
        {output}
      </pre>
      {error != null ? (
        <p className={styles.errorMessage} role="alert">
          {error}
        </p>
      ) : null}
      <form className={styles.inputRow} onSubmit={handleSubmit}>
        <span className={styles.prompt} aria-hidden="true">
          &gt;
        </span>
        <input
          className={styles.input}
          aria-label="Terminal input"
          value={inputValue}
          disabled={!canSendInput}
          onChange={(event) => setInputValue(event.target.value)}
          onKeyDown={handleInputKeyDown}
          autoComplete="off"
          spellCheck={false}
        />
        <button type="submit" className={styles.sendButton} disabled={!canSendInput || inputValue === ''}>
          Enter
        </button>
      </form>
    </section>
  );
}
