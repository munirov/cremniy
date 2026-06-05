export type FileMenuActionId =
  | 'newProject'
  | 'openFolder'
  | 'openFile'
  | 'save'
  | 'saveAs'
  | 'preferences'
  | 'closeEditorTab'
  | 'closeWorkspace';

export type FileMenuEntry = {
  id: FileMenuActionId;
  label: string;
};

export const FILE_MENU_ENTRIES: readonly FileMenuEntry[] = [
  { id: 'newProject', label: 'New project…' },
  { id: 'openFolder', label: 'Open folder…' },
  { id: 'openFile', label: 'Open file…' },
  { id: 'save', label: 'Save' },
  { id: 'saveAs', label: 'Save as…' },
  { id: 'preferences', label: 'Preferences…' },
  { id: 'closeEditorTab', label: 'Close editor' },
  { id: 'closeWorkspace', label: 'Close workspace' },
] as const;
