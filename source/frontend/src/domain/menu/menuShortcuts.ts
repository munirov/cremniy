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

function matchShortcutAction(ev: KeyboardEvent): GlobalShortcutAction | null {
  const k = ev.key.length === 1 ? ev.key.toLowerCase() : ev.key.toLowerCase();
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
  if (ev.key === ',' || k === ',') {
    return { kind: 'file', id: 'preferences' };
  }
  if (k === 'w' && ev.shiftKey) {
    return { kind: 'file', id: 'closeWorkspace' };
  }
  if (k === 'w' && !ev.shiftKey) {
    return { kind: 'file', id: 'closeEditorTab' };
  }
  if (ev.key === '`' || ev.code === 'Backquote') {
    return { kind: 'view', id: 'toggleTerminal' };
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
