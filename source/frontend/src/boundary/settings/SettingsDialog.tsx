import { useCallback, useEffect, useState } from 'react';
import type { ReactNode } from 'react';

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

type Category = 'general' | 'editor' | 'files' | 'hex' | 'disassembly' | 'tools';

const CATEGORIES: Array<{ id: Category; label: string }> = [
  { id: 'general', label: 'General' },
  { id: 'editor', label: 'Editor' },
  { id: 'files', label: 'Files' },
  { id: 'hex', label: 'Hex viewer' },
  { id: 'disassembly', label: 'Disassembly' },
  { id: 'tools', label: 'External tools' },
];

/** A styled checkbox rendered as a switch — keeps role=checkbox + the label. */
function Toggle({ checked, onChange, label }: { checked: boolean; onChange: (v: boolean) => void; label: string }) {
  return (
    <span className={`${styles.toggle} ${checked ? styles.toggleOn : ''}`}>
      <input
        type="checkbox"
        className={styles.toggleInput}
        checked={checked}
        aria-label={label}
        onChange={(e) => onChange(e.target.checked)}
      />
      <span className={styles.toggleKnob} aria-hidden />
    </span>
  );
}

function SettingRow({
  title,
  description,
  control,
  stacked,
}: {
  title: string;
  description?: string;
  control: ReactNode;
  stacked?: boolean;
}) {
  return (
    <div className={`${styles.settingRow} ${stacked ? styles.settingRowStacked : ''}`}>
      <div className={styles.info}>
        <div className={styles.rowTitle}>{title}</div>
        {description ? <div className={styles.rowDesc}>{description}</div> : null}
      </div>
      <div className={styles.control}>{control}</div>
    </div>
  );
}

