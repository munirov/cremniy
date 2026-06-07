import { createContext, useContext, type ReactNode } from 'react';

import type { AppPreferences } from '@domain/preferences/appPreferences';
import { settingsService } from '@infrastructure/settings/settingsService';
import { SettingsDialog } from '@boundary/settings/SettingsDialog';
import { AdvancedGitDialog } from '@boundary/workspace/AdvancedGitDialog';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

/**
 * The center "tab space" is a generic host: a tab can be a file editor OR one
 * of these registered panels (settings today; git / tools later). RootApp owns
 * the live prefs, so it publishes how to apply a saved settings change here.
 */
const SettingsApplyContext = createContext<(prefs: AppPreferences) => void>(() => undefined);

export function SettingsApplyProvider({
  apply,
  children,
}: {
  apply: (prefs: AppPreferences) => void;
  children: ReactNode;
}) {
  return <SettingsApplyContext.Provider value={apply}>{children}</SettingsApplyContext.Provider>;
}

function SettingsTab() {
  const workspaceRoot = useWorkspaceRoot();
  const { closePanel } = useIdeSession();
  const applySaved = useContext(SettingsApplyContext);
  return (
    <SettingsDialog
      embedded
      service={settingsService}
      workspaceRoot={workspaceRoot?.path ?? null}
      onSaved={applySaved}
      onClose={() => closePanel('settings')}
    />
  );
}

function AdvancedGitTab() {
  const workspaceRoot = useWorkspaceRoot();
  const { closePanel } = useIdeSession();
  return (
    <AdvancedGitDialog
      embedded
      workspaceRoot={workspaceRoot?.path ?? null}
      onClose={() => closePanel('advancedGit')}
    />
  );
}

/** Registry of non-file center panels. Add an entry to host a new view as a
 *  center tab — the tab strip (label) and the editor body (render) pick it up. */
export const CENTER_PANELS: Record<string, { label: string; render: () => ReactNode }> = {
  settings: { label: 'Settings', render: () => <SettingsTab /> },
  advancedGit: { label: 'Git', render: () => <AdvancedGitTab /> },
};
