import type { editor } from 'monaco-editor';
import Editor from '@monaco-editor/react';
import { useEffect, useRef, useState, type CSSProperties, type MutableRefObject } from 'react';

import { monacoLanguageForPath } from '@domain/editor/editorLanguage';
import { useToolDock } from '@boundary/workspace/ToolDockContext';

import { IDE_MONACO_BASE_OPTIONS } from './ideMonacoSharedOptions';
import styles from './IdeMonacoEditor.module.css';

const kbdStyle: CSSProperties = {
  display: 'inline-block',
  padding: '1px 6px',
  fontSize: 11,
  background: 'rgba(255,255,255,0.06)',
  border: '1px solid rgba(255,255,255,0.1)',
  borderRadius: 3,
  fontFamily: 'var(--font-family-mono)',
};

/**
 * A single shortcut row inside the idle "no file open" card. Two cells in the
 * parent grid: left cell is the left-aligned action label, right cell is the
 * chord, pushed flush-right via `justify-self`. The Cursor "Quick Actions"
 * pattern — every chord ends at the same x-coordinate down the column.
 */
function FragmentRow({ label, keys }: { label: string; keys: string[] }) {
  return (
    <>
      <span>{label}</span>
      <span
        style={{
          display: 'inline-flex',
          alignItems: 'center',
          gap: 4,
          justifySelf: 'end',
        }}
      >
        {keys.map((k, i) => (
          <span key={`${k}-${i}`} style={{ display: 'inline-flex', alignItems: 'center', gap: 4 }}>
            {i > 0 ? <span style={{ opacity: 0.5 }}>+</span> : null}
            <kbd style={kbdStyle}>{k}</kbd>
          </span>
        ))}
      </span>
    </>
  );
}

/**
 * Heuristic — Qt parity. A small null-byte run or a high ratio of replacement
 * characters strongly suggests the file is binary, not text.
 */
function detectBinary(text: string): boolean {
  if (text.length === 0) {
    return false;
  }
  const sample = text.slice(0, 2048);
  let nulls = 0;
  let replacements = 0;
  for (let i = 0; i < sample.length; i++) {
    const c = sample.charCodeAt(i);
    if (c === 0) nulls += 1;
    else if (c === 0xfffd) replacements += 1;
  }
  return nulls > 0 || replacements / sample.length > 0.05;
}

export type IdeEditorCommand =
  | {
      kind: 'findInEditor';
      nonce: number;
    };

export type IdeEditorCursorPosition = {
  line: number;
  column: number;
};

export type IdeMonacoEditorProps = {
  /** The document text to show/edit (the active group's active-file buffer). */
  value: string;
  /** Called on edit with the new text. */
  onChange: (value: string) => void;
  /** Path of the file shown — drives language, binary reset, reveal targeting. */
  filePath: string | null;
  /** Reveal-a-line request (from Search); nonce-guarded so it fires once. */
  revealTarget: { path: string; line: number; nonce: number } | null;
  onCursorPositionChange?: (position: IdeEditorCursorPosition | null) => void;
  wordWrapEnabled?: boolean;
  insertSpaces?: boolean;
  tabWidth?: number;
  command?: IdeEditorCommand | null;
  fontSize?: number;
  onFontSizeChange?: (size: number) => void;
};

function wireCursorReporting(
  editorInstance: editor.IStandaloneCodeEditor,
  onCursorPositionChangeRef: MutableRefObject<
    IdeMonacoEditorProps['onCursorPositionChange']
  >,
) {
  const emitIfFocused = () => {
    if (!editorInstance.hasTextFocus()) {
      return;
    }
    const pos = editorInstance.getPosition();
    if (pos != null) {
      onCursorPositionChangeRef.current?.({ line: pos.lineNumber, column: pos.column });
    }
  };

  const disposables = [
    editorInstance.onDidChangeCursorPosition(() => {
      emitIfFocused();
    }),
    editorInstance.onDidFocusEditorWidget(() => {
      emitIfFocused();
    }),
    editorInstance.onDidBlurEditorWidget(() => {
      onCursorPositionChangeRef.current?.(null);
    }),
  ];

  return () => {
    for (const d of disposables) {
      d.dispose();
    }
  };
}

