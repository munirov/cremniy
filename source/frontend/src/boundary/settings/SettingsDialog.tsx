import { useCallback, useEffect, useState } from 'react';

import type {
  AppPreferences,
  DisassemblySyntaxPreference,
  ThemePreference,
} from '@domain/preferences/appPreferences';
import {
  DEFAULT_DISASSEMBLY_PREFERENCES,
  MAX_DISASSEMBLY_INSTRUCTION_LIMIT,
  MIN_DISASSEMBLY_INSTRUCTION_LIMIT,
  normalizeDisassemblyPreferences,
} from '@domain/preferences/appPreferences';
import type { SettingsService } from '@domain/preferences/settingsService';

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
  const [terminalPanelVisible, setTerminalPanelVisible] = useState(true);
  const [editorWordWrap, setEditorWordWrap] = useState(true);
  const [objdumpPath, setObjdumpPath] = useState('');
  const [archHint, setArchHint] = useState('');
  const [instructionLimit, setInstructionLimit] = useState(
    String(DEFAULT_DISASSEMBLY_PREFERENCES.instructionLimit),
  );
  const [syntax, setSyntax] = useState<DisassemblySyntaxPreference>(
    DEFAULT_DISASSEMBLY_PREFERENCES.syntax,
  );
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
          setTerminalPanelVisible(p.terminalPanelVisible);
          setEditorWordWrap(p.editorWordWrap);
          setObjdumpPath(p.disassembly.objdumpPath);
          setArchHint(p.disassembly.archHint);
          setInstructionLimit(String(p.disassembly.instructionLimit));
          setSyntax(p.disassembly.syntax);
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
        terminalPanelVisible,
        editorWordWrap,
        disassembly: normalizeDisassemblyPreferences({
          ...current.disassembly,
          backend: 'objdump',
          objdumpPath: objdumpPath.trim(),
          archHint: archHint.trim(),
          instructionLimit: Number.isFinite(parsedInstructionLimit)
            ? parsedInstructionLimit
            : DEFAULT_DISASSEMBLY_PREFERENCES.instructionLimit,
          syntax,
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
  }, [archHint, editorWordWrap, instructionLimit, objdumpPath, onClose, onSaved, service, syntax, theme, terminalPanelVisible]);

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

        <section className={styles.section} aria-labelledby="settings-disassembly-title">
          <h3 id="settings-disassembly-title" className={styles.sectionTitle}>
            Disassembly tooling
          </h3>

          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-disassembly-backend">
              Disassembler backend
            </label>
            <select id="settings-disassembly-backend" className={styles.input} value="objdump" disabled>
              <option value="objdump">objdump</option>
              <option value="radare2" disabled>
                radare2 (upcoming)
              </option>
            </select>
            <p className={styles.helpText}>radare2 settings will be enabled when the backend is available.</p>
          </div>

          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-objdump-path">
              objdump path
            </label>
            <div className={styles.inlineControl}>
              <input
                id="settings-objdump-path"
                className={styles.input}
                type="text"
                value={objdumpPath}
                onChange={(e) => {
                  setObjdumpPath(e.target.value);
                  setObjdumpStatus(null);
                }}
                placeholder="Leave blank to search PATH"
              />
              <button
                type="button"
                className={styles.btn}
                onClick={() => void handleTestObjdump()}
                disabled={busy}
              >
                Test objdump
              </button>
            </div>
            <p className={styles.helpText}>
              Custom paths must be absolute executable files outside the active workspace.
            </p>
            {objdumpStatus != null ? <p className={styles.success}>{objdumpStatus}</p> : null}
          </div>

          <div className={styles.field}>
            <label className={styles.label} htmlFor="settings-arch-hint">
              Architecture hint
            </label>
            <input
              id="settings-arch-hint"
              className={styles.input}
              type="text"
              value={archHint}
              onChange={(e) => setArchHint(e.target.value)}
              placeholder="Blank uses objdump auto/default"
            />
            <p className={styles.helpText}>Example: i386:x86-64 for raw x86-64 binaries.</p>
          </div>

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
