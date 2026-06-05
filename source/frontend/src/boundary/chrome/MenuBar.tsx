import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { EDIT_MENU_ENTRIES, type EditMenuActionId } from '@domain/menu/editMenu';
import { FILE_MENU_ENTRIES, type FileMenuActionId } from '@domain/menu/fileMenu';
import { matchGlobalShortcut, type GlobalShortcutAction } from '@domain/menu/menuShortcuts';
import { mainMenuEntries, type MainMenuId } from '@domain/menu/mainMenu';
import { REFERENCES_MENU_ENTRIES, type ReferencesMenuActionId } from '@domain/menu/referencesMenu';
import { TOOLS_MENU_ENTRIES, type ToolsMenuActionId } from '@domain/menu/toolsMenu';
import { VIEW_MENU_ENTRIES, type ViewMenuActionId } from '@domain/menu/viewMenu';
import { useT } from '@boundary/i18n/LocaleContext';

import styles from './MenuBar.module.css';

const DROPDOWN_MENUS = new Set<MainMenuId>(['1', '2', '3', '5', '6']);

export type MenuBarProps = {
  onFileMenuAction?: (id: FileMenuActionId) => void;
  onEditMenuAction?: (id: EditMenuActionId) => void;
  onViewMenuAction?: (id: ViewMenuActionId) => void;
  onToolsMenuAction?: (id: ToolsMenuActionId) => void;
  onReferencesMenuAction?: (id: ReferencesMenuActionId) => void;
  onShortcut?: (action: GlobalShortcutAction) => void;
  terminalPanelVisible?: boolean;
  wordWrapEnabled?: boolean;
  hasActiveDocument?: boolean;
  activeDocumentDirty?: boolean;
  /** When false, disables File → Close editor (no tabs and empty scratch buffer). */
  closeEditorAvailable?: boolean;
};

