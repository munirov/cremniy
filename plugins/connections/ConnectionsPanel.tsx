import { useCallback, useEffect, useState } from 'react';

import {
  connDelete,
  connList,
  connSave,
  serialPorts,
  type Connection,
  type SerialPortInfo,
} from '@infrastructure/tauri/bridge';
import { openConnection } from '@shared/connections/connectionBus';

import styles from './ConnectionsPanel.module.css';

const COMMON_BAUDS = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600];

type Mode = { kind: 'list' } | { kind: 'form'; draft: Connection };

function blankSerial(): Connection {
  return { id: '', label: '', kind: 'serial', tags: [], serial: { port: '', baud: 115200 } };
}

function blankSsh(): Connection {
  return {
    id: '',
    label: '',
    kind: 'ssh',
    tags: [],
    ssh: { address: '', port: 22, username: '', password: '' },
  };
}

/** Subtitle shown under a host name in the list. */
function describe(conn: Connection): string {
  if (conn.kind === 'serial' && conn.serial != null) {
    return `${conn.serial.port || '—'} · ${conn.serial.baud} baud`;
  }
  if (conn.kind === 'ssh' && conn.ssh != null) {
    const user = conn.ssh.username ? `${conn.ssh.username}@` : '';
    return `${user}${conn.ssh.address || '—'}:${conn.ssh.port}`;
  }
  return conn.kind;
}

function PlugIcon() {
  return (
    <svg aria-hidden width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
      <path d="M9 2v6M15 2v6M7 8h10v3a5 5 0 0 1-10 0V8zM12 16v6" />
    </svg>
  );
}

function ServerIcon() {
  return (
    <svg aria-hidden width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
      <rect x="3" y="4" width="18" height="7" rx="1.5" />
      <rect x="3" y="13" width="18" height="7" rx="1.5" />
      <path d="M7 7.5h.01M7 16.5h.01" />
    </svg>
  );
}

