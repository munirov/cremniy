import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { pluginToolTabs } from '@shared/plugins/registry';
import { useRegistryVersion } from '@shared/plugins/useRegistry';

import styles from './ToolRail.module.css';

/**
 * Fixed-width vertical strip pinned to the right edge of the IDE shell.
 * Clicking a button opens that tool in the adjacent ToolPane; clicking the
 * currently-active button closes the pane (toggle behavior is enforced by
 * `selectToolTab` inside ToolDockContext).
 *
 * The rail itself is always visible — it stays on screen even when no tool
 * pane is open, which is the Cursor / Fleet / Code-rail pattern.
 */
export function ToolRail() {
  const { activeToolTab, selectToolTab } = useToolDock();
  useRegistryVersion(); // re-render when a tool plugin is enabled/disabled
  return (
    <aside className={styles.rail} aria-label="Tool selector" role="tablist" aria-orientation="vertical">
      {pluginToolTabs().map((tab) => {
        const active = activeToolTab === tab.id;
        return (
          <button
            key={tab.id}
            type="button"
            role="tab"
            aria-selected={active}
            className={`${styles.btn} ${active ? styles.btnActive : ''}`}
            onClick={() => selectToolTab(tab.id)}
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