export function SettingsDialog({ open, onClose, onSaved, workspaceRoot, service }: SettingsDialogProps) {
  const [category, setCategory] = useState<Category>('general');
  const [theme, setTheme] = useState<ThemePreference>('dark');
  const [locale, setLocale] = useState<LocalePreference>('en');
  const [terminalPanelVisible, setTerminalPanelVisible] = useState(true);
  const [editorWordWrap, setEditorWordWrap] = useState(true);
  const [editorFontSize, setEditorFontSize] = useState('14');
  const [editorInsertSpaces, setEditorInsertSpaces] = useState(false);
  const [editorTabWidth, setEditorTabWidth] = useState(4);
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
          setEditorFontSize(String(p.editorFontSize));
          setEditorInsertSpaces(p.editorInsertSpaces);
          setEditorTabWidth(p.editorTabWidth);
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
      const parsedFontSize = Number.parseInt(editorFontSize, 10);
      const next: AppPreferences = {
        ...current,
        theme,
        locale,
        terminalPanelVisible,
        editorWordWrap,
        editorFontSize: Number.isFinite(parsedFontSize)
          ? Math.max(8, Math.min(48, parsedFontSize))
          : current.editorFontSize,
        editorInsertSpaces,
        editorTabWidth,
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
  }, [archHint, backend, editorFontSize, editorInsertSpaces, editorTabWidth, editorWordWrap, excludedFilePatterns, hexOptions, instructionLimit, locale, objdumpPath, onClose, onSaved, radare2AnalysisLevel, radare2Path, radare2PreCommands, service, syntax, terminalPanelVisible, theme]);

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
        setLocale(imported.locale);
        setTerminalPanelVisible(imported.terminalPanelVisible);
        setEditorWordWrap(imported.editorWordWrap);
        setEditorFontSize(String(imported.editorFontSize));
        setEditorInsertSpaces(imported.editorInsertSpaces);
        setEditorTabWidth(imported.editorTabWidth);
        setExcludedFilePatterns(imported.excludedFilePatterns);
        setHexOptions(imported.hexOptions);
        setObjdumpPath(imported.disassembly.objdumpPath);
        setArchHint(imported.disassembly.archHint);
        setInstructionLimit(String(imported.disassembly.instructionLimit));
        setSyntax(imported.disassembly.syntax);
        setBackend(imported.disassembly.backend);
        setRadare2Path(imported.disassembly.radare2Path);
        setRadare2AnalysisLevel(imported.disassembly.radare2AnalysisLevel);
        setRadare2PreCommands(imported.disassembly.radare2PreCommands);
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

  const activeCategory = CATEGORIES.find((c) => c.id === category)?.label ?? '';

  return (
    <div className={styles.backdrop} role="presentation" onMouseDown={(e) => e.target === e.currentTarget && onClose()}>
      <div
        className={styles.dialog}
        role="dialog"
        aria-modal="true"
        aria-labelledby="settings-dialog-title"
        onMouseDown={(e) => e.stopPropagation()}
        onKeyDown={(e) => {
          if (e.key === 'Escape') {
            onClose();
          }
        }}
      >
        <nav className={styles.sidebar} aria-label="Settings categories">
          <span className={styles.sidebarTitle} id="settings-dialog-title">
            Settings
          </span>
          {CATEGORIES.map((c) => (
            <button
              key={c.id}
              type="button"
              className={`${styles.navItem} ${category === c.id ? styles.navItemActive : ''}`}
              aria-current={category === c.id}
              onClick={() => setCategory(c.id)}
            >
              {c.label}
            </button>
          ))}
        </nav>

        <div className={styles.main}>
          <div className={styles.mainScroll}>
            <h2 className={styles.catTitle}>{activeCategory}</h2>

            {category === 'general' ? (
              <>
                <SettingRow
                  title="Appearance"
                  description="Color theme for the whole interface."
                  control={
                    <select
                      className={styles.select}
                      aria-label="Appearance"
                      value={theme}
                      onChange={(e) => setTheme(e.target.value as ThemePreference)}
                    >
                      <option value="dark">Dark</option>
                      <option value="light">Light</option>
                    </select>
                  }
                />
                <SettingRow
                  title="Language"
                  description="Interface language."
                  control={
                    <select
                      className={styles.select}
                      aria-label="Language"
                      value={locale}
                      onChange={(e) => setLocale(e.target.value as LocalePreference)}
                    >
                      {LOCALES.map((l) => (
                        <option key={l.id} value={l.id}>
                          {l.label}
                        </option>
                      ))}
                    </select>
                  }
                />
                <SettingRow
                  title="Show terminal panel"
                  description="Show the terminal region at the bottom of the window."
                  control={<Toggle checked={terminalPanelVisible} onChange={setTerminalPanelVisible} label="Show terminal panel" />}
                />
              </>
            ) : null}

            {category === 'editor' ? (
              <>
                <SettingRow
                  title="Word wrap"
                  description="Wrap long lines in the editor."
                  control={<Toggle checked={editorWordWrap} onChange={setEditorWordWrap} label="Word wrap in editor" />}
                />
                <SettingRow
                  title="Font size"
                  description="Editor font size in pixels (also adjustable with Ctrl + mouse wheel)."
                  control={
                    <input
                      className={styles.num}
                      type="number"
                      min={8}
                      max={48}
                      aria-label="Font size"
                      value={editorFontSize}
                      onChange={(e) => setEditorFontSize(e.target.value)}
                    />
                  }
                />
                <SettingRow
                  title="Indent with spaces"
                  description="Tab key inserts spaces instead of a tab character."
                  control={<Toggle checked={editorInsertSpaces} onChange={setEditorInsertSpaces} label="Indent with spaces" />}
                />
                <SettingRow
                  title="Tab width"
                  description="Spaces per indent and the display width of a tab."
                  control={
                    <select
                      className={styles.select}
                      aria-label="Tab width"
                      value={editorTabWidth}
                      onChange={(e) => setEditorTabWidth(Number(e.target.value))}
                    >
                      <option value={2}>2</option>
                      <option value={4}>4</option>
                      <option value={8}>8</option>
                    </select>
                  }
                />
              </>
            ) : null}

            {category === 'files' ? (
              <SettingRow
                stacked
                title="Excluded patterns"
                description="The file tree hides entries whose path contains any of these substrings (one per line)."
                control={
                  <textarea
                    className={styles.fullInput}
                    rows={5}
                    aria-label="Excluded patterns"
                    value={excludedFilePatterns}
                    onChange={(e) => setExcludedFilePatterns(e.target.value)}
                    placeholder=".git&#10;node_modules&#10;target"
                  />
                }
              />
            ) : null}

            {category === 'hex' ? (
              <>
                <SettingRow
                  title="Bytes per line"
                  description="How many bytes each row shows."
                  control={
                    <select
                      className={styles.select}
                      aria-label="Bytes per line"
                      value={hexOptions.bytesPerLine}
                      onChange={(e) => setHexOptions((o) => ({ ...o, bytesPerLine: Number(e.target.value) }))}
                    >
                      <option value={8}>8</option>
                      <option value={16}>16</option>
                      <option value={32}>32</option>
                    </select>
                  }
                />
                <SettingRow
                  title="Address width"
                  description="Hex digits in the address column."
                  control={
                    <select
                      className={styles.select}
                      aria-label="Address width"
                      value={hexOptions.addressWidth}
                      onChange={(e) => setHexOptions((o) => ({ ...o, addressWidth: Number(e.target.value) }))}
                    >
                      <option value={8}>8 (32-bit)</option>
                      <option value={16}>16 (64-bit)</option>
                    </select>
                  }
                />
                <SettingRow
                  title="Group length"
                  description="Insert a gap after this many bytes."
                  control={
                    <select
                      className={styles.select}
                      aria-label="Group length"
                      value={hexOptions.groupLength}
                      onChange={(e) => setHexOptions((o) => ({ ...o, groupLength: Number(e.target.value) }))}
                    >
                      <option value={1}>1</option>
                      <option value={2}>2</option>
                      <option value={4}>4</option>
                      <option value={8}>8</option>
                    </select>
                  }
                />
              </>
            ) : null}

            {category === 'disassembly' ? (
              <>
                <SettingRow
                  title="Backend"
                  description={
                    backend === 'objdump'
                      ? 'Built-in x86 / x86-64 decoder — no external tool required.'
                      : 'External radare2 (r2) — ARM / MIPS / RISC-V via the same tool. Backend wiring is pending.'
                  }
                  control={
                    <select
                      className={styles.select}
                      aria-label="Disassembler backend"
                      value={backend}
                      onChange={(e) => setBackend(e.target.value as 'objdump' | 'radare2')}
                    >
                      <option value="objdump">embedded (iced-x86 + goblin)</option>
                      <option value="radare2">radare2 (external)</option>
                    </select>
                  }
                />

                {backend === 'objdump' ? (
                  <>
                    <SettingRow
                      stacked
                      title="objdump path"
                      description="Path to objdump. Empty to search PATH."
                      control={
                        <input
                          className={styles.fullInput}
                          type="text"
                          aria-label="objdump path"
                          value={objdumpPath}
                          onChange={(e) => setObjdumpPath(e.target.value)}
                          placeholder="Empty to search PATH (objdump)"
                        />
                      }
                    />
                    <SettingRow
                      stacked
                      title="Architecture hint"
                      description="Forwarded to objdump as the target architecture (e.g. i386:x86-64, arm)."
                      control={
                        <input
                          className={styles.fullInput}
                          type="text"
                          aria-label="Architecture hint"
                          value={archHint}
                          onChange={(e) => setArchHint(e.target.value)}
                          placeholder="auto-detect"
                        />
                      }
                    />
                  </>
                ) : (
                  <>
                    <SettingRow
                      stacked
                      title="radare2 path"
                      description="Path to the radare2 binary. Empty to search PATH (r2 / radare2)."
                      control={
                        <input
                          className={styles.fullInput}
                          type="text"
                          aria-label="radare2 path"
                          value={radare2Path}
                          onChange={(e) => setRadare2Path(e.target.value)}
                          placeholder="Empty to search PATH (r2 / radare2)"
                        />
                      }
                    />
                    <SettingRow
                      title="Analysis level"
                      description="How much radare2 analyses before queries."
                      control={
                        <select
                          className={styles.select}
                          aria-label="Analysis level"
                          value={radare2AnalysisLevel}
                          onChange={(e) => setRadare2AnalysisLevel(e.target.value as 'none' | 'aa' | 'aaa')}
                        >
                          <option value="none">none</option>
                          <option value="aa">aa — basic</option>
                          <option value="aaa">aaa — full</option>
                        </select>
                      }
                    />
                    <SettingRow
                      stacked
                      title="Pre-commands"
                      description="radare2 commands run before any query (one per line)."
                      control={
                        <textarea
                          className={styles.fullInput}
                          rows={3}
                          aria-label="Pre-commands"
                          value={radare2PreCommands}
                          onChange={(e) => setRadare2PreCommands(e.target.value)}
                          placeholder="e.g. e asm.syntax=intel"
                        />
                      }
                    />
                  </>
                )}

                <SettingRow
                  title="Instruction/render limit"
                  description="Maximum instructions decoded / rendered per view."
                  control={
                    <input
                      className={styles.num}
                      type="number"
                      min={MIN_DISASSEMBLY_INSTRUCTION_LIMIT}
                      max={MAX_DISASSEMBLY_INSTRUCTION_LIMIT}
                      step={50}
                      aria-label="Instruction/render limit"
                      value={instructionLimit}
                      onChange={(e) => setInstructionLimit(e.target.value)}
                    />
                  }
                />
                <SettingRow
                  title="Syntax preference"
                  description="Assembly flavour for x86 output when the backend supports it."
                  control={
                    <select
                      className={styles.select}
                      aria-label="Syntax preference"
                      value={syntax}
                      onChange={(e) => setSyntax(e.target.value as DisassemblySyntaxPreference)}
                    >
                      <option value="intel">Intel</option>
                      <option value="att">AT&amp;T</option>
                    </select>
                  }
                />
                <SettingRow
                  title="Self-check"
                  description="Verify the selected backend can decode a sample."
                  control={
                    <button type="button" className={styles.btn} onClick={() => void handleTestObjdump()} disabled={busy}>
                      Self-check
                    </button>
                  }
                />
              </>
            ) : null}

            {category === 'tools' ? (
              <>
                <p className={styles.helpText} style={{ margin: '0 0 0.5rem' }}>
                  Check whether the optional tools Cremniy can shell out to are reachable.
                </p>
                <SettingRow
                  title="file(1)"
                  description="Identifies file types."
                  control={
                    <button type="button" className={styles.btn} disabled={busy} onClick={() => void runToolTest('file')}>
                      Test file(1)
                    </button>
                  }
                />
                <SettingRow
                  title="nasm"
                  description="Netwide assembler."
                  control={
                    <button type="button" className={styles.btn} disabled={busy} onClick={() => void runToolTest('nasm')}>
                      Test nasm
                    </button>
                  }
                />
                <SettingRow
                  title="objdump"
                  description="GNU object dumper."
                  control={
                    <button type="button" className={styles.btn} disabled={busy} onClick={() => void runToolTest('objdump', objdumpPath)}>
                      Test objdump
                    </button>
                  }
                />
                <SettingRow
                  title="radare2"
                  description="Reverse-engineering framework."
                  control={
                    <button type="button" className={styles.btn} disabled={busy} onClick={() => void runToolTest('r2', radare2Path)}>
                      Test radare2
                    </button>
                  }
                />
              </>
            ) : null}
          </div>

          <div className={styles.footer}>
            <button type="button" className={styles.btn} onClick={() => void handleImport()} disabled={busy}>
              Import…
            </button>
            <button type="button" className={styles.btn} onClick={() => void handleExport()} disabled={busy}>
              Export…
            </button>
            {error != null ? <span className={styles.error}>{error}</span> : objdumpStatus != null ? <span className={styles.statusMsg}>{objdumpStatus}</span> : null}
            <span className={styles.footerSpacer} />
            <button type="button" className={styles.btn} onClick={onClose} disabled={busy}>
              Cancel
            </button>
            <button type="button" className={styles.btnPrimary} onClick={() => void handleSave()} disabled={busy}>
              Save
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
