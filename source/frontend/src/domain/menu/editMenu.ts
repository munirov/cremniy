export type EditMenuActionId = 'findInEditor';

export type EditMenuEntry = {
  id: EditMenuActionId;
  label: string;
};

export const EDIT_MENU_ENTRIES: readonly EditMenuEntry[] = [
  { id: 'findInEditor', label: 'Find in editor' },
] as const;
