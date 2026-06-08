import {
  useCallback,
  useEffect,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type KeyboardEvent,
  type UIEvent,
} from 'react';

import {
  buildDisassemblyDiagnosticLog,
  filterDisassemblyRows,
  findDisassemblyRowByAddress,
  parseDisassemblyOutput,
  type DisassemblyDiagnosticEntry,
  type DisassemblyDocument,
  type DisassembleWorkspaceFile,
} from '@domain/disassembly/disassembly';
import { instructionHelpForToken, type InstructionHelp } from '@domain/disassembly/instructionHelp';
import { applyHexPatchToFile } from '@domain/disassembly/hexPatch';
import {
  extractAsciiStrings,
  indexStringsByFileOffset,
  resolveStringComment,
} from '@domain/disassembly/stringRefs';
import { parseHexByteSequence } from '@domain/hexView/hexBufferSearch';
import { fileNameFromPath } from '@domain/workspace/paths';
import { readWorkspaceFileBytes, writeWorkspaceFileBytes } from '@infrastructure/tauri/bridge';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './DisassemblerToolPanel.module.css';

const ROW_PX = 22; // Listing row height; section-header rows use this too.
const VIEWPORT_BUFFER_ROWS = 6;

type LoadState =
  | { status: 'idle'; message?: string }
  | { status: 'loading' }
  | { status: 'ready'; document: DisassemblyDocument }
  | { status: 'error'; message: string };

type DisassemblerToolPanelProps = {
  disassembleFile: DisassembleWorkspaceFile;
};

function formatUserMessage(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }
  return String(error);
}

const REG_RE =
  /^(rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|rip|eip|ip|r\d+[bwd]?|eax|ebx|ecx|edx|esi|edi|ebp|esp|ax|bx|cx|dx|si|di|bp|sp|al|bl|cl|dl|ah|bh|ch|dh|sil|dil|spl|bpl|cs|ds|es|fs|gs|ss|xmm\d+|ymm\d+|zmm\d+|mm\d+)$/i;

function highlightInstruction(text: string) {
  // Keep whitespace as its own token so the rendered output preserves spacing.
  const re = /(\s+|<[^>]+>|0x[0-9a-fA-F]+|[+-]?\d+|[\w$%.]+|\S)/g;
  const out: Array<{ text: string; color?: string; weight?: number }> = [];
  let m: RegExpExecArray | null;
  let mnemonicTaken = false;
  while ((m = re.exec(text)) != null) {
    const tok = m[0];
    if (/^\s+$/.test(tok)) {
      out.push({ text: tok });
      continue;
    }
    let color: string | undefined;
    let weight: number | undefined;
    if (tok.startsWith('<') && tok.endsWith('>')) {
      color = '#c586c0';
    } else if (REG_RE.test(tok)) {
      color = '#9cdcfe';
    } else if (/^0x[0-9a-fA-F]+$/i.test(tok) || /^-?\d+$/.test(tok)) {
      color = '#dcdcaa';
    } else if (!mnemonicTaken && /^[a-zA-Z][\w.]*$/.test(tok)) {
      color = '#dcdcaa';
      weight = 600;
      mnemonicTaken = true;
    }
    out.push({ text: tok, color, weight });
  }
  return out;
}

