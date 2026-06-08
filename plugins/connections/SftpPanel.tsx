import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import {
  connList,
  createEmptyFileUnderWorkspace,
  listDirectoryEntries,
  pickFolder,
  readWorkspaceFileBytes,
  sftpClose,
  sftpList,
  sftpOpen,
  sftpRead,
  sftpWrite,
  writeWorkspaceFileBytes,
  type Connection,
  type SftpEntry,
} from '@infrastructure/tauri/bridge';

import styles from './SftpPanel.module.css';

/**
 * SFTP — a dual-pane file browser over SSH (the file-transfer half of the
 * Connections pack; the interactive-shell half is the SSH terminal). Pick a
 * saved SSH host, Connect, then move files between the local machine (LEFT) and
 * the remote host (RIGHT).
 *
 * LEFT pane uses the workspace-scoped fs bridge rooted at a user-picked folder
 * (the backend confines every op to that root); navigation stays inside it,
 * "Change…" re-roots elsewhere. RIGHT pane drives the sftp_* bridge.
 *
 * Backend sessions live in `source/backend/src/sftp.rs`; this panel is wired in
 * as a 2nd centerPanel via the plugin manifest (index.tsx).
 */

const SFTP_SESSION_ID = 'sftp-panel';

type LocalItem = { name: string; path: string; isDir: boolean };

/** "/a/b/c" → "/a/b" (remote, POSIX). Root stays root. */
function remoteParent(path: string): string {
  if (path === '/' || path === '') return '/';
  const trimmed = path.replace(/\/+$/, '');
  const idx = trimmed.lastIndexOf('/');
  if (idx <= 0) return '/';
  return trimmed.slice(0, idx);
}

/** Join a remote dir + child name with a single "/". */
function remoteJoin(dir: string, name: string): string {
  if (dir === '' || dir === '/') return `/${name}`;
  return `${dir.replace(/\/+$/, '')}/${name}`;
}

/** Native path separator for the local side (Windows uses "\\"). */
function localSep(root: string): string {
  return root.includes('\\') ? '\\' : '/';
}

function localJoin(dir: string, name: string): string {
  const sep = localSep(dir);
  return dir.endsWith(sep) ? `${dir}${name}` : `${dir}${sep}${name}`;
}

function FolderIcon() {
  return (
    <svg aria-hidden width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
      <path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V7z" />
    </svg>
  );
}

function FileIcon() {
  return (
    <svg aria-hidden width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
      <path d="M6 3h7l5 5v13a0 0 0 0 1 0 0H6a0 0 0 0 1 0 0V3z" />
      <path d="M13 3v5h5" />
    </svg>
  );
}

function errText(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

function humanSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  const units = ['KB', 'MB', 'GB', 'TB'];
  let v = bytes / 1024;
  let i = 0;
  while (v >= 1024 && i < units.length - 1) {
    v /= 1024;
    i += 1;
  }
  return `${v < 10 ? v.toFixed(1) : Math.round(v)} ${units[i]}`;
}

