import { useState } from 'react';

import { PLUGINS } from '@plugins/index';
import type { PluginManifest } from '@shared/plugins/contributions';
import { isPluginDisabled, setPluginEnabled } from '@shared/plugins/pluginState';
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
 * lists every plugin from the catalog (plugins/index.ts): `bundled` ones ship
 * with the IDE and are always on; `recommended` ones are official add-ons the
 * user enables here so the base package stays small. Toggling persists and takes
 * effect on reload (contributions are read across the render tree, so re-running
 * loadPlugins() on reload is cleaner than hot-swapping). A server-backed
 * marketplace for third-party plugins is a later step.
 *
 * Each row is a VS Code-style entry (icon + name + author + delivery badge +
 * truncated description). The whole row is clickable: it points the shared
 * details store at the plugin and opens the `extensionDetails` center tab.
 */
export function ExtensionsPanel() {
  const [, force] = useState(0);
  const [dirty, setDirty] = useState(false);
  // Optional: the panel also renders in unit tests without the IDE provider, so
  // opening the details tab is simply inert there rather than throwing.
  const ide = useIdeSessionOptional();

  const toggle = (id: string, enabled: boolean) => {
    setPluginEnabled(id, enabled);
    setDirty(true);
    force((x) => x + 1);
  };

  const openDetails = (id: string) => {
    setSelectedExtension(id);
    ide?.openPanel(DETAILS_PANEL_ID);
  };

  // "Active" = would load on next start (bundled, or recommended-and-not-disabled).
  const active = PLUGINS.filter((p) => isBundled(p) || !isPluginDisabled(p.id));
  const available = PLUGINS.filter((p) => !isBundled(p) && isPluginDisabled(p.id));

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
            {isBundled(p) ? null : off ? (
              <button
                type="button"
                className={styles.btnPrimary}
                onClick={(e) => {
                  e.stopPropagation();
                  toggle(p.id, true);
                }}
              >
                Install
              </button>
            ) : (
              <button
                type="button"
                className={styles.btn}
                onClick={(e) => {
                  e.stopPropagation();
                  toggle(p.id, false);
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
        <ul className={styles.list}>{active.map((p) => renderRow(p, false))}</ul>

        {available.length > 0 ? (
          <>
            <div className={styles.section}>Recommended — {available.length}</div>
            <ul className={styles.list}>{available.map((p) => renderRow(p, true))}</ul>
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
