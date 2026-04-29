import type { NavigateFunction } from 'react-router-dom';

import type { MainMenuId } from '@domain/menu/mainMenu';
import { MAIN_MENU_IDS } from '@domain/menu/mainMenu';

export type TopLevelMenuHandlers = Record<MainMenuId, () => void>;

/** Qt menu bar stub: top-level menus are no-ops until real menus exist. */
export function buildStubTopMenuHandlers(): TopLevelMenuHandlers {
  const out = {} as TopLevelMenuHandlers;
  for (const id of MAIN_MENU_IDS) {
    out[id] = () => {};
  }
  return out;
}

/** Returns to Welcome and drops workspace query from history (single stack entry). */
export function closeWorkspaceHandler(navigate: NavigateFunction): () => void {
  return () => navigate('/', { replace: true });
}