export function ConnectionsPanel() {
  const [connections, setConnections] = useState<Connection[]>([]);
  const [ports, setPorts] = useState<SerialPortInfo[]>([]);
  const [mode, setMode] = useState<Mode>({ kind: 'list' });
  const [error, setError] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);
  const [confirmDeleteId, setConfirmDeleteId] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    try {
      setConnections(await connList());
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, []);

  const refreshPorts = useCallback(async () => {
    try {
      setPorts(await serialPorts());
    } catch {
      setPorts([]);
    }
  }, []);

  useEffect(() => {
    void refresh();
    void refreshPorts();
  }, [refresh, refreshPorts]);

  const startNew = useCallback(
    (kind: 'serial' | 'ssh') => {
      setError(null);
      void refreshPorts();
      setMode({ kind: 'form', draft: kind === 'serial' ? blankSerial() : blankSsh() });
    },
    [refreshPorts],
  );

  const startEdit = useCallback(
    (conn: Connection) => {
      setError(null);
      void refreshPorts();
      // Clone so editing the draft doesn't mutate the list entry until saved.
      setMode({ kind: 'form', draft: structuredClone(conn) });
    },
    [refreshPorts],
  );

  const remove = useCallback(
    async (id: string) => {
      try {
        await connDelete(id);
        setConfirmDeleteId(null);
        await refresh();
      } catch (e) {
        setError(e instanceof Error ? e.message : String(e));
      }
    },
    [refresh],
  );

  const connect = useCallback((conn: Connection) => {
    if (conn.kind === 'serial' && conn.serial != null && conn.serial.port.trim() !== '') {
      openConnection({
        connId: conn.id,
        label: conn.label || conn.serial.port,
        serial: { port: conn.serial.port, baud: conn.serial.baud },
      });
    } else if (conn.kind === 'ssh' && conn.ssh != null && conn.ssh.address.trim() !== '') {
      openConnection({
        connId: conn.id,
        label: conn.label || conn.ssh.address,
        ssh: {
          address: conn.ssh.address,
          port: conn.ssh.port,
          username: conn.ssh.username,
          password: conn.ssh.password,
        },
      });
    }
  }, []);

  const save = useCallback(async () => {
    if (mode.kind !== 'form') return;
    const draft = mode.draft;
    const label = draft.label.trim();
    if (label === '') {
      setError('Give the connection a name.');
      return;
    }
    if (draft.kind === 'serial' && (draft.serial == null || draft.serial.port.trim() === '')) {
      setError('Choose a serial port.');
      return;
    }
    if (draft.kind === 'ssh' && (draft.ssh == null || draft.ssh.address.trim() === '')) {
      setError('Enter the host address.');
      return;
    }
    setSaving(true);
    setError(null);
    try {
      const toSave: Connection = {
        ...draft,
        label,
        id: draft.id !== '' ? draft.id : crypto.randomUUID(),
      };
      await connSave(toSave);
      await refresh();
      setMode({ kind: 'list' });
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setSaving(false);
    }
  }, [mode, refresh]);

  const patchDraft = useCallback((patch: (d: Connection) => Connection) => {
    setMode((m) => (m.kind === 'form' ? { kind: 'form', draft: patch(m.draft) } : m));
  }, []);

  return (
    <section className={styles.panel} aria-label="Connections">
      {mode.kind === 'list' ? (
        <>
          <header className={styles.header}>
            <h2 className={styles.title}>Connections</h2>
            <div className={styles.headerActions}>
              <button type="button" className={styles.btn} onClick={() => startNew('serial')}>
                New serial
              </button>
              <button type="button" className={styles.btn} onClick={() => startNew('ssh')}>
                New SSH
              </button>
            </div>
          </header>

          <div className={styles.body}>
            {connections.length === 0 ? (
              <div className={styles.empty}>
                <p className={styles.emptyTitle}>No saved connections</p>
                <p className={styles.emptyHint}>
                  Save a serial port (e.g. an STM32 on COM3) or an SSH host, then connect with one
                  click.
                </p>
              </div>
            ) : (
              <ul className={styles.grid}>
                {connections.map((conn) => {
                  const isSerial = conn.kind === 'serial';
                  return (
                    <li key={conn.id} className={styles.card}>
                      <div className={styles.cardHead}>
                        <span className={styles.cardIcon}>
                          {isSerial ? <PlugIcon /> : <ServerIcon />}
                        </span>
                        <span className={styles.cardName} title={conn.label}>
                          {conn.label}
                        </span>
                        <span className={styles.cardKind}>{conn.kind}</span>
                      </div>
                      <div className={styles.cardSub}>{describe(conn)}</div>
                      <div className={styles.cardActions}>
                        <button
                          type="button"
                          className={styles.btnPrimary}
                          onClick={() => connect(conn)}
                          title="Open a session tab"
                        >
                          Connect
                        </button>
                        <button type="button" className={styles.btn} onClick={() => startEdit(conn)}>
                          Edit
                        </button>
                        {confirmDeleteId === conn.id ? (
                          <button
                            type="button"
                            className={styles.btnDanger}
                            onClick={() => void remove(conn.id)}
                          >
                            Confirm
                          </button>
                        ) : (
                          <button
                            type="button"
                            className={styles.btn}
                            onClick={() => setConfirmDeleteId(conn.id)}
                          >
                            Delete
                          </button>
                        )}
                      </div>
                    </li>
                  );
                })}
              </ul>
            )}
            {error != null ? <p className={styles.error}>{error}</p> : null}
          </div>
        </>
      ) : (
        <ConnectionForm
          draft={mode.draft}
          ports={ports}
          saving={saving}
          error={error}
          onRefreshPorts={() => void refreshPorts()}
          onPatch={patchDraft}
          onCancel={() => {
            setError(null);
            setMode({ kind: 'list' });
          }}
          onSave={() => void save()}
        />
      )}
    </section>
  );
}

type ConnectionFormProps = {
  draft: Connection;
  ports: SerialPortInfo[];
  saving: boolean;
  error: string | null;
  onRefreshPorts: () => void;
  onPatch: (patch: (d: Connection) => Connection) => void;
  onCancel: () => void;
  onSave: () => void;
};

