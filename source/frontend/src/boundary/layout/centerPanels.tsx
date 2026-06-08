import { createContext, useContext, type ReactNode } from 'react';

import type { AppPreferences } from '@domain/preferences/appPreferences';
import { settingsService } from '@infrastructure/settings/settingsService';
import { SettingsDialog } from '@boundary/settings/SettingsDialog';
import { ExtensionDetailsPanel } from '@boundary/extensions/ExtensionDetailsPanel';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { pluginCenterPanels, pluginToolTabs } from '@shared/plugins/registry';

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
  // The Extensions details tab (VS Code's "Extension: <name>"). The Extensions
  // panel sets the target plugin id in extensionDetailsStore before opening it.
  extensionDetails: { label: 'Extension', render: () => <ExtensionDetailsPanel /> },
};

/** Look up a center panel by id: core panels first, then plugin center-panel
 *  contributions, then plugin tool tabs. The rail tools (hex, disassembler,
 *  strings, …) open as ordinary center tabs — same tab space as files and the
 *  Git / Connections panels — so they resolve here too. */
export function resolveCenterPanel(id: string | null | undefined): CenterPanelDef | undefined {
  if (id == null) return undefined;
  const core = CENTER_PANELS[id];
  if (core != null) return core;
  const contributed = pluginCenterPanels().find((p) => p.id === id);
  if (contributed != null) return { label: contributed.label, render: contributed.render };
  const tool = pluginToolTabs().find((t) => t.id === id);
  if (tool != null) return { label: tool.label, render: tool.render };
  return undefined;
}
