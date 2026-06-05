import type { FileMenuActionId } from './fileMenu';
import type { EditMenuActionId } from './editMenu';
import type { ViewMenuActionId } from './viewMenu';

export type GlobalShortcutAction =
  | { kind: 'file'; id: FileMenuActionId }
  | { kind: 'edit'; id: EditMenuActionId }
  | { kind: 'view'; id: ViewMenuActionId };

function isTypingSurface(target: EventTarget | null): boolean {
  if (target == null || !(target instanceof HTMLElement)) {
    return false;
  }
  const tag = target.tagName;
  if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') {
    return true;
  }
  return target.isContentEditable;
}

function mod(ev: KeyboardEvent): boolean {
  return ev.ctrlKey || ev.metaKey;
}

// Cyrillic → Latin for keys we hot-bind. Lets Ctrl+ё toggle terminal, Ctrl+б
// open preferences, Ctrl+щ open file, etc. — Qt parity with Russian keyboard
// layouts. ev.code carries the physical key when we need it.
const CYRILLIC_TO_LATIN: Record<string, string> = {
  ё: '`',
  й: 'q',
  ц: 'w',
  у: 'e',
  к: 'r',
  е: 't',
  н: 'y',
  г: 'u',
  ш: 'i',
  щ: 'o',
  з: 'p',
  ф: 'a',
  ы: 's',
  в: 'd',
  а: 'f',
  п: 'g',
  р: 'h',
  о: 'j',
  л: 'k',
  д: 'l',
  я: 'z',
  ч: 'x',
  с: 'c',
  м: 'v',
  и: 'b',
  т: 'n',
  ь: 'm',
  б: ',',
  ю: '.',
};

function normalizeKey(ev: KeyboardEvent): string {
  const raw = ev.key.toLowerCase();
  return CYRILLIC_TO_LATIN[raw] ?? raw;
}

function matchShortcutAction(ev: KeyboardEvent): GlobalShortcutAction | null {
  const k = normalizeKey(ev);
  if (k === 'o' && !ev.shiftKey) {
    return { kind: 'file', id: 'openFile' };
  }
  if (k === 'o' && ev.shiftKey) {
    return { kind: 'file', id: 'openFolder' };
  }
  if (k === 's' && !ev.shiftKey) {
    return { kind: 'file', id: 'save' };
  }
  if (k === 's' && ev.shiftKey) {
    return { kind: 'file', id: 'saveAs' };
  }
  if (k === 'f' && !ev.shiftKey) {
    return { kind: 'edit', id: 'findInEditor' };
  }
  if (k === ',') {
    return { kind: 'file', id: 'preferences' };
  }
  if (k === 'w' && ev.shiftKey) {
    return { kind: 'file', id: 'closeWorkspace' };
  }
  if (k === 'w' && !ev.shiftKey) {
    return { kind: 'file', id: 'closeEditorTab' };
  }
  if (k === '`' || ev.code === 'Backquote') {
    return { kind: 'view', id: 'toggleTerminal' };
  }
  if (k === 'b' && !ev.shiftKey) {
    return { kind: 'view', id: 'toggleFileTree' };
  }
  return null;
}

function isAllowedOnTypingSurface(action: GlobalShortcutAction): boolean {
  return (
    (action.kind === 'file' &&
      (action.id === 'openFile' || action.id === 'save' || action.id === 'closeEditorTab')) ||
    (action.kind === 'edit' && action.id === 'findInEditor')
  );
}

export function matchGlobalShortcut(ev: KeyboardEvent): GlobalShortcutAction | null {
  if (!mod(ev) || ev.altKey) {
    return null;
  }
  const action = matchShortcutAction(ev);
  if (action == null) {
    return null;
  }
  if (isTypingSurface(ev.target) && !isAllowedOnTypingSurface(action)) {
    return null;
  }
  return action;
}