function ConnectionForm({
  draft,
  ports,
  saving,
  error,
  onRefreshPorts,
  onPatch,
  onCancel,
  onSave,
}: ConnectionFormProps) {
  const isSerial = draft.kind === 'serial';
  const serial = draft.serial ?? { port: '', baud: 115200 };
  const ssh = draft.ssh ?? { address: '', port: 22, username: '', password: '' };
  const editing = draft.id !== '';

  return (
    <>
      <header className={styles.header}>
        <h2 className={styles.title}>{editing ? 'Edit connection' : 'New connection'}</h2>
      </header>
      <div className={styles.body}>
        <div className={styles.form}>
          <label className={styles.field}>
            <span className={styles.fieldLabel}>Name</span>
            <input
              className={styles.input}
              value={draft.label}
              placeholder={isSerial ? 'STM32 (COM3)' : 'prod-box'}
              onChange={(e) => onPatch((d) => ({ ...d, label: e.target.value }))}
              autoFocus
            />
          </label>

          <div className={styles.field}>
            <span className={styles.fieldLabel}>Type</span>
            <div className={styles.segmented} role="group" aria-label="Connection type">
              <button
                type="button"
                className={`${styles.segment} ${isSerial ? styles.segmentOn : ''}`}
                onClick={() =>
                  onPatch((d) =>
                    d.kind === 'serial' ? d : { ...d, kind: 'serial', serial, ssh: null },
                  )
                }
              >
                Serial
              </button>
              <button
                type="button"
                className={`${styles.segment} ${!isSerial ? styles.segmentOn : ''}`}
                onClick={() =>
                  onPatch((d) => (d.kind === 'ssh' ? d : { ...d, kind: 'ssh', ssh, serial: null }))
                }
              >
                SSH
              </button>
            </div>
          </div>

          {isSerial ? (
            <>
              <label className={styles.field}>
                <span className={styles.fieldLabel}>Port</span>
                <div className={styles.inlineRow}>
                  <input
                    className={styles.input}
                    list="serial-port-list"
                    value={serial.port}
                    placeholder="COM3 or /dev/ttyUSB0"
                    onChange={(e) =>
                      onPatch((d) => ({ ...d, serial: { ...serial, port: e.target.value } }))
                    }
                  />
                  <datalist id="serial-port-list">
                    {ports.map((p) => (
                      <option key={p.name} value={p.name} />
                    ))}
                  </datalist>
                  <button type="button" className={styles.btn} onClick={onRefreshPorts} title="Rescan ports">
                    Rescan
                  </button>
                </div>
              </label>
              <label className={styles.field}>
                <span className={styles.fieldLabel}>Baud rate</span>
                <select
                  className={styles.select}
                  value={serial.baud}
                  onChange={(e) =>
                    onPatch((d) => ({ ...d, serial: { ...serial, baud: Number(e.target.value) } }))
                  }
                >
                  {COMMON_BAUDS.map((b) => (
                    <option key={b} value={b}>
                      {b}
                    </option>
                  ))}
                </select>
              </label>
            </>
          ) : (
            <>
              <label className={styles.field}>
                <span className={styles.fieldLabel}>Host address</span>
                <input
                  className={styles.input}
                  value={ssh.address}
                  placeholder="192.168.1.10 or host.example.com"
                  onChange={(e) => onPatch((d) => ({ ...d, ssh: { ...ssh, address: e.target.value } }))}
                />
              </label>
              <div className={styles.inlineRow}>
                <label className={`${styles.field} ${styles.grow}`}>
                  <span className={styles.fieldLabel}>Username</span>
                  <input
                    className={styles.input}
                    value={ssh.username}
                    placeholder="root"
                    onChange={(e) =>
                      onPatch((d) => ({ ...d, ssh: { ...ssh, username: e.target.value } }))
                    }
                  />
                </label>
                <label className={styles.field} style={{ width: '6rem' }}>
                  <span className={styles.fieldLabel}>Port</span>
                  <input
                    className={styles.input}
                    type="number"
                    value={ssh.port}
                    onChange={(e) =>
                      onPatch((d) => ({ ...d, ssh: { ...ssh, port: Number(e.target.value) || 22 } }))
                    }
                  />
                </label>
              </div>
              <label className={styles.field}>
                <span className={styles.fieldLabel}>Password</span>
                <input
                  className={styles.input}
                  type="password"
                  value={ssh.password ?? ''}
                  placeholder="(or use an SSH key / agent)"
                  onChange={(e) =>
                    onPatch((d) => ({ ...d, ssh: { ...ssh, password: e.target.value } }))
                  }
                />
              </label>
            </>
          )}

          {error != null ? <p className={styles.error}>{error}</p> : null}
        </div>
      </div>
      <footer className={styles.footer}>
        <button type="button" className={styles.btn} onClick={onCancel}>
          Cancel
        </button>
        <div className={styles.footerSpacer} />
        <button type="button" className={styles.btnPrimary} onClick={onSave} disabled={saving}>
          {saving ? 'Saving…' : 'Save'}
        </button>
      </footer>
    </>
  );
}