export function SftpPanel() {
  const [hosts, setHosts] = useState<Connection[]>([]);
  const [selectedHostId, setSelectedHostId] = useState<string>('');
  const [connected, setConnected] = useState(false);
  const [connecting, setConnecting] = useState(false);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  // Local (LEFT) pane: rooted at a picked folder; the backend confines ops to it.
  const [localRoot, setLocalRoot] = useState<string>('');
  const [localDir, setLocalDir] = useState<string>('');
  const [localItems, setLocalItems] = useState<LocalItem[]>([]);
  const [localSelected, setLocalSelected] = useState<LocalItem | null>(null);

  // Remote (RIGHT) pane.
  const [remoteDir, setRemoteDir] = useState<string>('.');
  const [remoteItems, setRemoteItems] = useState<SftpEntry[]>([]);
  const [remoteSelected, setRemoteSelected] = useState<SftpEntry | null>(null);

  // Close the session if the panel unmounts while connected.
  const connectedRef = useRef(false);
  connectedRef.current = connected;
  useEffect(() => {
    return () => {
      if (connectedRef.current) void sftpClose(SFTP_SESSION_ID).catch(() => {});
    };
  }, []);

  // Saved SSH hosts for the dropdown.
  useEffect(() => {
    void (async () => {
      try {
        const all = await connList();
        const ssh = all.filter((c) => c.kind === 'ssh' && c.ssh != null);
        setHosts(ssh);
        if (ssh.length > 0) setSelectedHostId((id) => (id === '' ? ssh[0].id : id));
      } catch (e) {
        setError(errText(e));
      }
    })();
  }, []);

  const selectedHost = useMemo(
    () => hosts.find((h) => h.id === selectedHostId) ?? null,
    [hosts, selectedHostId],
  );

  const loadLocal = useCallback(async (root: string, dir: string) => {
    const entries = await listDirectoryEntries(root, dir);
    setLocalItems(entries.map((e) => ({ name: e.name, path: e.path, isDir: e.isDirectory })));
    setLocalDir(dir);
    setLocalSelected(null);
  }, []);

  const loadRemote = useCallback(async (dir: string) => {
    const entries = await sftpList(SFTP_SESSION_ID, dir);
    setRemoteItems(entries);
    setRemoteSelected(null);
    // Resolve "." to the absolute home the first time so ".." works.
    if (dir === '.' && entries.length > 0) {
      setRemoteDir(remoteParent(entries[0].path));
    } else {
      setRemoteDir(dir);
    }
  }, []);

  const pickLocalRoot = useCallback(async () => {
    try {
      const picked = await pickFolder();
      if (picked == null || picked === '') return;
      setError(null);
      setLocalRoot(picked);
      await loadLocal(picked, picked);
    } catch (e) {
      setError(errText(e));
    }
  }, [loadLocal]);

  const connect = useCallback(async () => {
    if (selectedHost?.ssh == null) {
      setError('Pick a saved SSH host first.');
      return;
    }
    setConnecting(true);
    setError(null);
    try {
      // Re-use one session id; close any stale session before opening.
      if (connectedRef.current) await sftpClose(SFTP_SESSION_ID).catch(() => {});
      await sftpOpen(
        SFTP_SESSION_ID,
        selectedHost.ssh.address,
        selectedHost.ssh.port,
        selectedHost.ssh.username,
        selectedHost.ssh.password,
      );
      setConnected(true);
      await loadRemote('.');
    } catch (e) {
      setConnected(false);
      setError(errText(e));
    } finally {
      setConnecting(false);
    }
  }, [selectedHost, loadRemote]);

  const disconnect = useCallback(async () => {
    try {
      await sftpClose(SFTP_SESSION_ID);
    } catch {
      /* ignore */
    }
    setConnected(false);
    setRemoteItems([]);
    setRemoteSelected(null);
    setRemoteDir('.');
  }, []);

  const openLocalItem = useCallback(
    (item: LocalItem) => {
      if (item.isDir) {
        void loadLocal(localRoot, item.path).catch((e) => setError(errText(e)));
      } else {
        setLocalSelected(item);
      }
    },
    [loadLocal, localRoot],
  );

  const localUp = useCallback(() => {
    // Clamp to the picked root — never browse above it (the backend rejects it
    // anyway). Use "Change…" to re-root elsewhere.
    if (localDir === localRoot || localRoot === '') return;
    const sep = localSep(localRoot);
    const idx = localDir.replace(/[\\/]+$/, '').lastIndexOf(sep);
    const parent = idx > localRoot.length ? localDir.slice(0, idx) : localRoot;
    void loadLocal(localRoot, parent).catch((e) => setError(errText(e)));
  }, [localDir, localRoot, loadLocal]);

  const openRemoteItem = useCallback(
    (item: SftpEntry) => {
      if (item.isDir) {
        void loadRemote(item.path).catch((e) => setError(errText(e)));
      } else {
        setRemoteSelected(item);
      }
    },
    [loadRemote],
  );

  const remoteUp = useCallback(() => {
    void loadRemote(remoteParent(remoteDir)).catch((e) => setError(errText(e)));
  }, [remoteDir, loadRemote]);

  // Download: remote file → local root's current dir.
  const download = useCallback(async () => {
    if (remoteSelected == null || remoteSelected.isDir) return;
    if (localRoot === '') {
      setError('Pick a local folder first (Change…).');
      return;
    }
    setBusy(`Downloading ${remoteSelected.name}…`);
    setError(null);
    try {
      const bytes = await sftpRead(SFTP_SESSION_ID, remoteSelected.path);
      const dest = localJoin(localDir, remoteSelected.name);
      // writeWorkspaceFileBytes needs an existing regular file; create it first
      // if it isn't there yet (overwrite is allowed).
      try {
        await createEmptyFileUnderWorkspace(localRoot, dest);
      } catch {
        /* already exists → overwrite below */
      }
      await writeWorkspaceFileBytes(localRoot, dest, bytes);
      await loadLocal(localRoot, localDir);
    } catch (e) {
      setError(errText(e));
    } finally {
      setBusy(null);
    }
  }, [remoteSelected, localRoot, localDir, loadLocal]);

  // Upload: local file → remote current dir.
  const upload = useCallback(async () => {
    if (localSelected == null || localSelected.isDir) return;
    if (!connected) {
      setError('Connect to a host first.');
      return;
    }
    setBusy(`Uploading ${localSelected.name}…`);
    setError(null);
    try {
      const bytes = await readWorkspaceFileBytes(localRoot, localSelected.path);
      const dest = remoteJoin(remoteDir, localSelected.name);
      await sftpWrite(SFTP_SESSION_ID, dest, bytes);
      await loadRemote(remoteDir);
    } catch (e) {
      setError(errText(e));
    } finally {
      setBusy(null);
    }
  }, [localSelected, connected, localRoot, remoteDir, loadRemote]);

  return (
    <section className={styles.panel} aria-label="SFTP">
      <header className={styles.header}>
        <h2 className={styles.title}>SFTP</h2>
        <div className={styles.headerActions}>
          <select
            className={styles.select}
            value={selectedHostId}
            onChange={(e) => setSelectedHostId(e.target.value)}
            disabled={connected || connecting}
            aria-label="SSH host"
          >
            {hosts.length === 0 ? (
              <option value="">No saved SSH hosts</option>
            ) : (
              hosts.map((h) => (
                <option key={h.id} value={h.id}>
                  {h.label || h.ssh?.address}
                </option>
              ))
            )}
          </select>
          {connected ? (
            <button type="button" className={styles.btn} onClick={() => void disconnect()}>
              Disconnect
            </button>
          ) : (
            <button
              type="button"
              className={styles.btnPrimary}
              onClick={() => void connect()}
              disabled={connecting || hosts.length === 0}
            >
              {connecting ? 'Connecting…' : 'Connect'}
            </button>
          )}
        </div>
      </header>

      <div className={styles.body}>
        <div className={styles.panes}>
          {/* LEFT — local */}
          <Pane
            label="Local"
            path={localRoot === '' ? '(no folder)' : localDir}
            onUp={localUp}
            upDisabled={localRoot === '' || localDir === localRoot}
            actionLabel="Change…"
            onAction={() => void pickLocalRoot()}
          >
            {localRoot === '' ? (
              <div className={styles.empty}>
                <p className={styles.emptyHint}>Pick a local folder to browse and transfer files.</p>
              </div>
            ) : (
              <ul className={styles.list}>
                {localItems.map((item) => (
                  <li
                    key={item.path}
                    className={`${styles.row} ${
                      !item.isDir && localSelected?.path === item.path ? styles.rowSelected : ''
                    }`}
                    onClick={() => (item.isDir ? undefined : setLocalSelected(item))}
                    onDoubleClick={() => openLocalItem(item)}
                    title={item.path}
                  >
                    <span className={styles.rowIcon}>{item.isDir ? <FolderIcon /> : <FileIcon />}</span>
                    <span className={styles.rowName}>{item.name}</span>
                  </li>
                ))}
                {localItems.length === 0 ? <li className={styles.rowEmpty}>empty</li> : null}
              </ul>
            )}
          </Pane>

          {/* Transfer controls */}
          <div className={styles.transfer}>
            <button
              type="button"
              className={styles.btn}
              onClick={() => void upload()}
              disabled={!connected || localSelected == null || localSelected.isDir || busy != null}
              title="Upload selected local file to the remote folder"
            >
              {'→'}
            </button>
            <span className={styles.transferHint}>upload</span>
            <button
              type="button"
              className={styles.btn}
              onClick={() => void download()}
              disabled={
                !connected ||
                remoteSelected == null ||
                remoteSelected.isDir ||
                localRoot === '' ||
                busy != null
              }
              title="Download selected remote file to the local folder"
            >
              {'←'}
            </button>
            <span className={styles.transferHint}>download</span>
          </div>

          {/* RIGHT — remote */}
          <Pane
            label="Remote"
            path={connected ? remoteDir : '(not connected)'}
            onUp={remoteUp}
            upDisabled={!connected || remoteDir === '/'}
          >
            {!connected ? (
              <div className={styles.empty}>
                <p className={styles.emptyHint}>Connect to a saved SSH host to browse its files.</p>
              </div>
            ) : (
              <ul className={styles.list}>
                {remoteItems.map((item) => (
                  <li
                    key={item.path}
                    className={`${styles.row} ${
                      !item.isDir && remoteSelected?.path === item.path ? styles.rowSelected : ''
                    }`}
                    onClick={() => (item.isDir ? undefined : setRemoteSelected(item))}
                    onDoubleClick={() => openRemoteItem(item)}
                    title={item.path}
                  >
                    <span className={styles.rowIcon}>{item.isDir ? <FolderIcon /> : <FileIcon />}</span>
                    <span className={styles.rowName}>{item.name}</span>
                    {!item.isDir ? <span className={styles.rowSize}>{humanSize(item.size)}</span> : null}
                  </li>
                ))}
                {remoteItems.length === 0 ? <li className={styles.rowEmpty}>empty</li> : null}
              </ul>
            )}
          </Pane>
        </div>

        {busy != null ? <p className={styles.status}>{busy}</p> : null}
        {error != null ? <p className={styles.error}>{error}</p> : null}
      </div>
    </section>
  );
}

type PaneProps = {
  label: string;
  path: string;
  onUp: () => void;
  upDisabled: boolean;
  actionLabel?: string;
  onAction?: () => void;
  children: React.ReactNode;
};

function Pane({ label, path, onUp, upDisabled, actionLabel, onAction, children }: PaneProps) {
  return (
    <div className={styles.pane}>
      <div className={styles.paneHead}>
        <span className={styles.paneLabel}>{label}</span>
        {actionLabel != null && onAction != null ? (
          <button type="button" className={styles.btnGhost} onClick={onAction}>
            {actionLabel}
          </button>
        ) : null}
      </div>
      <div className={styles.pathBar}>
        <button type="button" className={styles.upBtn} onClick={onUp} disabled={upDisabled} title="Up one level">
          ..
        </button>
        <span className={styles.pathText} title={path}>
          {path}
        </span>
      </div>
      <div className={styles.paneBody}>{children}</div>
    </div>
  );
}