export function MenuBar({
  onFileMenuAction,
  onEditMenuAction,
  onViewMenuAction,
  onToolsMenuAction,
  onReferencesMenuAction,
  onShortcut,
  terminalPanelVisible = true,
  wordWrapEnabled = true,
  hasActiveDocument = false,
  activeDocumentDirty = false,
  closeEditorAvailable = false,
}: MenuBarProps) {
  const t = useT();
  const entries = useMemo(() => mainMenuEntries(), []);
  const [openMenuId, setOpenMenuId] = useState<MainMenuId | null>(null);
  const wrapRefs = useRef<Partial<Record<MainMenuId, HTMLDivElement | null>>>({});

  const closeDropdown = useCallback(() => {
    setOpenMenuId(null);
  }, []);

  useEffect(() => {
    if (openMenuId == null || !DROPDOWN_MENUS.has(openMenuId)) {
      return;
    }
    const onPointerDown = (ev: PointerEvent) => {
      const el = openMenuId != null ? wrapRefs.current[openMenuId] : null;
      if (el != null && !el.contains(ev.target as Node)) {
        closeDropdown();
      }
    };
    document.addEventListener('pointerdown', onPointerDown, true);
    return () => {
      document.removeEventListener('pointerdown', onPointerDown, true);
    };
  }, [closeDropdown, openMenuId]);

  useEffect(() => {
    if (onShortcut == null) {
      return;
    }
    const onKey = (ev: KeyboardEvent) => {
      const m = matchGlobalShortcut(ev);
      if (m == null) {
        return;
      }
      ev.preventDefault();
      onShortcut(m);
    };
    // Capture phase — otherwise Monaco grabs Ctrl+S/F/W/O first and our global
    // bindings never fire while the editor has focus.
    window.addEventListener('keydown', onKey, true);
    return () => {
      window.removeEventListener('keydown', onKey, true);
    };
  }, [onShortcut]);

  const runFile = useCallback(
    (id: FileMenuActionId) => {
      onFileMenuAction?.(id);
      closeDropdown();
    },
    [closeDropdown, onFileMenuAction],
  );

  const runEdit = useCallback(
    (id: EditMenuActionId) => {
      onEditMenuAction?.(id);
      closeDropdown();
    },
    [closeDropdown, onEditMenuAction],
  );

  const runView = useCallback(
    (id: ViewMenuActionId) => {
      onViewMenuAction?.(id);
      closeDropdown();
    },
    [closeDropdown, onViewMenuAction],
  );

  const runTools = useCallback(
    (id: ToolsMenuActionId) => {
      onToolsMenuAction?.(id);
      closeDropdown();
    },
    [closeDropdown, onToolsMenuAction],
  );

  const runRefs = useCallback(
    (id: ReferencesMenuActionId) => {
      onReferencesMenuAction?.(id);
      closeDropdown();
    },
    [closeDropdown, onReferencesMenuAction],
  );

  const toggleMenu = useCallback((id: MainMenuId) => {
    setOpenMenuId((cur) => (cur === id ? null : id));
  }, []);

  const setWrapRef = useCallback((id: MainMenuId) => {
    return (el: HTMLDivElement | null) => {
      wrapRefs.current[id] = el;
    };
  }, []);

  return (
    <nav className={styles.menuBar} aria-label="Main menu">
      <ul className={styles.menuList}>
        {entries.map(({ id, label, i18nKey }) => {
          const displayLabel = t(i18nKey) || label;
          if (id === '1') {
            return (
              <li key={id} className={styles.menuItemWrap}>
                <div ref={setWrapRef('1')} className={styles.menuWithDropdown}>
                  <button
                    type="button"
                    className={styles.menuItem}
                    aria-haspopup="menu"
                    aria-expanded={openMenuId === '1'}
                    onClick={() => toggleMenu('1')}
                  >
                    {displayLabel}
                  </button>
                  {openMenuId === '1' && onFileMenuAction != null ? (
                    <ul className={styles.dropdown} role="menu">
                      {FILE_MENU_ENTRIES.map((item) => (
                        <li key={item.id} role="none">
                          <button
                            type="button"
                            role="menuitem"
                            className={styles.dropdownItem}
                            disabled={
                              item.id === 'save'
                                ? !hasActiveDocument || !activeDocumentDirty
                                : item.id === 'closeEditorTab'
                                  ? !closeEditorAvailable
                                  : undefined
                            }
                            onClick={() => runFile(item.id)}
                          >
                            {item.label}
                            {item.id === 'save' && activeDocumentDirty ? ' *' : ''}
                          </button>
                        </li>
                      ))}
                    </ul>
                  ) : null}
                </div>
              </li>
            );
          }
          if (id === '2') {
            return (
              <li key={id} className={styles.menuItemWrap}>
                <div ref={setWrapRef('2')} className={styles.menuWithDropdown}>
                  <button
                    type="button"
                    className={styles.menuItem}
                    aria-haspopup="menu"
                    aria-expanded={openMenuId === '2'}
                    onClick={() => toggleMenu('2')}
                  >
                    {displayLabel}
                  </button>
                  {openMenuId === '2' ? (
                    <ul className={styles.dropdown} role="menu">
                      {EDIT_MENU_ENTRIES.map((item) => (
                        <li key={item.id} role="none">
                          <button
                            type="button"
                            role="menuitem"
                            className={styles.dropdownItem}
                            disabled={!hasActiveDocument}
                            onClick={() => runEdit(item.id)}
                          >
                            {item.label}
                          </button>
                        </li>
                      ))}
                    </ul>
                  ) : null}
                </div>
              </li>
            );
          }
          if (id === '3') {
            return (
              <li key={id} className={styles.menuItemWrap}>
                <div ref={setWrapRef('3')} className={styles.menuWithDropdown}>
                  <button
                    type="button"
                    className={styles.menuItem}
                    aria-haspopup="menu"
                    aria-expanded={openMenuId === '3'}
                    onClick={() => toggleMenu('3')}
                  >
                    {displayLabel}
                  </button>
                  {openMenuId === '3' ? (
                    <ul className={styles.dropdown} role="menu">
                      {VIEW_MENU_ENTRIES.map((item) => {
                        const isTerminalToggle = item.id === 'toggleTerminal';
                        const isWordWrapToggle = item.id === 'toggleWordWrap';
                        const checked =
                          isTerminalToggle ? terminalPanelVisible : wordWrapEnabled;
                        return (
                          <li key={item.id} role="none">
                            <button
                              type="button"
                              role={isTerminalToggle || isWordWrapToggle ? 'menuitemcheckbox' : 'menuitem'}
                              aria-checked={isTerminalToggle || isWordWrapToggle ? checked : undefined}
                              className={styles.dropdownItem}
                              onClick={() => runView(item.id)}
                            >
                              {item.label}
                              {isTerminalToggle || isWordWrapToggle ? (checked ? ' ✓' : '') : ''}
                            </button>
                          </li>
                        );
                      })}
                    </ul>
                  ) : null}
                </div>
              </li>
            );
          }
          if (id === '4') {
            return (
              <li key={id} className={styles.menuItemWrap}>
                <div className={styles.menuWithDropdown}>
                  <button type="button" className={styles.menuItem} disabled title="Build actions are not available yet">
                    {displayLabel}
                  </button>
                </div>
              </li>
            );
          }
          if (id === '5') {
            return (
              <li key={id} className={styles.menuItemWrap}>
                <div ref={setWrapRef('5')} className={styles.menuWithDropdown}>
                  <button
                    type="button"
                    className={styles.menuItem}
                    aria-haspopup="menu"
                    aria-expanded={openMenuId === '5'}
                    onClick={() => toggleMenu('5')}
                  >
                    {displayLabel}
                  </button>
                  {openMenuId === '5' ? (
                    <ul className={styles.dropdown} role="menu">
                      {TOOLS_MENU_ENTRIES.map((item) => (
                        <li key={item.id} role="none">
                          <button type="button" role="menuitem" className={styles.dropdownItem} onClick={() => runTools(item.id)}>
                            {item.label}
                          </button>
                        </li>
                      ))}
                    </ul>
                  ) : null}
                </div>
              </li>
            );
          }
          if (id === '6') {
            return (
              <li key={id} className={styles.menuItemWrap}>
                <div ref={setWrapRef('6')} className={styles.menuWithDropdown}>
                  <button
                    type="button"
                    className={styles.menuItem}
                    aria-haspopup="menu"
                    aria-expanded={openMenuId === '6'}
                    onClick={() => toggleMenu('6')}
                  >
                    {displayLabel}
                  </button>
                  {openMenuId === '6' ? (
                    <ul className={styles.dropdown} role="menu">
                      {REFERENCES_MENU_ENTRIES.map((item) => (
                        <li key={item.id} role="none">
                          <button type="button" role="menuitem" className={styles.dropdownItem} onClick={() => runRefs(item.id)}>
                            {item.label}
                          </button>
                        </li>
                      ))}
                    </ul>
                  ) : null}
                </div>
              </li>
            );
          }
          return (
            <li key={id} className={styles.menuItemWrap}>
              <button type="button" className={styles.menuItem} disabled>
                {label}
              </button>
            </li>
          );
        })}
      </ul>
    </nav>
  );
}
