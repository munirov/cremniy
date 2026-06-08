import { createContext, useContext, type ReactNode } from 'react';

import type { AppPreferences } from '@domain/preferences/appPreferences';
import { settingsService } from '@infrastructure/settings/settingsService';
import { SettingsDialog } from '@boundary/settings/SettingsDialog';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { pluginCenterPanels } from '@shared/plugins/registry';

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

type CenterPanelDef = { label: string; render: () => ReactNode };

/** Core (first-party) non-file center panels. Plugins add more via contributions
 *  (e.g. the Git plugin's Advanced Git tab) — resolve any panel (core or plugin)
 *  through {@link resolveCenterPanel}. */
export const CENTER_PANELS: Record<string, CenterPanelDef> = {
  settings: { label: 'Settings', render: () => <SettingsTab /> },
};

/** Look up a center panel by id: core panels first, then plugin contributions.
 *  Use this instead of indexing CENTER_PANELS so plugin panels resolve too. */
export function resolveCenterPanel(id: string | null | undefined): CenterPanelDef | undefined {
  if (id == null) return undefined;
  const core = CENTER_PANELS[id];
  if (core != null) return core;
  const contributed = pluginCenterPanels().find((p) => p.id === id);
  return contributed != null ? { label: contributed.label, render: contributed.render } : undefined;
}
