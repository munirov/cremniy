import {
  useCallback,
  useEffect,
  useId,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type UIEvent,
} from 'react';

import {
  createBinaryBufferState,
  replaceBinaryBufferByte,
  resetBinaryBuffer,
  snapshotBinaryBufferBytes,
  type BinaryBufferState,
} from '@domain/binaryBuffer/binaryBuffer';
import { findAllSubsequenceIndices } from '@domain/hexView/hexBufferSearch';
import { computeVisibleHexRows, type HexRow } from '@domain/hexView/hexViewModel';
import { fileNameFromPath } from '@domain/workspace/paths';
import { readWorkspaceFileBytes, writeWorkspaceFileBytes } from '@infrastructure/tauri/bridge';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import { BinaryFormatPanel } from './BinaryFormatPanel';
import { BinaryFindGoDialog } from './BinaryFindGoDialog';
import styles from './BinaryToolPanel.module.css';

const BYTES_PER_ROW = 16;
const ROW_PX = 18;

type PanelLoadState =
  | { status: 'loading' }
  | { status: 'ready'; buffer: BinaryBufferState }
  | { status: 'error'; message: string };

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

function formatOffsetHex(offset: number): string {
  return offset.toString(16).padStart(8, '0');
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
  onBeginEdit,
  onEditValueChange,
  onCommitEdit,
  onCancelEdit,
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
  onBeginEdit: (offset: number, value: string) => void;
  onEditValueChange: (value: string) => void;
  onCommitEdit: () => boolean;
  onCancelEdit: () => void;
}) {
  const hasHighlight =
    highlightStart != null &&
    highlightEndExclusive != null &&
    highlightEndExclusive > highlightStart;

  return (
    <div className={styles.hexRow}>
      <span className={styles.offset}>{formatOffsetHex(row.offset)}</span>
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
          return (
            <span key={`${row.offset}-${i}`}>
              {i === 8 ? <span className={styles.hexByteGap} /> : null}
              {isGap ? (
                <span className={styles.hexByte} aria-hidden>
                  {pair}
                </span>
              ) : isEditing ? (
                <input
                  autoFocus
                  className={styles.hexByteInput}
                  aria-label={`Hex byte at offset ${formatOffsetHex(abs)}`}
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
                  aria-label={`Edit byte at offset ${formatOffsetHex(abs)}, current value ${pair}`}
                  disabled={disabled}
                  onClick={() => onBeginEdit(abs, pair)}
                  onFocus={() => onBeginEdit(abs, pair)}
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
  const { activeFilePath } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';
  const editErrorId = useId();

  const [loadState, setLoadState] = useState<PanelLoadState | null>(null);
  const scrollRef = useRef<HTMLDivElement>(null);
  const [scrollTop, setScrollTop] = useState(0);
  const [viewportHeight, setViewportHeight] = useState(0);
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

    // Bytes loaded via Tauri, like WorkspaceFileTree.
    void readWorkspaceFileBytes(workspacePath, activeFilePath).then(
      (data) => {
        if (!cancelled) {
          setLoadState({ status: 'ready', buffer: createBinaryBufferState(data) });
        }
      },
      (e: unknown) => {
        if (!cancelled) {
          setLoadState({ status: 'error', message: formatUserMessage(e) });
        }
      },
    );

    return () => {
      cancelled = true;
    };
  }, [activeFilePath, workspacePath]);

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

  const scrollToByteOffset = useCallback((offset: number) => {
    const rowIndex = Math.floor(offset / BYTES_PER_ROW);
    const top = rowIndex * ROW_PX;
    const el = scrollRef.current;
    if (el != null) {
      el.scrollTop = top;
    }
    setScrollTop(top);
  }, []);

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

  const scrollViewportMounted = readyData != null && readyData.length > 0;

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

  const totalRows =
    readyData == null || readyData.length === 0 ? 0 : Math.ceil(readyData.length / BYTES_PER_ROW);

  const innerHeightPx = totalRows * ROW_PX;

  const firstRowIndex = Math.floor(scrollTop / ROW_PX);
  const viewportRowCount = Math.max(
    1,
    Math.ceil((viewportHeight || ROW_PX) / ROW_PX) + 2,
  );

  const visibleRows = useMemo(() => {
    if (readyData == null || readyData.length === 0) {
      return [];
    }
    return computeVisibleHexRows({
      data: readyData,
      bufferStartOffset: 0,
      startOffset: firstRowIndex * BYTES_PER_ROW,
      bytesPerRow: BYTES_PER_ROW,
      viewportRowCount,
      totalByteLength: readyData.length,
    });
  }, [readyData, firstRowIndex, viewportRowCount]);

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
      return {
        status: 'ready',
        buffer: replaceBinaryBufferByte(current.buffer, editingOffset, parsed.value),
      };
    });
    setEditingOffset(null);
    setEditValue('');
    setEditError('');
    setSaveError('');
    setSaveMessage('');
    setHighlightRange(null);
    return true;
  }, [editValue, editingOffset]);

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

  const handleFindBytes = useCallback(
    (needle: Uint8Array): boolean => {
      if (readyData == null) {
        return false;
      }
      const indices = findAllSubsequenceIndices(readyData, needle);
      if (indices.length === 0) {
        return false;
      }
      const start = indices[0]!;
      setHighlightRange({ start, endExclusive: start + needle.length });
      scrollToByteOffset(start);
      return true;
    },
    [readyData, scrollToByteOffset],
  );

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

    const nextBuffer = replaceBinaryBufferByte(loadState.buffer, editingOffset, parsed.value);
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
    setEditingOffset(null);
    setEditValue('');
    setEditError('');
    setSaveError('');
    setSaveMessage('');
    setHighlightRange(null);
  }, []);

  const displayName =
    activeFilePath != null && activeFilePath !== '' ? fileNameFromPath(activeFilePath) : '';

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
                    disabled={isSaving}
                    highlightStart={highlightRange?.start ?? null}
                    highlightEndExclusive={highlightRange?.endExclusive ?? null}
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
      ) : null}

      {findGoOpen && readyData != null && readyData.length > 0 ? (
        <BinaryFindGoDialog
          bufferLength={readyData.length}
          onClose={() => setFindGoOpen(false)}
          onFindBytes={handleFindBytes}
          onGoToOffset={handleGoToOffset}
        />
      ) : null}
    </section>
  );
}
