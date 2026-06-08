import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { pluginToolTabs } from '@shared/plugins/registry';
import { useRegistryVersion } from '@shared/plugins/useRegistry';

import styles from './ToolRail.module.css';

/**
 * Fixed-width vertical strip pinned to the right edge of the IDE shell — a
 * launcher for the byte tools. Clicking a button opens (or focuses) that tool as
 * an ordinary center tab, the same tab space as files and the Git / Connections
 * panels; close it from its tab like any other. The rail is hidden by the shell
 * when no tool plugin contributes icons (see IdeDockview).
 */
export function ToolRail() {
  const { activePanel, openPanel } = useIdeSession();
  useRegistryVersion(); // re-render when a tool plugin is enabled/disabled
  return (
    <aside className={styles.rail} aria-label="Tool selector" role="tablist" aria-orientation="vertical">
      {pluginToolTabs().map((tab) => {
        const active = activePanel === tab.id;
        return (
          <button
            key={tab.id}
            type="button"
            role="tab"
            aria-selected={active}
            className={`${styles.btn} ${active ? styles.btnActive : ''}`}
            onClick={() => openPanel(tab.id)}
            title={tab.label}
            aria-label={tab.label}
          >
            <svg
              aria-hidden
              className={styles.icon}
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="1.5"
              strokeLinecap="round"
              strokeLinejoin="round"
            >
              <path d={tab.railIconPath} />
            </svg>
          </button>
        );
      })}
    </aside>
  );
}
