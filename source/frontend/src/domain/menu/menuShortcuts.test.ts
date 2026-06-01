import { describe, expect, it } from 'vitest';

import { matchGlobalShortcut } from './menuShortcuts';

function keyEv(init: Partial<KeyboardEvent> & Pick<KeyboardEvent, 'key'>): KeyboardEvent {
  return new KeyboardEvent('keydown', { bubbles: true, cancelable: true, ...init });
}

function keyEvWithTarget(init: Partial<KeyboardEvent> & Pick<KeyboardEvent, 'key'>, target: HTMLElement): KeyboardEvent {
  const ev = keyEv(init);
  Object.defineProperty(ev, 'target', { value: target, enumerable: true });
  return ev;
}

describe('matchGlobalShortcut', () => {
  it('matches Ctrl+O to open file', () => {
    const ev = keyEv({ key: 'o', ctrlKey: true });
    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'file', id: 'openFile' });
  });

  it('matches Ctrl+Shift+O to open folder', () => {
    const ev = keyEv({ key: 'O', ctrlKey: true, shiftKey: true });
    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'file', id: 'openFolder' });
  });

  it('matches Ctrl+S to save', () => {
    const ev = keyEv({ key: 's', ctrlKey: true });
    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'file', id: 'save' });
  });

  it('matches Ctrl+W to close editor tab', () => {
    const ev = keyEv({ key: 'w', ctrlKey: true });
    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'file', id: 'closeEditorTab' });
  });

  it('matches Ctrl+Shift+W to close workspace', () => {
    const ev = keyEv({ key: 'w', ctrlKey: true, shiftKey: true });
    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'file', id: 'closeWorkspace' });
  });

  it('matches Ctrl+F to find in editor', () => {
    const ev = keyEv({ key: 'f', ctrlKey: true });
    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'edit', id: 'findInEditor' });
  });

  it('matches Ctrl+` to toggle terminal', () => {
    const ev = keyEv({ key: '`', ctrlKey: true, code: 'Backquote' });
    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'view', id: 'toggleTerminal' });
  });

  it('matches Ctrl+S when focus is in a Monaco textbox target', () => {
    const editorTextInput = document.createElement('textarea');
    const ev = keyEvWithTarget({ key: 's', ctrlKey: true }, editorTextInput);

    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'file', id: 'save' });
  });

  it('matches Cmd+F when focus is in a textbox target', () => {
    const input = document.createElement('input');
    const ev = keyEvWithTarget({ key: 'f', metaKey: true }, input);

    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'edit', id: 'findInEditor' });
  });

  it('allows Ctrl+W on a typing surface (close editor tab)', () => {
    const textarea = document.createElement('textarea');
    const ev = keyEvWithTarget({ key: 'w', ctrlKey: true }, textarea);

    expect(matchGlobalShortcut(ev)).toEqual({ kind: 'file', id: 'closeEditorTab' });
  });

  it('blocks Ctrl+Shift+W on a typing surface', () => {
    const input = document.createElement('input');
    const ev = keyEvWithTarget({ key: 'w', ctrlKey: true, shiftKey: true }, input);

    expect(matchGlobalShortcut(ev)).toBeNull();
  });

  it('blocks non-editor-safe shortcuts when typing in an input', () => {
    const input = document.createElement('input');
    const ev = keyEvWithTarget({ key: '`', ctrlKey: true, code: 'Backquote' }, input);

    expect(matchGlobalShortcut(ev)).toBeNull();
  });
});