export function IdeMonacoEditor({
  onCursorPositionChange,
  wordWrapEnabled = true,
  insertSpaces = false,
  tabWidth = 4,
  command = null,
  fontSize = 14,
  onFontSizeChange,
  value,
  onChange,
  filePath,
  revealTarget,
}: IdeMonacoEditorProps) {
  const { selectToolTab } = useToolDock();
  const language = monacoLanguageForPath(filePath);
  const [forceTextMode, setForceTextMode] = useState(false);
  const isBinary = !forceTextMode && detectBinary(value);

  // Reset force-text whenever the active file changes.
  useEffect(() => {
    setForceTextMode(false);
  }, [filePath]);
  const editorRef = useRef<editor.IStandaloneCodeEditor | null>(null);
  const disposeCursorReportingRef = useRef<(() => void) | null>(null);
  const onCursorPositionChangeRef = useRef(onCursorPositionChange);
  onCursorPositionChangeRef.current = onCursorPositionChange;

  useEffect(
    () => () => {
      disposeCursorReportingRef.current?.();
      disposeCursorReportingRef.current = null;
      editorRef.current = null;
    },
    [],
  );

  useEffect(() => {
    editorRef.current?.updateOptions({ wordWrap: wordWrapEnabled ? 'on' : 'off' });
  }, [wordWrapEnabled]);

  useEffect(() => {
    editorRef.current?.updateOptions({ fontSize });
  }, [fontSize]);

  // Tab key behavior + display tab width — propagate to active model.
  useEffect(() => {
    const ed = editorRef.current;
    if (ed == null) return;
    ed.getModel()?.updateOptions({ insertSpaces, tabSize: tabWidth });
  }, [insertSpaces, tabWidth, filePath]);

  useEffect(() => {
    if (command?.kind !== 'findInEditor') {
      return;
    }
    const editorInstance = editorRef.current;
    if (editorInstance == null) {
      return;
    }
    editorInstance.focus();
    void editorInstance.getAction('actions.find')?.run();
  }, [command]);

  // Reveal a line requested by Search (openFileAtLine). Nonce-guarded so it
  // fires once per click — not again while the user edits the file.
  const lastRevealNonceRef = useRef(0);
  useEffect(() => {
    if (revealTarget == null || revealTarget.nonce === lastRevealNonceRef.current) {
      return;
    }
    const ed = editorRef.current;
    if (ed == null || filePath !== revealTarget.path) {
      return;
    }
    lastRevealNonceRef.current = revealTarget.nonce;
    ed.revealLineInCenter(revealTarget.line);
    ed.setPosition({ lineNumber: revealTarget.line, column: 1 });
    ed.focus();
  }, [revealTarget, filePath, value]);

  // No file open AND scratch buffer is empty → show a clean welcome card
  // instead of an empty Monaco. Cursor / VS Code idle pattern: muted product
  // logo centered above a two-column grid where action names align right and
  // keyboard shortcuts align left — so every `+` and every glyph sits in the
  // same column across rows.
  const noActiveFile = filePath == null || filePath === '';
  if (noActiveFile && value === '') {
    const rows: Array<{ label: string; keys: string[] }> = [
      { label: 'Open file', keys: ['Ctrl', 'O'] },
      { label: 'Open folder', keys: ['Ctrl', 'Shift', 'O'] },
      { label: 'Preferences', keys: ['Ctrl', ','] },
    ];
    return (
      <div
        className={styles.wrapper}
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          color: 'var(--color-text-primary)',
        }}
      >
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: '1.5rem',
          }}
        >
          <img
            src="/cremniy-logo.svg"
            alt=""
            aria-hidden
            style={{
              width: 100,
              height: 'auto',
              opacity: 0.18,
              filter: 'grayscale(100%)',
              userSelect: 'none',
              pointerEvents: 'none',
            }}
          />
          <div
            style={{
              display: 'grid',
              gridTemplateColumns: 'auto auto',
              alignItems: 'center',
              columnGap: 24,
              rowGap: 12,
              fontFamily: 'var(--font-family-mono)',
              fontSize: 12,
              opacity: 0.7,
            }}
          >
            {rows.map((row) => (
              <FragmentRow key={row.label} label={row.label} keys={row.keys} />
            ))}
          </div>
        </div>
      </div>
    );
  }

  if (isBinary) {
    return (
      <div className={styles.wrapper}>
        <div
          role="alert"
          style={{
            position: 'absolute',
            inset: 0,
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '0.75rem',
            background: 'rgba(0,0,0,0.55)',
            color: 'var(--color-text-primary)',
            padding: '2rem',
            textAlign: 'center',
            zIndex: 5,
          }}
        >
          <p style={{ margin: 0, fontSize: '1rem', opacity: 0.85 }}>
            Binary file detected — opening as text could corrupt it.
          </p>
          <div style={{ display: 'flex', gap: '0.5rem' }}>
            <button
              type="button"
              style={{
                padding: '0.4rem 0.9rem',
                border: '1px solid var(--color-border-muted)',
                borderRadius: 4,
                background: 'var(--color-bg-panel)',
                color: 'inherit',
                cursor: 'pointer',
                font: 'inherit',
              }}
              onClick={() => selectToolTab('binary')}
            >
              Open in HEX view
            </button>
            <button
              type="button"
              style={{
                padding: '0.4rem 0.9rem',
                border: '1px solid var(--color-border-muted)',
                borderRadius: 4,
                background: 'transparent',
                color: 'inherit',
                cursor: 'pointer',
                font: 'inherit',
              }}
              onClick={() => setForceTextMode(true)}
            >
              Open as text anyway
            </button>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className={styles.wrapper}>
      <Editor
        height="100%"
        language={language}
        path={filePath ?? undefined}
        theme="cremniy-dark"
        beforeMount={(monaco) => {
          // vs-dark marks the active line with an outline; we want a fill.
          monaco.editor.defineTheme('cremniy-dark', {
            base: 'vs-dark',
            inherit: true,
            rules: [],
            colors: {
              'editor.lineHighlightBackground': '#ffffff12',
              'editor.lineHighlightBorder': '#00000000',
            },
          });
        }}
        value={value}
        onChange={(v) => onChange(v ?? '')}
        onMount={(editorInstance) => {
          editorRef.current = editorInstance;
          editorInstance.updateOptions({
            wordWrap: wordWrapEnabled ? 'on' : 'off',
            fontSize,
          });
          // Common line-edit commands bound to keyboard shortcuts that users
          // expect from any modern editor (Qt parity + standard VS Code keys).
          // We trigger Monaco's built-in actions to keep behavior identical
          // to its built-in command palette.
          // KeyMod / KeyCode are exported by 'monaco-editor' — pull them via
          // dynamic import-side resolution (already loaded at this point).
          const monaco = (window as unknown as { monaco?: typeof import('monaco-editor') })
            .monaco;
          if (monaco != null) {
            const KM = monaco.KeyMod;
            const KC = monaco.KeyCode;
            const triggerAction = (id: string) => () => {
              void editorInstance.getAction(id)?.run();
            };
            editorInstance.addAction({
              id: 'cremniy.duplicateLine',
              label: 'Duplicate line',
              keybindings: [KM.CtrlCmd | KM.Shift | KC.KeyD],
              run: triggerAction('editor.action.copyLinesDownAction'),
            });
            editorInstance.addAction({
              id: 'cremniy.moveLineUp',
              label: 'Move line up',
              keybindings: [KM.Alt | KC.UpArrow],
              run: triggerAction('editor.action.moveLinesUpAction'),
            });
            editorInstance.addAction({
              id: 'cremniy.moveLineDown',
              label: 'Move line down',
              keybindings: [KM.Alt | KC.DownArrow],
              run: triggerAction('editor.action.moveLinesDownAction'),
            });
            editorInstance.addAction({
              id: 'cremniy.deleteLine',
              label: 'Delete line',
              keybindings: [KM.CtrlCmd | KM.Shift | KC.KeyK],
              run: triggerAction('editor.action.deleteLines'),
            });
            editorInstance.addAction({
              id: 'cremniy.toggleLineComment',
              label: 'Toggle line comment',
              keybindings: [KM.CtrlCmd | KC.Slash],
              run: triggerAction('editor.action.commentLine'),
            });
            editorInstance.addAction({
              id: 'cremniy.toggleBlockComment',
              label: 'Toggle block comment',
              keybindings: [KM.CtrlCmd | KM.Shift | KC.Slash],
              run: triggerAction('editor.action.blockComment'),
            });
            editorInstance.addAction({
              id: 'cremniy.insertLineBelow',
              label: 'Insert line below',
              keybindings: [KM.CtrlCmd | KC.Enter],
              run: triggerAction('editor.action.insertLineAfter'),
            });
            editorInstance.addAction({
              id: 'cremniy.insertLineAbove',
              label: 'Insert line above',
              keybindings: [KM.CtrlCmd | KM.Shift | KC.Enter],
              run: triggerAction('editor.action.insertLineBefore'),
            });
          }
          disposeCursorReportingRef.current?.();
          disposeCursorReportingRef.current = wireCursorReporting(
            editorInstance,
            onCursorPositionChangeRef,
          );
          // Persist Ctrl+wheel zoom — mirror Monaco's runtime fontSize back
          // into preferences so the next session restores it. Read via the
          // public getRawOptions(); typed enum constants vary between Monaco
          // versions and getOption(EditorOption.fontInfo) is brittle here.
          editorInstance.onDidChangeConfiguration(() => {
            try {
              const raw = editorInstance.getRawOptions() as { fontSize?: number };
              const candidate = raw?.fontSize;
              if (
                typeof candidate === 'number' &&
                Number.isFinite(candidate) &&
                candidate !== fontSize
              ) {
                onFontSizeChange?.(Math.round(candidate));
              }
            } catch {
              // ignore — Monaco internals are out of our control
            }
          });
        }}
        options={{
          ...IDE_MONACO_BASE_OPTIONS,
          wordWrap: wordWrapEnabled ? 'on' : 'off',
          fontSize,
          mouseWheelZoom: true,
          ariaLabel: 'Active document',
        }}
      />
    </div>
  );
}
