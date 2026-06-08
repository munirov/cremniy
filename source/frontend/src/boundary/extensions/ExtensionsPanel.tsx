import { PLUGINS } from '@plugins/index';
import type { PluginManifest } from '@shared/plugins/contributions';
import { isPluginDisabled } from '@shared/plugins/pluginState';
import { setPluginActive } from '@shared/plugins/pluginToggle';
import { useRegistryVersion } from '@shared/plugins/useRegistry';
import { useIdeSessionOptional } from '@boundary/workspace/IdeSessionContext';

import { PluginGlyph } from './PluginGlyph';
import { setSelectedExtension } from './extensionDetailsStore';

import styles from './ExtensionsPanel.module.css';

const DETAILS_PANEL_ID = 'extensionDetails';

function isBundled(p: PluginManifest): boolean {
  return (p.delivery ?? 'recommended') === 'bundled';
}

/**
 * Extensions — the plugin manager (a core side-panel view, like VS Code's). It
 * lists every plugin from the catalog (plugins/index.ts) and lets you turn each
 * one on or off. Toggling is LIVE: enabling/disabling adds/removes the plugin
 * from the active registry, so its UI — side-panel view, tool-rail tools, center
 * panels, menu items, commands — appears or disappears immediately (no reload),
 * via useRegistryVersion. The choice persists across restarts (pluginState).
 *
 * Uninstall / download-from-server isn't available yet (everything ships in the
 * local build); the `bundled` / `recommended` badge only marks where a plugin
 * came from. Each row opens the plugin's details center-tab on click.
 */
export function ExtensionsPanel() {
  // Re-render whenever any plugin is enabled/disabled (here or elsewhere).
  useRegistryVersion();
  const ide = useIdeSessionOptional();

  const openDetails = (id: string) => {
    setSelectedExtension(id);
    ide?.openPanel(DETAILS_PANEL_ID);
  };

  const enabled = PLUGINS.filter((p) => !isPluginDisabled(p.id));
  const disabled = PLUGINS.filter((p) => isPluginDisabled(p.id));

  const renderRow = (p: PluginManifest, off: boolean) => (
    <li key={p.id}>
      <div
        className={`${styles.card} ${off ? styles.cardOff : ''}`}
        role="button"
        tabIndex={0}
        aria-label={`${p.name} — view details`}
        onClick={() => openDetails(p.id)}
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            openDetails(p.id);
          }
        }}
        title={p.name}
      >
        <PluginGlyph path={p.icon} size={26} className={styles.cardIcon} />
        <div className={styles.cardBody}>
          <div className={styles.cardHead}>
            <span className={styles.name} title={p.name}>
              {p.name}
            </span>
            <span className={`${styles.badge} ${isBundled(p) ? styles.badgeBundled : styles.badgeRec}`}>
              {isBundled(p) ? 'Bundled' : 'Recommended'}
            </span>
          </div>
          {p.author != null ? <div className={styles.author}>{p.author}</div> : null}
          {p.description != null ? <div className={styles.desc}>{p.description}</div> : null}
          <div className={styles.actions}>
            {off ? (
              <button
                type="button"
                className={styles.btnPrimary}
                onClick={(e) => {
                  e.stopPropagation();
                  setPluginActive(p.id, true);
                }}
              >
                Enable
              </button>
            ) : (
              <button
                type="button"
                className={styles.btn}
                onClick={(e) => {
                  e.stopPropagation();
                  setPluginActive(p.id, false);
                }}
              >
                Disable
              </button>
            )}
          </div>
        </div>
      </div>
    </li>
  );

  return (
    <div className={styles.panel} aria-label="Extensions">
      <div className={styles.header}>Extensions</div>

      <div className={styles.scroll}>
        <div className={styles.section}>Enabled — {enabled.length}</div>
        <ul className={styles.list}>{enabled.map((p) => renderRow(p, false))}</ul>

        {disabled.length > 0 ? (
          <>
            <div className={styles.section}>Disabled — {disabled.length}</div>
            <ul className={styles.list}>{disabled.map((p) => renderRow(p, true))}</ul>
          </>
        ) : null}

        <p className={styles.note}>
          Turn any plugin off and its UI disappears live; turn it back on and it returns. Bundled
          plugins ship with Cremniy; recommended ones are official add-ons. Installing third-party
          plugins from a server is planned.
        </p>
      </div>
    </div>
  );
}
