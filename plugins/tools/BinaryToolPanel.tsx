import {
  useCallback,
  useEffect,
  useId,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type MouseEvent as ReactMouseEvent,
  type UIEvent,
} from 'react';

import {
  createBinaryBufferState,
  resetBinaryBuffer,
  setBinaryBufferCurrentBytes,
  snapshotBinaryBufferBytes,
  spliceBinaryBuffer,
  type BinaryBufferState,
} from '@domain/binaryBuffer/binaryBuffer';
import {
  applyCommand as applyHexCommand,
  canRedo as stackCanRedo,
  canUndo as stackCanUndo,
  clearHistory as clearStackHistory,
  emptyHexCommandStack,
  pushCommand,
  redo as redoStack,
  undo as undoStack,
  type HexCommand,
  type HexCommandStackState,
} from '@domain/hexView/hexCommandStack';
import { clearSessionPatches, pushSessionPatch } from '@domain/hexView/sessionPatchStore';
import {
  byteSpanForRows,
  computeVisibleHexRows,
  type HexRow,
} from '@domain/hexView/hexViewModel';
import { DEFAULT_HEX_OPTIONS, type HexOptions } from '@domain/preferences/appPreferences';
import { fileNameFromPath } from '@domain/workspace/paths';
import { loadPreferences } from '@infrastructure/preferences/preferencesBridge';
import {
  MAX_INLINE_ANALYSIS_BYTES,
  getWorkspaceFileSize,
  readWorkspaceFileBytesForAnalysis,
  readWorkspaceFileChunk,
  writeWorkspaceFileBytes,
} from '@infrastructure/tauri/bridge';
import { useBinarySelection, useSetBinarySelection } from '@boundary/workspace/BinarySelectionContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import { BinaryFormatPanel } from './BinaryFormatPanel';
import { BinaryFindGoDialog } from './BinaryFindGoDialog';
import styles from './BinaryToolPanel.module.css';

const ROW_PX = 18;

// Files past this load whole-file into an editable buffer (today's behaviour).
// Anything larger switches to a read-only windowed view that only ever holds a
// slice in memory. Fall back to 16 MiB if the bridge constant is unavailable
// (e.g. a mocked bridge module in tests) so the comparison stays well-defined.
const INLINE_ANALYSIS_LIMIT =
  typeof MAX_INLINE_ANALYSIS_BYTES === 'number' ? MAX_INLINE_ANALYSIS_BYTES : 16 * 1024 * 1024;

// Rows of headroom fetched on each side of the viewport so small scrolls don't
// trigger a refetch every frame.
const WINDOW_BUFFER_ROWS = 256;
// Cap a single loaded window so a big file never holds more than this slice.
const WINDOW_MAX_BYTES = 1.5 * 1024 * 1024;
// Per-call ceiling of readWorkspaceFileChunk; larger windows loop and concat.
const CHUNK_MAX_BYTES = 4 * 1024 * 1024;
// Bytes fed to the format detector (read once from offset 0).
const FORMAT_CHUNK_BYTES = 64 * 1024;

type HexWindow = { offset: number; bytes: Uint8Array };

type PanelLoadState =
  | { status: 'loading' }
  | { status: 'ready'; buffer: BinaryBufferState }
  | { status: 'windowed'; fileSize: number }
  | { status: 'error'; message: string };

/** Read `[offset, offset+length)` from a workspace file, looping the 4 MiB-capped
 *  chunk bridge and concatenating so a window larger than one chunk still works.
 *  `length` is clamped so the read never runs past `fileSize`. */
async function readFileRange(
  workspacePath: string,
  filePath: string,
  offset: number,
  length: number,
  fileSize: number,
): Promise<Uint8Array> {
  const start = Math.max(0, Math.min(offset, fileSize));
  const end = Math.max(start, Math.min(offset + length, fileSize));
  const total = end - start;
  if (total <= 0) {
    return new Uint8Array();
  }
  const out = new Uint8Array(total);
  let read = 0;
  while (read < total) {
    const chunkLen = Math.min(CHUNK_MAX_BYTES, total - read);
    const chunk = await readWorkspaceFileChunk(workspacePath, filePath, start + read, chunkLen);
    if (chunk.length === 0) {
      // Backend returned short — stop rather than spin; return what we have.
      return out.subarray(0, read);
    }
    out.set(chunk.subarray(0, Math.min(chunk.length, total - read)), read);
    read += chunk.length;
  }
  return out;
}

type SaveTarget = {
  workspacePath: string;
  activeFilePath: string;
};

function formatUserMessage(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }
  return String(error);
}

type CopyFormat = 'raw' | 'cArray' | 'asm' | 'pyBytes' | 'spaced';

function formatBytes(bytes: Uint8Array, fmt: CopyFormat): string {
  const pairs = Array.from(bytes, (b) => b.toString(16).padStart(2, '0').toUpperCase());
  switch (fmt) {
    case 'raw':
      return pairs.join('');
    case 'spaced':
      return pairs.join(' ');
    case 'cArray':
      return `{ ${pairs.map((p) => `0x${p}`).join(', ')} }`;
    case 'asm':
      return `db ${pairs.map((p) => `0x${p}`).join(', ')}`;
    case 'pyBytes':
      return `b'${pairs.map((p) => `\\x${p.toLowerCase()}`).join('')}'`;
  }
}

/** Tint a hex pair by its byte value — null/control/printable/high-bit. */
function byteColor(pair: string): string | undefined {
  if (pair === '  ') {
    return undefined;
  }
  const v = Number.parseInt(pair, 16);
  if (!Number.isFinite(v)) {
    return undefined;
  }
  if (v === 0x00) return '#555';
  if (v < 0x20) return '#d96a6a';
  if (v === 0x7f) return '#888';
  if (v >= 0x80) return '#6c9cdc';
  return undefined; // printable ASCII — inherit
}

function formatOffsetHex(offset: number, width: number): string {
  return offset.toString(16).padStart(width, '0');
}

