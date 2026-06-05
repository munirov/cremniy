import { useCallback, useEffect, useState } from 'react';

import type {
  AppPreferences,
  DisassemblySyntaxPreference,
  LocalePreference,
  ThemePreference,
} from '@domain/preferences/appPreferences';
import {
  DEFAULT_DISASSEMBLY_PREFERENCES,
  DEFAULT_HEX_OPTIONS,
  MAX_DISASSEMBLY_INSTRUCTION_LIMIT,
  MIN_DISASSEMBLY_INSTRUCTION_LIMIT,
  normalizeDisassemblyPreferences,
  type HexOptions,
} from '@domain/preferences/appPreferences';
import { LOCALES } from '@domain/i18n/translations';
import type { SettingsService } from '@domain/preferences/settingsService';
import { testExternalTool } from '@infrastructure/tauri/bridge';

import styles from './SettingsDialog.module.css';

export type SettingsDialogProps = {
  open: boolean;
  onClose: () => void;
  onSaved?: (prefs: AppPreferences) => void;
  workspaceRoot?: string | null;
  service: SettingsService;
};

export function SettingsDialog({ open, onClose, onSaved, workspaceRoot, service }: SettingsDialogProps) {
  const [theme, setTheme] = useState<ThemePreference>('dark');
  const [locale, setLocale] = useState<LocalePreference>('en');
  const [terminalPanelVisible, setTerminalPanelVisible] = useState(true);
  const [editorWordWrap, setEditorWordWrap] = useState(true);
  const [excludedFilePatterns, setExcludedFilePatterns] = useState('');
  const [objdumpPath, setObjdumpPath] = useState('');
  const [archHint, setArchHint] = useState('');
  const [instructionLimit, setInstructionLimit] = useState(
    String(DEFAULT_DISASSEMBLY_PREFERENCES.instructionLimit),
  );
  const [syntax, setSyntax] = useState<DisassemblySyntaxPreference>(
    DEFAULT_DISASSEMBLY_PREFERENCES.syntax,
  );
  const [backend, setBackend] = useState<'objdump' | 'radare2'>('objdump');
  const [radare2Path, setRadare2Path] = useState('');
  const [radare2AnalysisLevel, setRadare2AnalysisLevel] = useState<'none' | 'aa' | 'aaa'>('none');
  const [radare2PreCommands, setRadare2PreCommands] = useState('');
  const [hexOptions, setHexOptions] = useState<HexOptions>(DEFAULT_HEX_OPTIONS);
  const [objdumpStatus, setObjdumpStatus] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) {
      return;
    }
    let cancelled = false;
    setError(null);
    setBusy(true);
    void service.loadPreferences().then(
      (p) => {
        if (!cancelled) {
          setTheme(p.theme);
          setLocale(p.locale);
          setTerminalPanelVisible(p.terminalPanelVisible);
          setEditorWordWrap(p.editorWordWrap);
          setExcludedFilePatterns(p.excludedFilePatterns);
          setHexOptions(p.hexOptions);
          setObjdumpPath(p.disassembly.objdumpPath);
          setArchHint(p.disassembly.archHint);
          setInstructionLimit(String(p.disassembly.instructionLimit));
          setSyntax(p.disassembly.syntax);
          setBackend(p.disassembly.backend);
          setRadare2Path(p.disassembly.radare2Path);
          setRadare2AnalysisLevel(p.disassembly.radare2AnalysisLevel);
          setRadare2PreCommands(p.disassembly.radare2PreCommands);
          setObjdumpStatus(null);
        }
      },
      () => {
        if (!cancelled) {
          setError('Could not load preferences');
        }
      },
    ).finally(() => {
      if (!cancelled) {
        setBusy(false);
      }
    });
    return () => {
      cancelled = true;
    };
  }, [open, service]);

  const handleSave = useCallback(async () => {
    setError(null);
    setBusy(true);
    try {
      const current = await service.loadPreferences();
      const parsedInstructionLimit = Number.parseInt(instructionLimit, 10);
      const next: AppPreferences = {
        ...current,
        theme,
        locale,
        terminalPanelVisible,
        editorWordWrap,
        excludedFilePatterns,
        hexOptions,
        disassembly: normalizeDisassemblyPreferences({
          ...current.disassembly,
          backend,
          objdumpPath: objdumpPath.trim(),
          archHint: archHint.trim(),
          instructionLimit: Number.isFinite(parsedInstructionLimit)
            ? parsedInstructionLimit
            : DEFAULT_DISASSEMBLY_PREFERENCES.instructionLimit,
          syntax,
          radare2Path: radare2Path.trim(),
          radare2AnalysisLevel,
          radare2PreCommands,
        }),
      };
      await service.savePreferences(next);
      onSaved?.(next);
      onClose();
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  }, [archHint, backend, editorWordWrap, excludedFilePatterns, hexOptions, instructionLimit, locale, objdumpPath, onClose, onSaved, radare2AnalysisLevel, radare2Path, radare2PreCommands, service, syntax, theme, terminalPanelVisible]);

  const handleExport = useCallback(async () => {
    setError(null);
    setBusy(true);
    try {
      const path = await service.exportPreferences();
      if (path != null) {
        setObjdumpStatus(`Settings exported to ${path}`);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  }, [service]);

  const handleImport = useCallback(async () => {
    setError(null);
    setBusy(true);
    try {
      const imported = await service.importPreferences();
      if (imported != null) {
        // Reflect imported values into the open dialog.
        setTheme(imported.theme);
        setTerminalPanelVisible(imported.terminalPanelVisible);
        setEditorWordWrap(imported.editorWordWrap);
        setObjdumpPath(imported.disassembly.objdumpPath);
        setArchHint(imported.disassembly.archHint);
        setInstructionLimit(String(imported.disassembly.instructionLimit));
        setSyntax(imported.disassembly.syntax);
        setObjdumpStatus('Settings imported.');
        onSaved?.(imported);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  }, [onSaved, service]);

  const handleTestObjdump = useCallback(async () => {
    setError(null);
    setObjdumpStatus(null);
    setBusy(true);
    try {
      const message = await service.testObjdumpTool(workspaceRoot, objdumpPath);
      setObjdumpStatus(message);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  }, [objdumpPath, service, workspaceRoot]);

  // Generic external-tool tests for r2 / file / nasm. Shared with M17 backend.
  const runToolTest = useCallback(
    async (name: string, path?: string) => {
      setError(null);
      setObjdumpStatus(null);
      setBusy(true);
      try {
        const message = await testExternalTool(name, path ?? null);
        setObjdumpStatus(message);
      } catch (e) {
        setError(e instanceof Error ? e.message : String(e));
      } finally {
        setBusy(false);
      }
    },
    [],
  );

  if (!open) {
    return null;
  }

  return (
    <div className={styles.backdrop} role="presentation" onMouseDown={(e) => e.target === e.currentTarget && onClose()}>
      <div
        className={styles.dialog}
        role="dialog"
        aria-modal="true"
        aria-labelledby="settings-dialog-title"
        onMouseDown={(e) => e.stopPropagation()}
      >
        <h2 id="settings-dialog-title" className={styles.title}>
          Preferences
        </h2>

        <div className={styles.field}>
          <span className={styles.label}>Theme</span>
          <div className={styles.row}>
            <label className={styles.radio}>
              <input type="radio" name="theme" checked={theme === 'dark'} onChange={() => setTheme('dark')} />
              Dark
            </label>
            <label className={styles.radio}>
              <input type="radio" name="theme" checked={theme === 'light'} onChange={() => setTheme('light')} />
              Light
            </label>
          </div>
        </div>

        <div className={styles.field}>
          <label className={styles.label} htmlFor="settings-locale">
            Language
          </label>
          <select
            id="settings-locale"
            className={styles.input}
            value={locale}
            onChange={(e) => setLocale(e.target.value as LocalePreference)}
          >
            {LOCALES.map((l) => (
              <option key={l.id} value={l.id}>
                {l.label}
              </option>
            ))}
          </select>
        </div>

        <div className={styles.field}>
          <label className={styles.checkbox}>
            <input
              type="checkbox"
              checked={terminalPanelVisible}
              onChange={(e) => setTerminalPanelVisible(e.target.checked)}
            />
            Show terminal panel
          </label>
        </div>

        <div className={styles.field}>
          <label className={styles.checkbox}>
            <input
              type="checkbox"
              checked={editorWordWrap}
              onChange={(e) => setEditorWordWrap(e.target.checked)}
            />
            Word wrap in editor
          </label>
        </div>

        <section className={styles.section} aria-labelledby="settings-hex-title">
          <h3 id="settings-hex-title" className={styles.sectionTitle}>
            Hex viewer
          </h3>
          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-hex-bytes-per-line">
              Bytes per line
            </label>
            <select
              id="settings-hex-bytes-per-line"
              className={styles.input}
              value={hexOptions.bytesPerLine}
              onChange={(e) =>
                setHexOptions((o) => ({ ...o, bytesPerLine: Number(e.target.value) }))
              }
            >
              <option value={8}>8</option>
              <option value={16}>16</option>
              <option value={32}>32</option>
            </select>
          </div>
          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-hex-address-width">
              Address width (hex digits)
            </label>
            <select
              id="settings-hex-address-width"
              className={styles.input}
              value={hexOptions.addressWidth}
              onChange={(e) =>
                setHexOptions((o) => ({ ...o, addressWidth: Number(e.target.value) }))
              }
            >
              <option value={8}>8 (32-bit)</option>
              <option value={16}>16 (64-bit)</option>
            </select>
          </div>
          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-hex-group-length">
              Group length (gap after N bytes)
            </label>
            <select
              id="settings-hex-group-length"
              className={styles.input}
              value={hexOptions.groupLength}
              onChange={(e) =>
                setHexOptions((o) => ({ ...o, groupLength: Number(e.target.value) }))
              }
            >
              <option value={1}>1</option>
              <option value={2}>2</option>
              <option value={4}>4</option>
              <option value={8}>8</option>
            </select>
          </div>
        </section>

        <section className={styles.section} aria-labelledby="settings-disassembly-title">
          <h3 id="settings-disassembly-title" className={styles.sectionTitle}>
            Disassembly tooling
          </h3>

          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-disassembly-backend">
              Disassembler backend
            </label>
            <select
              id="settings-disassembly-backend"
              className={styles.input}
              value={backend}
              onChange={(e) => setBackend(e.target.value as 'objdump' | 'radare2')}
            >
              <option value="objdump">embedded (iced-x86 + goblin)</option>
              <option value="radare2">radare2 (external)</option>
            </select>
            <p className={styles.helpText}>
              {backend === 'objdump'
                ? 'Built-in x86 / x86-64 decoder — no external tool required.'
                : 'Use external radare2 (r2). ARM / MIPS / RISC-V via the same tool. Backend wiring is pending.'}
            </p>
            {objdumpStatus != null ? <p className={styles.success}>{objdumpStatus}</p> : null}
            <button
              type="button"
              className={styles.btn}
              onClick={() => void handleTestObjdump()}
              disabled={busy}
              style={{ marginTop: '0.5rem' }}
            >
              Self-check
            </button>
          </div>

          {backend === 'radare2' ? (
            <>
              <div className={styles.field}>
                <label className={styles.label} htmlFor="settings-r2-path">
                  radare2 path
                </label>
                <input
                  id="settings-r2-path"
                  className={styles.input}
                  type="text"
                  value={radare2Path}
                  onChange={(e) => setRadare2Path(e.target.value)}
                  placeholder="Empty to search PATH (r2 / radare2)"
                />
                <button
                  type="button"
                  className={styles.btn}
                  disabled={busy}
                  onClick={() => void runToolTest('r2', radare2Path)}
                  style={{ marginTop: '0.35rem' }}
                >
                  Test radare2
                </button>
              </div>
              <div className={styles.field}>
                <label className={styles.label} htmlFor="settings-r2-aa">
                  Analysis level
                </label>
                <select
                  id="settings-r2-aa"
                  className={styles.input}
                  value={radare2AnalysisLevel}
                  onChange={(e) =>
                    setRadare2AnalysisLevel(e.target.value as 'none' | 'aa' | 'aaa')
                  }
                >
                  <option value="none">none — no analysis</option>
                  <option value="aa">aa — basic analysis</option>
                  <option value="aaa">aaa — full analysis</option>
                </select>
              </div>
              <div className={styles.field}>
                <label className={styles.label} htmlFor="settings-r2-pre">
                  Pre-commands (one per line)
                </label>
                <textarea
                  id="settings-r2-pre"
                  className={styles.input}
                  rows={3}
                  value={radare2PreCommands}
                  onChange={(e) => setRadare2PreCommands(e.target.value)}
                  placeholder="e.g. e asm.syntax=intel"
                />
              </div>
            </>
          ) : null}

          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-instruction-limit">
              Instruction/render limit
            </label>
            <input
              id="settings-instruction-limit"
              className={styles.input}
              type="number"
              min={MIN_DISASSEMBLY_INSTRUCTION_LIMIT}
              max={MAX_DISASSEMBLY_INSTRUCTION_LIMIT}
              step={50}
              value={instructionLimit}
              onChange={(e) => setInstructionLimit(e.target.value)}
            />
          </div>

          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-disassembly-syntax">
              Syntax preference
            </label>
            <select
              id="settings-disassembly-syntax"
              className={styles.input}
              value={syntax}
              onChange={(e) => setSyntax(e.target.value as DisassemblySyntaxPreference)}
            >
              <option value="intel">Intel</option>
              <option value="att">AT&amp;T</option>
            </select>
            <p className={styles.helpText}>Used for x86 objdump output when the selected backend supports it.</p>
          </div>
        </section>

        <section className={styles.section} aria-labelledby="settings-files-title">
          <h3 id="settings-files-title" className={styles.sectionTitle}>
            Files
          </h3>
          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-excluded-patterns">
              Excluded patterns (one per line — substring match)
            </label>
            <textarea
              id="settings-excluded-patterns"
              className={styles.input}
              rows={4}
              value={excludedFilePatterns}
              onChange={(e) => setExcludedFilePatterns(e.target.value)}
              placeholder=".git&#10;node_modules&#10;target"
              style={{ fontFamily: 'var(--font-family-mono)', fontSize: 12 }}
            />
            <p className={styles.helpText}>
              File tree hides entries whose path contains any of these substrings.
            </p>
          </div>
        </section>

        <section className={styles.section} aria-labelledby="settings-deps-title">
          <h3 id="settings-deps-title" className={styles.sectionTitle}>
            External tools — availability
          </h3>
          <p className={styles.helpText}>
            Quickly check if the optional tools Cremniy can shell out to are reachable.
          </p>
          <div className={styles.row} style={{ flexWrap: 'wrap', gap: '0.35rem' }}>
            <button
              type="button"
              className={styles.btn}
              disabled={busy}
              onClick={() => void runToolTest('file')}
            >
              Test file(1)
            </button>
            <button
              type="button"
              className={styles.btn}
              disabled={busy}
              onClick={() => void runToolTest('nasm')}
            >
              Test nasm
            </button>
            <button
              type="button"
              className={styles.btn}
              disabled={busy}
              onClick={() => void runToolTest('objdump', objdumpPath)}
            >
              Test objdump
            </button>
            <button
              type="button"
              className={styles.btn}
              disabled={busy}
              onClick={() => void runToolTest('r2', radare2Path)}
            >
              Test radare2
            </button>
          </div>
        </section>

        {error != null ? <p className={styles.error}>{error}</p> : null}

        <div className={styles.actions}>
          <button type="button" className={styles.btn} onClick={() => void handleImport()} disabled={busy}>
            Import…
          </button>
          <button type="button" className={styles.btn} onClick={() => void handleExport()} disabled={busy}>
            Export…
          </button>
          <span className={styles.actionsSpacer} />
          <button type="button" className={styles.btn} onClick={onClose} disabled={busy}>
            Cancel
          </button>
          <button type="button" className={styles.btnPrimary} onClick={() => void handleSave()} disabled={busy}>
            Save
          </button>
        </div>
      </div>
    </div>
  );
}
