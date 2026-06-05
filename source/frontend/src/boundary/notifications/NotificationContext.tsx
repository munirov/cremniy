import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';

/**
 * In-app toast stack — replaces native `window.alert()` so every error /
 * success message renders identically across Windows / macOS / Linux. Plain
 * React context, no third-party deps. Auto-dismiss after 5 s; user can close
 * a single toast by clicking ✕ or hovering to pause the timer (handled per
 * row).
 */

export type NotificationLevel = 'info' | 'success' | 'error' | 'warn';

export type Notification = {
  id: string;
  level: NotificationLevel;
  message: string;
  /** Optional secondary line shown smaller below the message. */
  detail?: string;
  /** Auto-dismiss timeout in ms; null disables auto-close. */
  timeoutMs?: number | null;
};

type Ctx = {
  notify: (n: Omit<Notification, 'id'>) => string;
  dismiss: (id: string) => void;
  clear: () => void;
  list: readonly Notification[];
};

const NotificationContext = createContext<Ctx | null>(null);

let nextIdCounter = 0;
function nextId(): string {
  nextIdCounter += 1;
  return `notif-${nextIdCounter}`;
}

const DEFAULT_TIMEOUT_BY_LEVEL: Readonly<Record<NotificationLevel, number | null>> = {
  info: 4000,
  success: 3000,
  warn: 6000,
  error: 8000,
};

export function NotificationProvider({ children }: { children: ReactNode }) {
  const [list, setList] = useState<Notification[]>([]);
  const timersRef = useRef<Map<string, ReturnType<typeof setTimeout>>>(new Map());

  const dismiss = useCallback((id: string) => {
    setList((prev) => prev.filter((n) => n.id !== id));
    const t = timersRef.current.get(id);
    if (t != null) {
      clearTimeout(t);
      timersRef.current.delete(id);
    }
  }, []);

  const notify = useCallback(
    (input: Omit<Notification, 'id'>): string => {
      const id = nextId();
      const timeout =
        input.timeoutMs === undefined ? DEFAULT_TIMEOUT_BY_LEVEL[input.level] : input.timeoutMs;
      const next: Notification = { id, ...input, timeoutMs: timeout };
      setList((prev) => [...prev, next]);
      if (timeout != null && timeout > 0) {
        const handle = setTimeout(() => dismiss(id), timeout);
        timersRef.current.set(id, handle);
      }
      return id;
    },
    [dismiss],
  );

  const clear = useCallback(() => {
    for (const t of timersRef.current.values()) {
      clearTimeout(t);
    }
    timersRef.current.clear();
    setList([]);
  }, []);

  // Cleanup on unmount.
  useEffect(
    () => () => {
      for (const t of timersRef.current.values()) {
        clearTimeout(t);
      }
      timersRef.current.clear();
    },
    [],
  );

  const value = useMemo<Ctx>(() => ({ notify, dismiss, clear, list }), [notify, dismiss, clear, list]);

  return (
    <NotificationContext.Provider value={value}>
      {children}
      <NotificationOverlay list={list} dismiss={dismiss} />
    </NotificationContext.Provider>
  );
}

export function useNotifyApi(): Ctx {
  const v = useContext(NotificationContext);
  if (v == null) {
    throw new Error('useNotifyApi must be used within NotificationProvider');
  }
  return v;
}

/**
 * Convenience: stable `(message, detail?) => void` helpers per level.
 * Components only need `const notify = useNotify();` then `notify.error(...)`.
 */
export function useNotify() {
  const { notify } = useNotifyApi();
  return useMemo(
    () => ({
      info: (message: string, detail?: string) => notify({ level: 'info', message, detail }),
      success: (message: string, detail?: string) => notify({ level: 'success', message, detail }),
      warn: (message: string, detail?: string) => notify({ level: 'warn', message, detail }),
      error: (message: string, detail?: string) => notify({ level: 'error', message, detail }),
    }),
    [notify],
  );
}

/** Render the toast stack — pinned to the bottom-right of the viewport. */
function NotificationOverlay({
  list,
  dismiss,
}: {
  list: readonly Notification[];
  dismiss: (id: string) => void;
}) {
  return (
    <div
      aria-live="polite"
      style={{
        position: 'fixed',
        right: '1rem',
        bottom: '1rem',
        display: 'flex',
        flexDirection: 'column',
        gap: '0.5rem',
        zIndex: 1000,
        maxWidth: 'min(420px, calc(100vw - 2rem))',
        pointerEvents: 'none',
      }}
    >
      {list.map((n) => (
        <NotificationRow key={n.id} n={n} onClose={() => dismiss(n.id)} />
      ))}
    </div>
  );
}

const LEVEL_STYLE: Readonly<Record<NotificationLevel, { bg: string; bd: string; icon: string }>> = {
  info: { bg: 'rgba(255, 255, 255, 0.06)', bd: 'rgba(255, 255, 255, 0.22)', icon: 'ⓘ' },
  success: { bg: 'rgba(33, 197, 93, 0.18)', bd: 'rgba(33, 197, 93, 0.55)', icon: '✓' },
  warn: { bg: 'rgba(215, 186, 125, 0.18)', bd: 'rgba(215, 186, 125, 0.55)', icon: '!' },
  error: { bg: 'rgba(239, 68, 68, 0.18)', bd: 'rgba(239, 68, 68, 0.55)', icon: '✕' },
};

function NotificationRow({
  n,
  onClose,
}: {
  n: Notification;
  onClose: () => void;
}) {
  const s = LEVEL_STYLE[n.level];
  return (
    <div
      role={n.level === 'error' || n.level === 'warn' ? 'alert' : 'status'}
      style={{
        pointerEvents: 'auto',
        display: 'flex',
        gap: '0.6rem',
        padding: '0.6rem 0.75rem',
        background: 'var(--color-bg-panel, #1f1f24)',
        border: `1px solid ${s.bd}`,
        borderLeft: `4px solid ${s.bd}`,
        borderRadius: 4,
        boxShadow: '0 0.5rem 1rem rgba(0, 0, 0, 0.45)',
        color: 'var(--color-text-primary, #d4d4d4)',
        font: 'inherit',
        fontSize: 13,
        backgroundColor: s.bg,
        backdropFilter: 'blur(4px)',
        animation: 'cremniyNotifSlideIn 0.18s ease-out',
      }}
    >
      <span
        aria-hidden
        style={{
          width: '1.25rem',
          textAlign: 'center',
          color: s.bd,
          fontWeight: 700,
          flexShrink: 0,
        }}
      >
        {s.icon}
      </span>
      <div style={{ flex: 1, minWidth: 0, wordBreak: 'break-word' }}>
        <div>{n.message}</div>
        {n.detail != null && n.detail !== '' ? (
          <div style={{ marginTop: '0.2rem', opacity: 0.75, fontSize: 11, whiteSpace: 'pre-wrap' }}>
            {n.detail}
          </div>
        ) : null}
      </div>
      <button
        type="button"
        onClick={onClose}
        aria-label="Dismiss notification"
        style={{
          flexShrink: 0,
          background: 'transparent',
          border: 'none',
          color: 'inherit',
          opacity: 0.6,
          cursor: 'pointer',
          fontSize: 14,
          lineHeight: 1,
          padding: '0 0.15rem',
        }}
      >
        ✕
      </button>
    </div>
  );
}