/** Human file size for the read-only note (MB for big files, KB below 1 MB). */
function formatFileSize(bytes: number): string {
  if (bytes >= 1024 * 1024) {
    const mb = bytes / (1024 * 1024);
    return `${mb < 10 ? mb.toFixed(1) : Math.round(mb)} MB`;
  }
  return `${Math.max(1, Math.round(bytes / 1024))} KB`;
}

function parseSingleHexByte(input: string): { ok: true; value: number } | { ok: false } {
  const trimmed = input.trim();
  if (!/^[0-9a-fA-F]{2}$/.test(trimmed)) {
    return { ok: false };
  }
  return { ok: true, value: Number.parseInt(trimmed, 16) };
}

function HexDumpRow({
  row,
  dirtyOffsets,
  editingOffset,
  editValue,
  editError,
  editErrorId,
  disabled,
  highlightStart,
  highlightEndExclusive,
  addressWidth,
  groupLength,
  hoverColumn,
  onHoverColumn,
  onContextMenu,
  onBeginEdit,
  onEditValueChange,
  onCommitEdit,
  onCancelEdit,
  commentForOffset,
}: {
  row: HexRow;
  dirtyOffsets: ReadonlySet<number>;
  editingOffset: number | null;
  editValue: string;
  editError: string;
  editErrorId: string;
  disabled: boolean;
  highlightStart: number | null;
  highlightEndExclusive: number | null;
  addressWidth: number;
  groupLength: number;
  hoverColumn: number | null;
  onHoverColumn: (col: number | null) => void;
  onContextMenu: (e: ReactMouseEvent, offset: number) => void;
  onBeginEdit: (offset: number, value: string) => void;
  onEditValueChange: (value: string) => void;
  onCommitEdit: () => boolean;
  onCancelEdit: () => void;
  commentForOffset?: (offset: number) => string | undefined;
}) {
  const hasHighlight =
    highlightStart != null &&
    highlightEndExclusive != null &&
    highlightEndExclusive > highlightStart;

  return (
    <div className={styles.hexRow}>
      <span className={styles.offset}>{formatOffsetHex(row.offset, addressWidth)}</span>
      <span className={styles.hexCells}>
        {row.hexPairs.map((pair, i) => {
          const abs = row.offset + i;
          const isGap = pair === '  ';
          const isEditing = editingOffset === abs;
          const highlighted =
            hasHighlight &&
            !isGap &&
            abs >= highlightStart! &&
            abs < highlightEndExclusive!;
          const classNames = [
            styles.hexByte,
            highlighted ? styles.hexByteHighlight : null,
            dirtyOffsets.has(abs) ? styles.hexByteDirty : null,
          ]
            .filter(Boolean)
            .join(' ');
          const showGap = groupLength > 0 && i > 0 && i % groupLength === 0;
          return (
            <span key={`${row.offset}-${i}`}>
              {showGap ? <span className={styles.hexByteGap} /> : null}
              {isGap ? (
                <span className={styles.hexByte} aria-hidden>
                  {pair}
                </span>
              ) : isEditing ? (
                <input
                  autoFocus
                  className={styles.hexByteInput}
                  aria-label={`Hex byte at offset ${formatOffsetHex(abs, addressWidth)}`}
                  aria-invalid={editError !== ''}
                  aria-describedby={editError !== '' ? editErrorId : undefined}
                  disabled={disabled}
                  maxLength={2}
                  value={editValue}
                  onChange={(e) => onEditValueChange(e.currentTarget.value)}
                  onFocus={(e) => e.currentTarget.select()}
                  onBlur={onCommitEdit}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') {
                      e.preventDefault();
                      onCommitEdit();
                    }
                    if (e.key === 'Escape') {
                      e.preventDefault();
                      onCancelEdit();
                    }
                  }}
                />
              ) : (
                <button
                  type="button"
                  className={classNames}
                  style={
                    dirtyOffsets.has(abs) || highlighted
                      ? hoverColumn === i
                        ? { boxShadow: 'inset 0 0 0 1px rgba(255,255,255,0.15)' }
                        : undefined
                      : {
                          color: byteColor(pair),
                          background:
                            hoverColumn === i ? 'rgba(255,255,255,0.06)' : undefined,
                        }
                  }
                  aria-label={`Edit byte at offset ${formatOffsetHex(abs, addressWidth)}, current value ${pair}`}
                  title={commentForOffset?.(abs)}
                  disabled={disabled}
                  onClick={() => onBeginEdit(abs, pair)}
                  onFocus={() => onBeginEdit(abs, pair)}
                  onMouseEnter={() => onHoverColumn(i)}
                  onMouseLeave={() => onHoverColumn(null)}
                  onContextMenu={(e) => onContextMenu(e, abs)}
                >
                  {pair}
                </button>
              )}
            </span>
          );
        })}
      </span>
      <span className={styles.ascii}>
        {row.ascii.split('').map((ch, i) => {
          const abs = row.offset + i;
          const pair = row.hexPairs[i] ?? '  ';
          const isGap = pair === '  ';
          const highlighted =
            hasHighlight &&
            !isGap &&
            abs >= highlightStart! &&
            abs < highlightEndExclusive!;
          return (
            <span
              key={`${row.offset}-a-${i}`}
              className={highlighted ? styles.asciiHighlight : undefined}
            >
              {ch}
            </span>
          );
        })}
      </span>
    </div>
  );
}

