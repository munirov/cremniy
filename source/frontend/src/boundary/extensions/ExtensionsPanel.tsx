import { useState } from 'react';

import { PLUGINS } from '@plugins/index';
import type { PluginManifest } from '@shared/plugins/contributions';
import { isPluginDisabled, setPluginEnabled } from '@shared/plugins/pluginState';

import styles from './ExtensionsPanel.module.css';

function isBundled(p: PluginManifest): boolean {
  return (p.delivery ?? 'recommended') === 'bundled';
}

/** One-line summary of what a plugin contributes, for the card. */
function contributionSummary(p: PluginManifest): string {
  const n = (arr: unknown[] | undefined, one: string, many = `${one}s`) =>
    arr != null && arr.length > 0 ? `${arr.length} ${arr.length === 1 ? one : many}` : null;
  return [
    n(p.views, 'view'),
    n(p.toolTabs, 'tool'),
    n(p.centerPanels, 'panel'),
    n(p.commands, 'command'),
    n(p.menuItems, 'menu item'),
  ]
    .filter(Boolean)
    .join(' · ');
}

/**
 * Extensions — the plugin manager (a core side-panel view, like VS Code's). It
 * lists every plugin from the catalog (plugins/index.ts): `bundled` ones ship
 * with the IDE and are always on; `recommended` ones are official add-ons the
 * user enables here so the base package stays small. Toggling persists and takes
 * effect on reload (contributions are read across the render tree, so re-running
 * loadPlugins() on reload is cleaner than hot-swapping). A server-backed
 * marketplace for third-party plugins is a later step.
 */
export function ExtensionsPanel() {
  const [, force] = useState(0);
  const [dirty, setDirty] = useState(false);

  const toggle = (id: string, enabled: boolean) => {
    setPluginEnabled(id, enabled);
    setDirty(true);
    force((x) => x + 1);
  };

  // "Active" = would load on next start (bundled, or recommended-and-not-disabled).
  const active = PLUGINS.filter((p) => isBundled(p) || !isPluginDisabled(p.id));
  const available = PLUGINS.filter((p) => !isBundled(p) && isPluginDisabled(p.id));

  return (
    <div className={styles.panel} aria-label="Extensions">
      <div className={styles.header}>Extensions</div>

      {dirty ? (
        <div className={styles.reloadBar} role="status">
          <span>Reload to apply.</span>
          <button type="button" className={styles.reloadBtn} onClick={() => window.location.reload()}>
            Reload
          </button>
        </div>
      ) : null}

      <div className={styles.scroll}>
        <div className={styles.section}>Installed — {active.length}</div>
        <ul className={styles.list}>
          {active.map((p) => (
            <li key={p.id} className={styles.card}>
              <div className={styles.cardHead}>
                <span className={styles.name} title={p.name}>
                  {p.name}
                </span>
                <span className={`${styles.badge} ${isBundled(p) ? styles.badgeBundled : styles.badgeRec}`}>
                  {isBundled(p) ? 'Bundled' : 'Recommended'}
                </span>
              </div>
              {p.description != null ? <div className={styles.desc}>{p.description}</div> : null}
              <div className={styles.contribs}>{contributionSummary(p)}</div>
              {!isBundled(p) ? (
                <div className={styles.actions}>
                  <button type="button" className={styles.btn} onClick={() => toggle(p.id, false)}>
                    Disable
                  </button>
                </div>
              ) : null}
            </li>
          ))}
        </ul>

        {available.length > 0 ? (
          <>
            <div className={styles.section}>Recommended — {available.length}</div>
            <ul className={styles.list}>
              {available.map((p) => (
                <li key={p.id} className={`${styles.card} ${styles.cardOff}`}>
                  <div className={styles.cardHead}>
                    <span className={styles.name}>{p.name}</span>
                    <span className={`${styles.badge} ${styles.badgeRec}`}>Recommended</span>
                  </div>
                  {p.description != null ? <div className={styles.desc}>{p.description}</div> : null}
                  <div className={styles.actions}>
                    <button type="button" className={styles.btnPrimary} onClick={() => toggle(p.id, true)}>
                      Install
                    </button>
                  </div>
                </li>
              ))}
            </ul>
          </>
        ) : null}

        <p className={styles.note}>
          Bundled plugins ship with Cremniy. Recommended ones are official add-ons. Installing
          third-party plugins from a server is planned.
        </p>
      </div>
    </div>
  );
}