export function DisassemblerToolPanel({ disassembleFile }: DisassemblerToolPanelProps) {
  const { activeFilePath } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';
  const [loadState, setLoadState] = useState<LoadState | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedSection, setSelectedSection] = useState('');
  const [showDiagnostics, setShowDiagnostics] = useState(false);
  const [diagnostics, setDiagnostics] = useState<DisassemblyDiagnosticEntry[]>([]);
  const [pendingJumpRowId, setPendingJumpRowId] = useState<string | null>(null);
  const [selectedRow, setSelectedRow] = useState<{
    rowId: string;
    help: InstructionHelp | null;
    fileOffset: number | null;
    bytes: string;
  } | null>(null);
  const [patchInput, setPatchInput] = useState('');
  const [patchError, setPatchError] = useState('');
  const [patchBusy, setPatchBusy] = useState(false);
  const [stringIndex, setStringIndex] = useState<Map<number, string>>(new Map());
  const [rowCtxMenu, setRowCtxMenu] = useState<
    | { x: number; y: number; address: string; bytes: string }
    | null
  >(null);
  const runRequestIdRef = useRef(0);

  const runDisassembly = useCallback(() => {
    if (activeFilePath == null || activeFilePath === '') {
      setLoadState(null);
      setDiagnostics([]);
      return;
    }
    if (workspacePath === '') {
      setLoadState({
        status: 'error',
        message: 'Open a workspace folder to disassemble the active file.',
      });
      setDiagnostics([]);
      return;
    }

    const requestId = runRequestIdRef.current + 1;
    runRequestIdRef.current = requestId;
    setSearchQuery('');
    setSelectedSection('');
    setPendingJumpRowId(null);
    setSelectedRow(null);
    setLoadState({ status: 'loading' });

    void disassembleFile(workspacePath, activeFilePath).then(
      (result) => {
        if (runRequestIdRef.current === requestId) {
          const document = parseDisassemblyOutput(result);
          setLoadState({ status: 'ready', document });
          setDiagnostics(buildDisassemblyDiagnosticLog(result, document));
        }
      },
      (e: unknown) => {
        if (runRequestIdRef.current === requestId) {
          const message = formatUserMessage(e);
          setLoadState({ status: 'error', message });
          setDiagnostics([{ id: 'invoke-error', label: 'Disassembly failed', detail: message }]);
        }
      },
    );
  }, [activeFilePath, disassembleFile, workspacePath]);

  useEffect(() => {
    runDisassembly();

    return () => {
      runRequestIdRef.current += 1;
    };
  }, [runDisassembly]);

  const displayName =
    activeFilePath != null && activeFilePath !== '' ? fileNameFromPath(activeFilePath) : '';
  const document = loadState?.status === 'ready' ? loadState.document : null;
  const filteredRows = useMemo(
    () =>
      document == null
        ? []
        : filterDisassemblyRows(document, { sectionName: selectedSection, query: searchQuery }),
    [document, searchQuery, selectedSection],
  );

  // Windowed listing: build a flat stream of {section-header | row} items so
  // the virtual scroll only renders the visible slice. Avoids the old 2000-
  // row hard cap that silently dropped large functions off the end.
  type FlatItem =
    | { kind: 'header'; sectionName: string; key: string }
    | { kind: 'row'; listingRow: (typeof filteredRows)[number]; key: string };
  const flatItems = useMemo<FlatItem[]>(() => {
    const items: FlatItem[] = [];
    let lastSection: string | null = null;
    for (let i = 0; i < filteredRows.length; i += 1) {
      const r = filteredRows[i]!;
      if (r.sectionName !== lastSection) {
        items.push({
          kind: 'header',
          sectionName: r.sectionName,
          key: `h-${r.sectionName}-${i}`,
        });
        lastSection = r.sectionName;
      }
      items.push({ kind: 'row', listingRow: r, key: r.id });
    }
    return items;
  }, [filteredRows]);

  const listingScrollRef = useRef<HTMLDivElement>(null);
  const [listingScrollTop, setListingScrollTop] = useState(0);
  const [listingViewportHeight, setListingViewportHeight] = useState(0);

  useLayoutEffect(() => {
    const el = listingScrollRef.current;
    if (el == null) return;
    const ro = new ResizeObserver(() => setListingViewportHeight(el.clientHeight));
    ro.observe(el);
    setListingViewportHeight(el.clientHeight);
    return () => ro.disconnect();
  }, [flatItems.length > 0]);

  const totalListingHeight = flatItems.length * ROW_PX;
  const firstVisibleIndex = Math.max(
    0,
    Math.floor(listingScrollTop / ROW_PX) - VIEWPORT_BUFFER_ROWS,
  );
  const visibleItemCount =
    Math.ceil((listingViewportHeight || ROW_PX) / ROW_PX) + VIEWPORT_BUFFER_ROWS * 2;
  const lastVisibleIndex = Math.min(flatItems.length, firstVisibleIndex + visibleItemCount);
  const renderedItems = flatItems.slice(firstVisibleIndex, lastVisibleIndex);
  const renderTopPad = firstVisibleIndex * ROW_PX;

  const handleListingScroll = useCallback((e: UIEvent<HTMLDivElement>) => {
    setListingScrollTop(e.currentTarget.scrollTop);
  }, []);
  const commandLine =
    document != null
      ? `${document.metadata.executable} ${document.metadata.args.join(' ')}`
      : '';
  const hasActiveFile = activeFilePath != null && activeFilePath !== '';
  const canRun = hasActiveFile && workspacePath !== '' && loadState?.status !== 'loading';

  // After a successful disassembly, extract strings from the file so the listing
  // can auto-comment references like Qt did (; "Hello"). Best-effort: a read
  // failure just leaves comments objdump-only.
  useEffect(() => {
    if (document == null || activeFilePath == null || workspacePath === '') {
      setStringIndex(new Map());
      return;
    }
    let cancelled = false;
    void readWorkspaceFileBytes(workspacePath, activeFilePath).then(
      (bytes) => {
        if (!cancelled) {
          setStringIndex(indexStringsByFileOffset(extractAsciiStrings(bytes)));
        }
      },
      () => {
        if (!cancelled) {
          setStringIndex(new Map());
        }
      },
    );
    return () => {
      cancelled = true;
    };
  }, [document, activeFilePath, workspacePath]);

  // F5 re-runs disassembly when focus is within the panel (Qt parity).
  const onPanelKeyDown = useCallback(
    (event: KeyboardEvent<HTMLElement>) => {
      if (event.key === 'F5') {
        event.preventDefault();
        if (canRun) {
          runDisassembly();
        }
      }
    },
    [canRun, runDisassembly],
  );

  useEffect(() => {
    if (pendingJumpRowId == null) {
      return;
    }
    // Virtualised listing: compute the row's flat-item index, scroll its row
    // into view roughly centred. We can't rely on getElementById any more —
    // the row may not be in the DOM yet if it's outside the current window.
    const targetIndex = flatItems.findIndex(
      (it) => it.kind === 'row' && it.key === pendingJumpRowId,
    );
    if (targetIndex >= 0 && listingScrollRef.current != null) {
      const el = listingScrollRef.current;
      const desired = Math.max(0, targetIndex * ROW_PX - el.clientHeight / 2);
      el.scrollTo({ top: desired });
    }
    setPendingJumpRowId(null);
  }, [pendingJumpRowId, flatItems]);

  function cancelRun() {
    runRequestIdRef.current += 1;
    setLoadState({ status: 'idle', message: 'Disassembly cancelled.' });
  }

  function clearListing() {
    runRequestIdRef.current += 1;
    setLoadState({ status: 'idle', message: 'Disassembly result cleared.' });
    setSearchQuery('');
    setSelectedSection('');
    setDiagnostics([]);
    setSelectedRow(null);
  }

  // Click an instruction row to show help + (when a file offset is known) a hex
  // patch box. The panel opens if either is available.
  function selectInstructionRow(
    rowId: string,
    mnemonic: string,
    instructionText: string,
    fileOffset: number | null,
    bytes: string,
  ) {
    const help = instructionHelpForToken(mnemonic, instructionText);
    if (help == null && fileOffset == null) {
      setSelectedRow((current) => (current?.rowId === rowId ? null : current));
      return;
    }
    setSelectedRow({ rowId, help, fileOffset, bytes });
    setPatchInput(bytes);
    setPatchError('');
  }

  async function applyPatch() {
    if (selectedRow == null || activeFilePath == null || workspacePath === '') {
      return;
    }
    const parsed = parseHexByteSequence(patchInput);
    if (!parsed.ok) {
      setPatchError(parsed.message);
      return;
    }
    setPatchBusy(true);
    setPatchError('');
    try {
      const fileBytes = await readWorkspaceFileBytes(workspacePath, activeFilePath);
      const patched = applyHexPatchToFile(fileBytes, selectedRow.fileOffset, parsed.bytes);
      if (!patched.ok) {
        setPatchError(patched.message);
        return;
      }
      await writeWorkspaceFileBytes(workspacePath, activeFilePath, patched.bytes);
      setSelectedRow(null);
      runDisassembly();
    } catch (e) {
      setPatchError(formatUserMessage(e));
    } finally {
      setPatchBusy(false);
    }
  }

  function jumpToFunction(address: string) {
    if (document == null || address === '') {
      return;
    }

    const targetRow = findDisassemblyRowByAddress(document, address);
    if (targetRow == null) {
      return;
    }

    setSearchQuery('');
    setSelectedSection(targetRow.sectionName);
    setPendingJumpRowId(targetRow.id);
  }

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root} aria-label="Disassembler tool">
        <h2 className={styles.title}>Disassembler</h2>
        <p className={styles.message} role="status">
          No file is active. Open a file from the workspace tree, then select this tab to disassemble it.
        </p>
      </section>
    );
  }

  return (
    <section className={styles.root} aria-label="Disassembler tool" onKeyDown={onPanelKeyDown}>
      <header className={styles.header}>
        <div className={styles.heading}>
          <h2 className={styles.title}>Disassembler</h2>
          <p className={styles.subtitle}>{displayName}</p>
        </div>
        <div className={styles.actions}>
          <button
            type="button"
            className={styles.button}
            onClick={runDisassembly}
            disabled={!canRun}
            title="Run disassembly (F5)"
          >
            Run disassembly
          </button>
          <button
            type="button"
            className={styles.button}
            onClick={cancelRun}
            disabled={loadState?.status !== 'loading'}
          >
            Cancel
          </button>
          <button type="button" className={styles.button} onClick={clearListing}>
            Clear result
          </button>
          {loadState?.status === 'loading' ? (
            <div
              role="progressbar"
              aria-label="Disassembling"
              style={{
                position: 'relative',
                flex: 1,
                height: 3,
                background: 'rgba(255,255,255,0.05)',
                marginLeft: '0.5rem',
                overflow: 'hidden',
                borderRadius: 2,
              }}
            >
              <div
                style={{
                  position: 'absolute',
                  inset: 0,
                  background:
                    'linear-gradient(90deg, transparent, #569cd6 30%, #569cd6 70%, transparent)',
                  animation: 'disasmProgress 1.2s linear infinite',
                  width: '50%',
                }}
              />
            </div>
          ) : null}
        </div>
      </header>

      {loadState?.status === 'loading' ? (
        <p className={styles.loading} role="status" aria-live="polite">
          Disassembling…
        </p>
      ) : null}

      {loadState?.status === 'idle' && loadState.message != null ? (
        <p className={styles.message} role="status">
          {loadState.message}
        </p>
      ) : null}

      {loadState?.status === 'error' ? (
        <p className={styles.messageError} role="alert">
          {loadState.message}
        </p>
      ) : null}

      {document != null ? (
        <>
          {document.errors.length > 0 ? (
            <div className={styles.messageError} role="alert">
              {document.errors.map((error) => (
                <p key={error}>{error}</p>
              ))}
            </div>
          ) : null}

          <p className={styles.body}>
            <span>{document.sections.length} section(s)</span>
            <span>{document.functions.length} function label(s)</span>
          </p>

          <p className={styles.command} title={commandLine}>
            {commandLine}
          </p>
        </>
      ) : null}

      <div className={styles.controls} aria-label="Disassembler filters">
        <label className={styles.field}>
          <span>Search</span>
          <input
            type="search"
            value={searchQuery}
            onChange={(event) => setSearchQuery(event.target.value)}
            placeholder="Address, bytes, mnemonic, operands"
            disabled={document == null}
          />
        </label>
        <label className={styles.field}>
          <span>Section</span>
          <select
            value={selectedSection}
            onChange={(event) => setSelectedSection(event.target.value)}
            disabled={document == null || document.sections.length === 0}
          >
            <option value="">All sections</option>
            {document?.sections.map((section) => (
              <option key={section.name} value={section.name}>
                {section.name}
              </option>
            ))}
          </select>
        </label>
        <label className={styles.field}>
          <span>Function</span>
          <select
            defaultValue=""
            onChange={(event) => jumpToFunction(event.target.value)}
            disabled={document == null || document.functions.length === 0}
          >
            <option value="">Jump to function</option>
            {document?.functions.map((label) => (
              <option key={`${label.address}-${label.name}`} value={label.address}>
                {label.name}
              </option>
            ))}
          </select>
        </label>
      </div>

      <div className={styles.diagnosticActions}>
        <button
          type="button"
          className={styles.button}
          onClick={() => setShowDiagnostics((current) => !current)}
        >
          {showDiagnostics ? 'Hide diagnostic log' : 'Show diagnostic log'}
        </button>
        <button
          type="button"
          className={styles.button}
          onClick={() => setDiagnostics([])}
          disabled={diagnostics.length === 0}
        >
          Clear diagnostic log
        </button>
      </div>

      {showDiagnostics ? (
        <div className={styles.diagnostics} aria-label="Diagnostic log">
          {diagnostics.length === 0 ? (
            <p className={styles.message}>No diagnostic entries.</p>
          ) : (
            diagnostics.map((entry) => (
              <div key={entry.id} className={styles.diagnosticEntry}>
                <strong>{entry.label}</strong>
                <pre>{entry.detail}</pre>
              </div>
            ))
          )}
        </div>
      ) : null}

      {selectedRow != null ? (
        <aside className={styles.help} aria-label="Instruction details">
          <div className={styles.helpHead}>
            <strong className={styles.helpTitle}>
              {selectedRow.help?.title ?? 'Instruction'}
            </strong>
            <button
              type="button"
              className={styles.button}
              onClick={() => setSelectedRow(null)}
              aria-label="Close instruction details"
            >
              ✕
            </button>
          </div>

          {selectedRow.help != null ? (
            <>
              <p className={styles.helpDesc}>{selectedRow.help.description}</p>
              <p className={styles.helpFlags}>
                <span className={styles.helpLabel}>Флаги: </span>
                {selectedRow.help.flags.length === 0
                  ? 'не изменяет'
                  : selectedRow.help.flags.join(', ')}
              </p>
              {selectedRow.help.numbers.length > 0 ? (
                <ul className={styles.helpNumbers} aria-label="Number conversions">
                  {selectedRow.help.numbers.map((n) => (
                    <li key={n.token}>
                      <code className={styles.code}>{n.token}</code> → dec: <b>{n.dec}</b>, oct:{' '}
                      <b>{n.oct}</b>, hex: <b>{n.hex}</b>
                    </li>
                  ))}
                </ul>
              ) : null}
            </>
          ) : null}

          {selectedRow.fileOffset != null ? (
            <div className={styles.patch} aria-label="Hex patch">
              <label className={styles.patchLabel} htmlFor="disasm-hex-patch">
                Patch bytes at file offset {selectedRow.fileOffset.toString(16).padStart(8, '0')}
              </label>
              <div className={styles.patchRow}>
                <input
                  id="disasm-hex-patch"
                  className={styles.patchInput}
                  type="text"
                  value={patchInput}
                  onChange={(e) => {
                    setPatchInput(e.target.value);
                    setPatchError('');
                  }}
                  placeholder="e.g. 90 90"
                  spellCheck={false}
                  disabled={patchBusy}
                />
                <button
                  type="button"
                  className={styles.button}
                  onClick={() => void applyPatch()}
                  disabled={patchBusy}
                >
                  {patchBusy ? 'Patching…' : 'Apply'}
                </button>
              </div>
              {patchError !== '' ? (
                <p className={styles.messageError} role="alert">
                  {patchError}
                </p>
              ) : null}
            </div>
          ) : null}
        </aside>
      ) : null}

      {rowCtxMenu != null ? (
        <ul
          role="menu"
          style={{
            position: 'fixed',
            top: rowCtxMenu.y,
            left: rowCtxMenu.x,
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
          onMouseLeave={() => setRowCtxMenu(null)}
        >
          {[
            {
              label: `Copy address (${rowCtxMenu.address})`,
              onClick: () => {
                void navigator.clipboard.writeText(rowCtxMenu.address);
                setRowCtxMenu(null);
              },
            },
            {
              label: 'Copy bytes',
              onClick: () => {
                void navigator.clipboard.writeText(rowCtxMenu.bytes);
                setRowCtxMenu(null);
              },
            },
          ].map((it) => (
            <li role="none" key={it.label}>
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
                onClick={it.onClick}
              >
                {it.label}
              </button>
            </li>
          ))}
        </ul>
      ) : null}

      <div className={styles.listing} aria-label="Disassembler listing">
        <div className={styles.listingHeader}>
          <span>Offset</span>
          <span>Bytes</span>
          <span>Instruction</span>
          <span>Comment</span>
        </div>
        {document == null ? (
          <div className={styles.listingEmpty}>Run disassembly to show instructions.</div>
        ) : filteredRows.length === 0 ? (
          <div className={styles.listingEmpty}>No instructions match the current filters.</div>
        ) : (
          <div
            ref={listingScrollRef}
            className={styles.listingRows}
            role="table"
            aria-label="Disassembly rows"
            onScroll={handleListingScroll}
            style={{ overflowY: 'auto', position: 'relative' }}
          >
            <div style={{ height: totalListingHeight, position: 'relative' }}>
              <div style={{ position: 'absolute', top: renderTopPad, left: 0, right: 0 }}>
                {renderedItems.map((item) => {
                  if (item.kind === 'header') {
                    return (
                      <div
                        key={item.key}
                        className={styles.sectionHeader}
                        style={{ height: ROW_PX, lineHeight: `${ROW_PX}px` }}
                      >
                        Disassembly of section {item.sectionName}
                      </div>
                    );
                  }
                  const listingRow = item.listingRow;
                  const offset =
                    listingRow.row.fileOffset == null
                      ? listingRow.row.address
                      : listingRow.row.fileOffset.toString(16).padStart(8, '0');
                  const instruction =
                    listingRow.row.kind === 'label'
                      ? listingRow.row.mnemonic
                      : `${listingRow.row.mnemonic}${
                          listingRow.row.operands === '' ? '' : ` ${listingRow.row.operands}`
                        }`;

                  let commentText = listingRow.row.comment;
                  if (
                    commentText === '' &&
                    listingRow.row.kind === 'instruction' &&
                    document != null &&
                    stringIndex.size > 0
                  ) {
                    const resolved = resolveStringComment(
                      listingRow.row.operands,
                      document.sections,
                      stringIndex,
                    );
                    if (resolved != null) {
                      commentText = `; ${resolved}`;
                    }
                  }

                  return (
                    <div
                      key={item.key}
                      id={listingRow.id}
                      className={`${styles.listingRow} ${
                        listingRow.row.kind === 'label' ? styles.listingRowLabel : ''
                      } ${selectedRow?.rowId === listingRow.id ? styles.listingRowSelected : ''}`}
                      role="row"
                      style={{ height: ROW_PX, lineHeight: `${ROW_PX}px` }}
                      onContextMenu={
                        listingRow.row.kind === 'instruction'
                          ? (e) => {
                              e.preventDefault();
                              setRowCtxMenu({
                                x: e.clientX,
                                y: e.clientY,
                                address: listingRow.row.address,
                                bytes: listingRow.row.bytes,
                              });
                            }
                          : undefined
                      }
                      onClick={
                        listingRow.row.kind === 'instruction'
                          ? () =>
                              selectInstructionRow(
                                listingRow.id,
                                listingRow.row.mnemonic,
                                instruction,
                                listingRow.row.fileOffset,
                                listingRow.row.bytes,
                              )
                          : undefined
                      }
                    >
                      <span className={styles.offset} role="cell">
                        {offset}
                      </span>
                      <span className={styles.bytes} role="cell">
                        {listingRow.row.bytes}
                      </span>
                      <span className={styles.instruction} role="cell">
                        {listingRow.row.kind === 'instruction'
                          ? highlightInstruction(instruction).map((part, i) => (
                              <span
                                key={i}
                                style={{ color: part.color, fontWeight: part.weight }}
                              >
                                {part.text}
                              </span>
                            ))
                          : instruction}
                      </span>
                      <span className={styles.comment} role="cell">
                        {commentText}
                      </span>
                    </div>
                  );
                })}
              </div>
            </div>
          </div>
        )}
      </div>
    </section>
  );
}