export function BinaryToolPanel() {
  const { activeFilePath, fileContentRevision, bumpFileContentRevision } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';
  const editErrorId = useId();
  const [hexOptions, setHexOptions] = useState<HexOptions>(DEFAULT_HEX_OPTIONS);
  const [hoverColumn, setHoverColumn] = useState<number | null>(null);
  const [ctxMenu, setCtxMenu] = useState<{ x: number; y: number; offset: number } | null>(null);

  // Read hex layout prefs once on mount and on workspace switch — Settings
  // changes during the session are picked up next time the panel mounts.
  useEffect(() => {
    let cancelled = false;
    void loadPreferences().then((prefs) => {
      if (!cancelled) {
        setHexOptions(prefs.hexOptions);
      }
    });
    return () => {
      cancelled = true;
    };
  }, [workspacePath]);

  const bytesPerRow = hexOptions.bytesPerLine;

  const [loadState, setLoadState] = useState<PanelLoadState | null>(null);
  const [cmdStack, setCmdStack] = useState<HexCommandStackState>(() => emptyHexCommandStack());
  const [editMode, setEditMode] = useState<'overwrite' | 'insert'>('overwrite');
  const [displayEndian, setDisplayEndian] = useState<'little' | 'big'>('little');
  const [metadata, setMetadata] = useState<Array<{ start: number; endExclusive: number; comment: string }>>([]);
  const [copyMenuOpen, setCopyMenuOpen] = useState(false);
  const scrollRef = useRef<HTMLDivElement>(null);
  const [scrollTop, setScrollTop] = useState(0);
  const [viewportHeight, setViewportHeight] = useState(0);
  // Windowed read-only mode (files > INLINE_ANALYSIS_LIMIT): only the visible
  // slice is held in `hexWindow`; `formatChunk` is the first chunk for format
  // detection. A ref guards against overlapping fetches racing each other.
  const [hexWindow, setHexWindow] = useState<HexWindow | null>(null);
  const [formatChunk, setFormatChunk] = useState<Uint8Array | null>(null);
  const [windowError, setWindowError] = useState('');
  const windowFetchRef = useRef(0);
  const [findGoOpen, setFindGoOpen] = useState(false);
  const [highlightRange, setHighlightRange] = useState<{
    start: number;
    endExclusive: number;
  } | null>(null);
  const [editingOffset, setEditingOffset] = useState<number | null>(null);
  const [editValue, setEditValue] = useState('');
  const [editError, setEditError] = useState('');
  const [isSaving, setIsSaving] = useState(false);
  const [saveError, setSaveError] = useState('');
  const [saveMessage, setSaveMessage] = useState('');
  const currentSaveTargetRef = useRef<{
    workspacePath: string;
    activeFilePath: string | null;
  }>({
    workspacePath,
    activeFilePath,
  });
  const saveTokenRef = useRef(0);

  useLayoutEffect(() => {
    currentSaveTargetRef.current = { workspacePath, activeFilePath };
  }, [activeFilePath, workspacePath]);

  useEffect(() => {
    if (activeFilePath == null || activeFilePath === '') {
      setLoadState(null);
      return;
    }
    if (workspacePath === '') {
      setLoadState({
        status: 'error',
        message: 'Open a workspace folder to load binary content.',
      });
      return;
    }

    let cancelled = false;
    setLoadState({ status: 'loading' });
    setHexWindow(null);
    setFormatChunk(null);
    setWindowError('');

    // Size-first: a file past the inline limit can't be slurped whole as a
    // number[] without ballooning webview memory, so it switches to a read-only
    // windowed view that only ever holds the visible slice.
    void getWorkspaceFileSize(workspacePath, activeFilePath)
      .then(async (fileSize) => {
        if (cancelled) {
          return;
        }
        if (fileSize > INLINE_ANALYSIS_LIMIT) {
          // Read-only windowed mode. Grab the first chunk up front for format
          // detection; the scroll effect fetches the visible window.
          const head = await readFileRange(
            workspacePath,
            activeFilePath,
            0,
            Math.min(FORMAT_CHUNK_BYTES, fileSize),
            fileSize,
          );
          if (cancelled) {
            return;
          }
          setFormatChunk(head);
          setLoadState({ status: 'windowed', fileSize });
          return;
        }

        // Small file: today's whole-file editable load (the analysis read also
        // re-checks the size cap, which is harmless here).
        const data = await readWorkspaceFileBytesForAnalysis(workspacePath, activeFilePath);
        if (!cancelled) {
          setLoadState({ status: 'ready', buffer: createBinaryBufferState(data) });
        }
      })
      .catch((e: unknown) => {
        if (!cancelled) {
          setLoadState({ status: 'error', message: formatUserMessage(e) });
        }
      });

    return () => {
      cancelled = true;
    };
    // Re-read on save (fileContentRevision bumps) so HEX sees the freshly-
    // written bytes if someone else saved this file (or to discard pending
    // edits that didn't go through cmdStack).
  }, [activeFilePath, workspacePath, fileContentRevision]);

  useEffect(() => {
    setScrollTop(0);
    setHighlightRange(null);
    setFindGoOpen(false);
    setEditingOffset(null);
    setEditValue('');
    setEditError('');
    setSaveError('');
    setSaveMessage('');
    setIsSaving(false);
  }, [activeFilePath, workspacePath]);

  const isCurrentSaveTarget = useCallback((target: SaveTarget, token: number): boolean => {
    const currentTarget = currentSaveTargetRef.current;
    return (
      saveTokenRef.current === token &&
      currentTarget.workspacePath === target.workspacePath &&
      currentTarget.activeFilePath === target.activeFilePath
    );
  }, []);

  const scrollToByteOffset = useCallback(
    (offset: number) => {
      const rowIndex = Math.floor(offset / bytesPerRow);
      const top = rowIndex * ROW_PX;
      const el = scrollRef.current;
      if (el != null) {
        el.scrollTop = top;
      }
      setScrollTop(top);
    },
    [bytesPerRow],
  );

  const bufferState = loadState?.status === 'ready' ? loadState.buffer : null;
  const readyData = useMemo(
    () => (bufferState == null ? null : snapshotBinaryBufferBytes(bufferState)),
    [bufferState],
  );
  const formatData = useMemo(() => {
    if (readyData == null || editingOffset == null) {
      return readyData;
    }
    const parsed = parseSingleHexByte(editValue);
    if (!parsed.ok || editingOffset < 0 || editingOffset >= readyData.length) {
      return readyData;
    }
    const nextData = new Uint8Array(readyData);
    nextData[editingOffset] = parsed.value;
    return nextData;
  }, [editValue, editingOffset, readyData]);
  const dirtyOffsets = useMemo(
    () => new Set(bufferState?.overlays.map((overlay) => overlay.offset) ?? []),
    [bufferState],
  );

  const windowedFileSize = loadState?.status === 'windowed' ? loadState.fileSize : null;
  const isWindowed = windowedFileSize != null;

  const scrollViewportMounted = (readyData != null && readyData.length > 0) || isWindowed;

  useLayoutEffect(() => {
    const el = scrollRef.current;
    if (el == null) {
      return;
    }
    const ro = new ResizeObserver(() => {
      setViewportHeight(el.clientHeight);
    });
    ro.observe(el);
    setViewportHeight(el.clientHeight);
    return () => {
      ro.disconnect();
    };
  }, [scrollViewportMounted]);

  // Content length driving the scrollbar: the loaded buffer for small files,
  // the full file size for windowed mode.
  const contentLength = isWindowed ? windowedFileSize! : (readyData?.length ?? 0);

  const totalRows = contentLength === 0 ? 0 : Math.ceil(contentLength / bytesPerRow);

  const innerHeightPx = totalRows * ROW_PX;

  const firstRowIndex = Math.floor(scrollTop / ROW_PX);
  const viewportRowCount = Math.max(
    1,
    Math.ceil((viewportHeight || ROW_PX) / ROW_PX) + 2,
  );

  const firstVisibleRowOffset = firstRowIndex * bytesPerRow;

  const visibleRows = useMemo(() => {
    if (isWindowed) {
      // Render from the loaded slice; bytes outside it render as gaps until the
      // fetch effect fills the window in.
      const windowBytes = hexWindow?.bytes ?? new Uint8Array();
      return computeVisibleHexRows({
        data: windowBytes,
        bufferStartOffset: hexWindow?.offset ?? 0,
        startOffset: firstVisibleRowOffset,
        bytesPerRow,
        viewportRowCount,
        totalByteLength: windowedFileSize!,
      });
    }
    if (readyData == null || readyData.length === 0) {
      return [];
    }
    return computeVisibleHexRows({
      data: readyData,
      bufferStartOffset: 0,
      startOffset: firstVisibleRowOffset,
      bytesPerRow,
      viewportRowCount,
      totalByteLength: readyData.length,
    });
  }, [
    isWindowed,
    hexWindow,
    windowedFileSize,
    readyData,
    firstVisibleRowOffset,
    viewportRowCount,
    bytesPerRow,
  ]);

  // Windowed mode: on mount and on scroll/resize, fetch the byte window covering
  // the viewport (± buffer rows) if the current window doesn't already cover it.
  useEffect(() => {
    if (!isWindowed || activeFilePath == null || activeFilePath === '' || workspacePath === '') {
      return;
    }
    const fileSize = windowedFileSize!;

    const bufferBytes = byteSpanForRows(WINDOW_BUFFER_ROWS, bytesPerRow);
    const viewportBytes = byteSpanForRows(viewportRowCount, bytesPerRow);
    const needStart = Math.max(0, firstVisibleRowOffset - bufferBytes);
    const needEnd = Math.min(fileSize, firstVisibleRowOffset + viewportBytes + bufferBytes);

    const windowEnd = hexWindow == null ? -1 : hexWindow.offset + hexWindow.bytes.length;
    const covered =
      hexWindow != null &&
      hexWindow.offset <= needStart &&
      // Covers the needed range, or already reaches EOF (can't grow further — a
      // short read at the tail of the file must not trigger an endless refetch).
      (windowEnd >= needEnd || windowEnd >= fileSize);
    if (covered || needEnd <= needStart) {
      return;
    }

    // Anchor the fetch a buffer ahead of the viewport and take a generous window
    // (capped at WINDOW_MAX_BYTES) so nearby scrolling stays in the loaded slice.
    const fetchStart = needStart;
    const fetchLength = Math.min(WINDOW_MAX_BYTES, fileSize - fetchStart);
    const token = windowFetchRef.current + 1;
    windowFetchRef.current = token;

    let cancelled = false;
    void readFileRange(workspacePath, activeFilePath, fetchStart, fetchLength, fileSize)
      .then((bytes) => {
        if (cancelled || windowFetchRef.current !== token) {
          return;
        }
        setHexWindow({ offset: fetchStart, bytes });
        setWindowError('');
      })
      .catch((e: unknown) => {
        if (cancelled || windowFetchRef.current !== token) {
          return;
        }
        setWindowError(formatUserMessage(e));
      });

    return () => {
      cancelled = true;
    };
  }, [
    isWindowed,
    windowedFileSize,
    activeFilePath,
    workspacePath,
    firstVisibleRowOffset,
    viewportRowCount,
    bytesPerRow,
    hexWindow,
  ]);

  const handleScroll = useCallback((e: UIEvent<HTMLDivElement>) => {
    setScrollTop(e.currentTarget.scrollTop);
  }, []);

  const commitEditingByte = useCallback((): boolean => {
    if (editingOffset == null) {
      return true;
    }

    const parsed = parseSingleHexByte(editValue);
    if (!parsed.ok) {
      setEditError('Enter exactly two hex digits (00-ff).');
      return false;
    }

    setLoadState((current) => {
      if (current?.status !== 'ready') {
        return current;
      }
      const currentBytes = snapshotBinaryBufferBytes(current.buffer);
      const cmd: HexCommand =
        editMode === 'insert'
          ? { kind: 'insert', offset: editingOffset, bytes: new Uint8Array([parsed.value]) }
          : (() => {
              const oldByte = currentBytes[editingOffset];
              return {
                kind: 'replace',
                offset: editingOffset,
                oldBytes: new Uint8Array([oldByte ?? 0]),
                newBytes: new Uint8Array([parsed.value]),
              };
            })();
      const nextBytes = applyHexCommand(currentBytes, cmd);
      setCmdStack((prev) => pushCommand(prev, cmd));
      if (activeFilePath != null && activeFilePath !== '') {
        pushSessionPatch(activeFilePath, cmd);
      }
      return {
        status: 'ready',
        buffer: setBinaryBufferCurrentBytes(current.buffer, nextBytes),
      };
    });
    setEditingOffset(null);
    setEditValue('');
    setEditError('');
    setSaveError('');
    setSaveMessage('');
    setHighlightRange(null);
    return true;
  }, [editMode, editValue, editingOffset]);

  const handleReplaceFromDialog = useCallback(
    (offset: number, oldLength: number, newBytes: Uint8Array) => {
      setLoadState((current) => {
        if (current?.status !== 'ready') {
          return current;
        }
        const currentBytes = snapshotBinaryBufferBytes(current.buffer);
        const oldBytes = currentBytes.subarray(offset, offset + oldLength);
        const cmd: HexCommand =
          oldLength === 0
            ? { kind: 'insert', offset, bytes: new Uint8Array(newBytes) }
            : newBytes.length === 0
            ? { kind: 'remove', offset, removed: new Uint8Array(oldBytes) }
            : {
                kind: 'replace',
                offset,
                oldBytes: new Uint8Array(oldBytes),
                newBytes: new Uint8Array(newBytes),
              };
        setCmdStack((prev) => pushCommand(prev, cmd));
        if (activeFilePath != null && activeFilePath !== '') {
          pushSessionPatch(activeFilePath, cmd);
        }
        return {
          status: 'ready',
          buffer: spliceBinaryBuffer(current.buffer, offset, oldLength, newBytes),
        };
      });
    },
    [],
  );

  const handleUndo = useCallback(() => {
    setLoadState((current) => {
      if (current?.status !== 'ready') {
        return current;
      }
      const bytes = snapshotBinaryBufferBytes(current.buffer);
      const result = undoStack(cmdStack, bytes);
      if (result.command == null) {
        return current;
      }
      setCmdStack(result.state);
      return { status: 'ready', buffer: setBinaryBufferCurrentBytes(current.buffer, result.buffer) };
    });
  }, [cmdStack]);

  const handleRedo = useCallback(() => {
    setLoadState((current) => {
      if (current?.status !== 'ready') {
        return current;
      }
      const bytes = snapshotBinaryBufferBytes(current.buffer);
      const result = redoStack(cmdStack, bytes);
      if (result.command == null) {
        return current;
      }
      setCmdStack(result.state);
      return { status: 'ready', buffer: setBinaryBufferCurrentBytes(current.buffer, result.buffer) };
    });
  }, [cmdStack]);

  // Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z for hex undo/redo. Only fires when focus
  // is inside the binary panel — capture handler so Monaco editor in another
  // tab doesn't eat the event.
  useEffect(() => {
    const onKey = (ev: KeyboardEvent) => {
      const tag = (ev.target as HTMLElement | null)?.tagName ?? '';
      if (tag === 'INPUT' || tag === 'TEXTAREA') {
        return;
      }
      if (!(ev.ctrlKey || ev.metaKey)) {
        return;
      }
      const key = ev.key.toLowerCase();
      if (key === 'z' && !ev.shiftKey) {
        ev.preventDefault();
        handleUndo();
      } else if (key === 'y' || (key === 'z' && ev.shiftKey)) {
        ev.preventDefault();
        handleRedo();
      }
    };
    const onInsertKey = (ev: KeyboardEvent) => {
      const tag = (ev.target as HTMLElement | null)?.tagName ?? '';
      if (tag === 'INPUT' || tag === 'TEXTAREA') {
        return;
      }
      if (ev.key === 'Insert') {
        ev.preventDefault();
        setEditMode((m) => (m === 'overwrite' ? 'insert' : 'overwrite'));
      }
    };
    window.addEventListener('keydown', onInsertKey);

    // Keyboard navigation in the HEX grid (no edit mode required) — Qt parity.
    // Skipped while editing a byte input or typing in a search field.
    const onNavKey = (ev: KeyboardEvent) => {
      const tag = (ev.target as HTMLElement | null)?.tagName ?? '';
      if (tag === 'INPUT' || tag === 'TEXTAREA') return;
      if (ev.ctrlKey || ev.metaKey || ev.altKey) return;
      const len = readyData?.length ?? 0;
      if (len === 0) return;
      const curr = highlightRange?.start ?? 0;
      const viewRows = Math.max(1, Math.floor(viewportHeight / ROW_PX));
      let next: number | null = null;
      switch (ev.key) {
        case 'ArrowLeft':
          next = Math.max(0, curr - 1);
          break;
        case 'ArrowRight':
          next = Math.min(len - 1, curr + 1);
          break;
        case 'ArrowUp':
          next = Math.max(0, curr - bytesPerRow);
          break;
        case 'ArrowDown':
          next = Math.min(len - 1, curr + bytesPerRow);
          break;
        case 'Home':
          next = curr - (curr % bytesPerRow);
          break;
        case 'End':
          next = Math.min(len - 1, curr - (curr % bytesPerRow) + bytesPerRow - 1);
          break;
        case 'PageUp':
          next = Math.max(0, curr - viewRows * bytesPerRow);
          break;
        case 'PageDown':
          next = Math.min(len - 1, curr + viewRows * bytesPerRow);
          break;
      }
      if (next != null) {
        ev.preventDefault();
        setHighlightRange({ start: next, endExclusive: next + 1 });
        scrollToByteOffset(next);
      }
    };
    window.addEventListener('keydown', onNavKey);

    // Ctrl+M — attach a comment to the highlighted range.
    const onCommentKey = (ev: KeyboardEvent) => {
      const tag = (ev.target as HTMLElement | null)?.tagName ?? '';
      if (tag === 'INPUT' || tag === 'TEXTAREA') {
        return;
      }
      if ((ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === 'm') {
        ev.preventDefault();
        setHighlightRange((range) => {
          if (range == null) {
            return range;
          }
          const text = window.prompt(
            `Comment for bytes 0x${range.start.toString(16)}–0x${(range.endExclusive - 1).toString(
              16,
            )}:`,
          );
          if (text != null && text.trim() !== '') {
            setMetadata((prev) =>
              prev
                .filter((m) => m.start !== range.start || m.endExclusive !== range.endExclusive)
                .concat({ start: range.start, endExclusive: range.endExclusive, comment: text.trim() }),
            );
          }
          return range;
        });
      }
    };
    window.addEventListener('keydown', onCommentKey);
    window.addEventListener('keydown', onKey);
    return () => {
      window.removeEventListener('keydown', onKey);
      window.removeEventListener('keydown', onInsertKey);
      window.removeEventListener('keydown', onCommentKey);
      window.removeEventListener('keydown', onNavKey);
    };
  }, [handleRedo, handleUndo, readyData, highlightRange, viewportHeight, bytesPerRow, scrollToByteOffset]);

  const commentForOffset = useCallback(
    (offset: number): string | undefined =>
      metadata.find((m) => offset >= m.start && offset < m.endExclusive)?.comment,
    [metadata],
  );

  const copyAs = useCallback(
    async (fmt: CopyFormat) => {
      const bytes = readyData;
      if (bytes == null) {
        return;
      }
      const start = highlightRange?.start ?? 0;
      const end = highlightRange?.endExclusive ?? Math.min(bytes.length, start + 1);
      const slice = bytes.subarray(start, end);
      try {
        await navigator.clipboard.writeText(formatBytes(slice, fmt));
        setCopyMenuOpen(false);
      } catch {
        // ignore clipboard failure
      }
    },
    [highlightRange, readyData],
  );

  const beginEditingByte = useCallback(
    (offset: number, value: string) => {
      if (isSaving || editingOffset === offset) {
        return;
      }
      if (editingOffset != null && !commitEditingByte()) {
        return;
      }
      setEditingOffset(offset);
      setEditValue(value);
      setEditError('');
      setSaveError('');
      setSaveMessage('');
    },
    [commitEditingByte, editingOffset, isSaving],
  );

  const cancelEditingByte = useCallback(() => {
    setEditingOffset(null);
    setEditValue('');
    setEditError('');
  }, []);

  const handleEditValueChange = useCallback((value: string) => {
    setEditValue(value);
    setEditError('');
  }, []);

  const handleGoToOffset = useCallback(
    (offset: number) => {
      setHighlightRange(null);
      scrollToByteOffset(offset);
    },
    [scrollToByteOffset],
  );

  const setSharedSelection = useSetBinarySelection();
  const sharedSelection = useBinarySelection();

  const handleSelectRange = useCallback(
    (offset: number, length: number) => {
      setHighlightRange({ start: offset, endExclusive: offset + length });
      scrollToByteOffset(offset);
      setSharedSelection({ offset, length, source: 'hex' });
    },
    [scrollToByteOffset, setSharedSelection],
  );

  // Sync incoming selection from other tabs (Symbols / Functions / Disasm).
  useEffect(() => {
    if (sharedSelection == null || sharedSelection.source === 'hex') {
      return;
    }
    setHighlightRange({
      start: sharedSelection.offset,
      endExclusive: sharedSelection.offset + Math.max(1, sharedSelection.length),
    });
    scrollToByteOffset(sharedSelection.offset);
  }, [sharedSelection, scrollToByteOffset]);

  const buildEditedBufferForSave = useCallback((): BinaryBufferState | null => {
    if (loadState?.status !== 'ready') {
      return null;
    }
    if (editingOffset == null) {
      return loadState.buffer;
    }

    const parsed = parseSingleHexByte(editValue);
    if (!parsed.ok) {
      setEditError('Enter exactly two hex digits (00-ff).');
      return null;
    }

    const currentBytes = snapshotBinaryBufferBytes(loadState.buffer);
    const oldByte = currentBytes[editingOffset];
    if (oldByte == null) {
      return null;
    }
    const cmd: HexCommand = {
      kind: 'replace',
      offset: editingOffset,
      oldBytes: new Uint8Array([oldByte]),
      newBytes: new Uint8Array([parsed.value]),
    };
    const nextBytes = applyHexCommand(currentBytes, cmd);
    setCmdStack((prev) => pushCommand(prev, cmd));
    if (activeFilePath != null && activeFilePath !== '') {
      pushSessionPatch(activeFilePath, cmd);
    }
    const nextBuffer = setBinaryBufferCurrentBytes(loadState.buffer, nextBytes);
    setLoadState({ status: 'ready', buffer: nextBuffer });
    setEditingOffset(null);
    setEditValue('');
    setEditError('');
    setHighlightRange(null);
    return nextBuffer;
  }, [editValue, editingOffset, loadState]);

  const handleSave = useCallback(async () => {
    if (activeFilePath == null || activeFilePath === '' || workspacePath === '') {
      return;
    }

    const bufferForSave = buildEditedBufferForSave();
    if (bufferForSave == null || !bufferForSave.isDirty) {
      return;
    }

    const bytes = snapshotBinaryBufferBytes(bufferForSave);
    const saveTarget = { workspacePath, activeFilePath };
    const saveToken = saveTokenRef.current + 1;
    saveTokenRef.current = saveToken;
    setIsSaving(true);
    setSaveError('');
    setSaveMessage('');
    try {
      await writeWorkspaceFileBytes(saveTarget.workspacePath, saveTarget.activeFilePath, bytes);
      if (!isCurrentSaveTarget(saveTarget, saveToken)) {
        return;
      }
      setLoadState({ status: 'ready', buffer: createBinaryBufferState(bytes) });
      setCmdStack((prev) => clearStackHistory(prev));
      if (activeFilePath != null && activeFilePath !== '') {
        clearSessionPatches(activeFilePath);
      }
      bumpFileContentRevision();
      setSaveMessage('Saved.');
    } catch (e: unknown) {
      if (!isCurrentSaveTarget(saveTarget, saveToken)) {
        return;
      }
      setSaveError(formatUserMessage(e));
    } finally {
      if (isCurrentSaveTarget(saveTarget, saveToken)) {
        setIsSaving(false);
      }
    }
  }, [activeFilePath, buildEditedBufferForSave, isCurrentSaveTarget, workspacePath]);

  const handleReset = useCallback(() => {
    setLoadState((current) => {
      if (current?.status !== 'ready') {
        return current;
      }
      return { status: 'ready', buffer: resetBinaryBuffer(current.buffer) };
    });
    setCmdStack(emptyHexCommandStack());
    setEditingOffset(null);
    setEditValue('');
    setEditError('');
    setSaveError('');
    setSaveMessage('');
    setHighlightRange(null);
  }, []);

  const displayName =
    activeFilePath != null && activeFilePath !== '' ? fileNameFromPath(activeFilePath) : '';

  // Shared hex grid (column header + virtualized scroll body). Read-only in
  // windowed mode — HexDumpRow's `disabled` blocks the edit affordances, and the
  // gap bytes outside the loaded window render until the fetch effect fills it.
  const hexGrid = (
    <>
      <div
        className={styles.hexRow}
        aria-hidden
        style={{ opacity: 0.6, borderBottom: '1px solid rgba(255,255,255,0.07)' }}
      >
        <span className={styles.offset}>{'Offset'.padStart(hexOptions.addressWidth, ' ')}</span>
        <span className={styles.hexCells}>
          {Array.from({ length: bytesPerRow }, (_, i) => {
            const showGap = hexOptions.groupLength > 0 && i > 0 && i % hexOptions.groupLength === 0;
            const label = i.toString(16).padStart(2, '0').toUpperCase();
            return (
              <span key={`hd-${i}`}>
                {showGap ? <span className={styles.hexByteGap} /> : null}
                <span
                  className={styles.hexByte}
                  style={{
                    background: hoverColumn === i ? 'rgba(255,255,255,0.08)' : undefined,
                  }}
                >
                  {label}
                </span>
              </span>
            );
          })}
        </span>
        <span className={styles.ascii}>
          {Array.from({ length: bytesPerRow }, (_, i) => i.toString(16).toUpperCase()[0] ?? '·').join('')}
        </span>
      </div>
      <div
        key={activeFilePath}
        ref={scrollRef}
        className={styles.scroll}
        onScroll={handleScroll}
      >
        <div className={styles.scrollInner} style={{ height: Math.max(innerHeightPx, 1) }}>
          <div className={styles.hexViewport} style={{ top: firstRowIndex * ROW_PX }}>
            {visibleRows.map((row) => (
              <HexDumpRow
                key={row.offset}
                row={row}
                dirtyOffsets={dirtyOffsets}
                editingOffset={editingOffset}
                editValue={editValue}
                editError={editError}
                editErrorId={editErrorId}
                disabled={isSaving || isWindowed}
                highlightStart={highlightRange?.start ?? null}
                highlightEndExclusive={highlightRange?.endExclusive ?? null}
                addressWidth={hexOptions.addressWidth}
                groupLength={hexOptions.groupLength}
                hoverColumn={hoverColumn}
                onHoverColumn={setHoverColumn}
                onContextMenu={(e, offset) => {
                  e.preventDefault();
                  setCtxMenu({ x: e.clientX, y: e.clientY, offset });
                }}
                commentForOffset={commentForOffset}
                onBeginEdit={beginEditingByte}
                onEditValueChange={handleEditValueChange}
                onCommitEdit={commitEditingByte}
                onCancelEdit={cancelEditingByte}
              />
            ))}
          </div>
        </div>
      </div>
    </>
  );

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root} aria-label="Binary tool">
        <h2 className={styles.title}>Binary / hex</h2>
        <p className={styles.message} role="status">
          No file is active. Open a file from the workspace tree, then select this tab to view hex and ASCII.
        </p>
      </section>
    );
  }

  return (
    <section className={styles.root} aria-label="Binary tool">
      <h2 className={styles.title}>Binary / hex</h2>
      <p className={styles.subtitle}>{displayName}</p>

      {loadState?.status === 'loading' ? (
        <p className={styles.loading} role="status">
          Loading…
        </p>
      ) : null}

      {loadState?.status === 'error' ? (
        <p className={styles.messageError} role="alert">
          {loadState.message}
        </p>
      ) : null}

      {readyData != null && readyData.length === 0 ? (
        <p className={styles.message} role="status">
          This file is empty (0 bytes).
        </p>
      ) : null}

      {readyData != null ? <BinaryFormatPanel bytes={formatData ?? readyData} /> : null}
      {isWindowed && formatChunk != null ? <BinaryFormatPanel bytes={formatChunk} /> : null}

      {isWindowed ? (
        <>
          <p className={styles.subtitle} role="status">
            Read-only windowed view — {formatFileSize(windowedFileSize!)}. The whole file is too
            large to edit inline; scroll to load more.
          </p>
          {windowError !== '' ? (
            <p className={styles.messageError} role="alert">
              {windowError}
            </p>
          ) : null}
          {hexGrid}
        </>
      ) : null}

      {bufferState != null && readyData != null && readyData.length > 0 ? (
        <>
          <div className={styles.toolbar}>
            <button
              type="button"
              className={styles.toolbarBtn}
              onClick={() => setFindGoOpen(true)}
            >
              Find / go to…
            </button>
            <button
              type="button"
              className={styles.toolbarBtn}
              disabled={
                editError !== '' || (!bufferState.isDirty && editingOffset == null) || isSaving
              }
              onClick={handleSave}
            >
              {isSaving ? 'Saving…' : 'Save'}
            </button>
            <button
              type="button"
              className={styles.toolbarBtn}
              disabled={(!bufferState.isDirty && editingOffset == null) || isSaving}
              onClick={handleReset}
            >
              Reset
            </button>
            <button
              type="button"
              className={styles.toolbarBtn}
              disabled={!stackCanUndo(cmdStack) || isSaving}
              onClick={handleUndo}
              title="Undo (Ctrl+Z)"
            >
              Undo
            </button>
            <button
              type="button"
              className={styles.toolbarBtn}
              disabled={!stackCanRedo(cmdStack) || isSaving}
              onClick={handleRedo}
              title="Redo (Ctrl+Y / Ctrl+Shift+Z)"
            >
              Redo
            </button>
            <button
              type="button"
              className={styles.toolbarBtn}
              onClick={() => setEditMode((m) => (m === 'overwrite' ? 'insert' : 'overwrite'))}
              title="Toggle edit mode (Insert key)"
              disabled={isSaving}
            >
              {editMode === 'overwrite' ? 'OVR' : 'INS'}
            </button>
            <button
              type="button"
              className={styles.toolbarBtn}
              onClick={() => setDisplayEndian((e) => (e === 'little' ? 'big' : 'little'))}
              title="Toggle display endianness for multi-byte values"
              disabled={isSaving}
            >
              {displayEndian === 'little' ? 'LE' : 'BE'}
            </button>
            <div style={{ position: 'relative' }}>
              <button
                type="button"
                className={styles.toolbarBtn}
                onClick={() => setCopyMenuOpen((v) => !v)}
                title="Copy bytes in various formats"
                disabled={isSaving || readyData == null}
              >
                Copy ▾
              </button>
              {copyMenuOpen ? (
                <ul
                  role="menu"
                  style={{
                    position: 'absolute',
                    top: '100%',
                    left: 0,
                    margin: 0,
                    padding: '0.25rem 0',
                    listStyle: 'none',
                    background: 'var(--color-bg-panel)',
                    border: '1px solid var(--color-border-pane)',
                    borderRadius: 4,
                    boxShadow: '0 0.5rem 1rem rgba(0,0,0,0.45)',
                    zIndex: 60,
                    minWidth: '12rem',
                  }}
                  onMouseLeave={() => setCopyMenuOpen(false)}
                >
                  {(
                    [
                      ['raw', 'Raw hex (DEADBEEF)'],
                      ['spaced', 'Spaced hex (DE AD BE EF)'],
                      ['cArray', 'C array { 0xDE, … }'],
                      ['asm', 'Asm db 0xDE, …'],
                      ['pyBytes', "Python b'\\xde…'"],
                    ] as Array<[CopyFormat, string]>
                  ).map(([fmt, label]) => (
                    <li role="none" key={fmt}>
                      <button
                        type="button"
                        role="menuitem"
                        style={{
                          display: 'block',
                          width: '100%',
                          padding: '0.3rem 0.65rem',
                          border: 'none',
                          background: 'transparent',
                          color: 'inherit',
                          textAlign: 'left',
                          font: 'inherit',
                          cursor: 'pointer',
                        }}
                        onClick={() => void copyAs(fmt)}
                      >
                        {label}
                      </button>
                    </li>
                  ))}
                </ul>
              ) : null}
            </div>
            <span className={styles.dirtyStatus} role="status">
              {bufferState.isDirty || (editingOffset != null && editError === '')
                ? 'Unsaved changes'
                : saveMessage || 'Saved'}
            </span>
          </div>
          {editError !== '' ? (
            <p id={editErrorId} className={styles.messageError} role="alert">
              {editError}
            </p>
          ) : null}
          {saveError !== '' ? (
            <p className={styles.messageError} role="alert">
              {saveError}
            </p>
          ) : null}
          {hexGrid}
        </>
      ) : null}

      {ctxMenu != null ? (
        <ul
          role="menu"
          style={{
            position: 'fixed',
            top: ctxMenu.y,
            left: ctxMenu.x,
            margin: 0,
            padding: '0.25rem 0',
            listStyle: 'none',
            background: 'var(--color-bg-panel)',
            border: '1px solid var(--color-border-pane)',
            borderRadius: 4,
            boxShadow: '0 0.5rem 1rem rgba(0,0,0,0.45)',
            zIndex: 100,
            minWidth: '10rem',
            fontSize: 13,
          }}
          onMouseLeave={() => setCtxMenu(null)}
        >
          {[
            {
              label: `Go to offset 0x${ctxMenu.offset.toString(16)}`,
              onClick: () => {
                scrollToByteOffset(ctxMenu.offset);
                setCtxMenu(null);
              },
            },
            {
              label: 'Copy hex byte',
              onClick: () => {
                if (readyData != null) {
                  const b = readyData[ctxMenu.offset];
                  if (b != null) {
                    void navigator.clipboard.writeText(b.toString(16).padStart(2, '0').toUpperCase());
                  }
                }
                setCtxMenu(null);
              },
            },
            {
              label: 'Copy selection (hex)',
              onClick: () => {
                void copyAs('spaced');
                setCtxMenu(null);
              },
            },
            {
              label: 'Copy selection (C array)',
              onClick: () => {
                void copyAs('cArray');
                setCtxMenu(null);
              },
            },
            {
              label: 'Find / go to…',
              onClick: () => {
                setCtxMenu(null);
                setFindGoOpen(true);
              },
            },
            {
              label: 'Select byte (highlight)',
              onClick: () => {
                handleSelectRange(ctxMenu.offset, 1);
                setCtxMenu(null);
              },
            },
          ].map((item) => (
            <li role="none" key={item.label}>
              <button
                type="button"
                role="menuitem"
                style={{
                  display: 'block',
                  width: '100%',
                  padding: '0.3rem 0.65rem',
                  border: 'none',
                  background: 'transparent',
                  color: 'inherit',
                  textAlign: 'left',
                  font: 'inherit',
                  cursor: 'pointer',
                }}
                onClick={item.onClick}
              >
                {item.label}
              </button>
            </li>
          ))}
        </ul>
      ) : null}

      {findGoOpen && readyData != null && readyData.length > 0 ? (
        <BinaryFindGoDialog
          buffer={readyData}
          cursorOffset={highlightRange?.start ?? 0}
          onClose={() => setFindGoOpen(false)}
          onGoToOffset={handleGoToOffset}
          onSelectRange={handleSelectRange}
          onReplace={handleReplaceFromDialog}
        />
      ) : null}
    </section>
  );
}
