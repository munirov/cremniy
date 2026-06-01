import type { editor } from 'monaco-editor';

// Autocomplete (quickSuggestions/suggestOnTriggerCharacters) replaces the Qt
// QCodeEditor keyword completer; it activates for the language set per file
// (see domain/editor/editorLanguage.ts).
export const IDE_MONACO_BASE_OPTIONS: editor.IStandaloneEditorConstructionOptions = {
  wordWrap: 'on',
  lineNumbers: 'on',
  minimap: { enabled: false },
  scrollBeyondLastLine: false,
  automaticLayout: true,
  quickSuggestions: true,
  suggestOnTriggerCharacters: true,
  wordBasedSuggestions: 'currentDocument',
};
