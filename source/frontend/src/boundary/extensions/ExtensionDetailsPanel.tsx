import { useState } from 'react';

import { PLUGINS } from '@plugins/index';
import type { PluginManifest } from '@shared/plugins/contributions';
import { isPluginDisabled, setPluginEnabled } from '@shared/plugins/pluginState';

import { Markdown } from './Markdown';
import { PluginGlyph } from './PluginGlyph';
import { getSelectedExtension } from './extensionDetailsStore';

import styles from './ExtensionDetailsPanel.module.css';

function isBundled(p: PluginManifest): boolean {
  return (p.delivery ?? 'recommended') === 'bundled';
}

// Small external-link glyph for the Resources rows (Lucide arrow-up-right-box).
const LINK_ICON = 'M15 3h6v6M10 14 21 3M21 14v5a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5';

/** One row in the "Contributes" sidebar block. */
type ContribRow = { kind: string; name: string; mono?: boolean };

/** Flatten a manifest's contributions into named rows the user can read. */
function contributes(p: PluginManifest): ContribRow[] {
  const rows: ContribRow[] = [];
  for (const v of p.views ?? []) rows.push({ kind: 'View', name: v.label });
  for (const t of p.toolTabs ?? []) rows.push({ kind: 'Tool', name: t.label });
  for (const c of p.centerPanels ?? []) rows.push({ kind: 'Panel', name: c.label });
  for (const c of p.commands ?? []) rows.push({ kind: 'Command', name: c.name, mono: true });
  return rows;
}

/**
 * Extension details — a core center tab (registered as `extensionDetails` in
 * centerPanels.tsx). It shows one plugin in the VS Code "Extension: <name>"
 * spirit: a header (icon + name + author + version + delivery badge + the
 * Install/Disable action) and a two-column body — the rendered `readme` on the
 * left, metadata blocks (identifier / version / categories / resources /
 * contributes) on the right.
 *
 * The panel is parameterised out-of-band: center panels open by id with no args,
 * so the Extensions panel writes the chosen plugin id to extensionDetailsStore
 * before calling openPanel('extensionDetails'); this reads it on render.
 */
export function ExtensionDetailsPanel() {
  const [, force] = useState(0);
  const [dirty, setDirty] = useState(false);

  const id = getSelectedExtension();
  const plugin = id != null ? PLUGINS.find((p) => p.id === id) : undefined;

  if (plugin == null) {
    return (
      <div className={styles.panel}>
        <div className={styles.empty}>Select an extension from the Extensions panel.</div>
      </div>
    );
  }

  const bundled = isBundled(plugin);
  const disabled = !bundled && isPluginDisabled(plugin.id);

  const toggle = (enabled: boolean) => {
    setPluginEnabled(plugin.id, enabled);
    setDirty(true);
    force((x) => x + 1);
  };

  const rows = contributes(plugin);
  const body = plugin.readme ?? plugin.description ?? '';

  return (
    <div className={styles.panel} aria-label={`Extension: ${plugin.name}`}>
      <div className={styles.header}>
        <PluginGlyph path={plugin.icon} size={44} className={styles.icon} />
        <div className={styles.headMeta}>
          <div className={styles.titleRow}>
            <span className={styles.name}>{plugin.name}</span>
            {plugin.version != null ? (
              <span className={styles.version}>v{plugin.version}</span>
            ) : null}
            <span className={`${styles.badge} ${bundled ? styles.badgeBundled : styles.badgeRec}`}>
              {bundled ? 'Bundled' : 'Recommended'}
            </span>
          </div>
          <div className={styles.subRow}>
            {plugin.author != null ? <span className={styles.author}>{plugin.author}</span> : null}
            {plugin.description != null ? <span>· {plugin.description}</span> : null}
          </div>
          <div className={styles.headActions}>
            {!bundled ? (
              disabled ? (
                <button type="button" className={styles.btnPrimary} onClick={() => toggle(true)}>
                  Install
                </button>
              ) : (
                <button type="button" className={styles.btn} onClick={() => toggle(false)}>
                  Disable
                </button>
              )
            ) : null}
            {dirty ? (
              <span className={styles.reloadNote}>
                Reload to apply.
                <button
                  type="button"
                  className={styles.reloadLink}
                  onClick={() => window.location.reload()}
                >
                  Reload
                </button>
              </span>
            ) : null}
          </div>
        </div>
      </div>

      <div className={styles.body}>
        <div className={styles.main}>
          <div className={styles.mainInner}>
            {body !== '' ? (
              <Markdown source={body} />
            ) : (
              <p className={styles.reloadNote}>No description provided.</p>
            )}
          </div>
        </div>

        <aside className={styles.sidebar} aria-label="Details">
          <div className={styles.metaBlock}>
            <div className={styles.metaLabel}>Identifier</div>
            <div className={styles.metaMono}>{plugin.id}</div>
          </div>

          {plugin.version != null ? (
            <div className={styles.metaBlock}>
              <div className={styles.metaLabel}>Version</div>
              <div className={styles.metaMono}>{plugin.version}</div>
            </div>
          ) : null}

          {plugin.categories != null && plugin.categories.length > 0 ? (
            <div className={styles.metaBlock}>
              <div className={styles.metaLabel}>Categories</div>
              <div className={styles.chips}>
                {plugin.categories.map((c) => (
                  <span key={c} className={styles.chip}>
                    {c}
                  </span>
                ))}
              </div>
            </div>
          ) : null}

          {plugin.links != null && plugin.links.length > 0 ? (
            <div className={styles.metaBlock}>
              <div className={styles.metaLabel}>Resources</div>
              <div className={styles.links}>
                {plugin.links.map((l) => (
                  <a
                    key={l.url}
                    className={styles.linkRow}
                    href={l.url}
                    target="_blank"
                    rel="noreferrer noopener"
                  >
                    <svg
                      className={styles.linkIcon}
                      viewBox="0 0 24 24"
                      fill="none"
                      stroke="currentColor"
                      strokeWidth={1.7}
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      aria-hidden
                    >
                      <path d={LINK_ICON} />
                    </svg>
                    <span>{l.label}</span>
                  </a>
                ))}
              </div>
            </div>
          ) : null}

          {rows.length > 0 ? (
            <div className={styles.metaBlock}>
              <div className={styles.metaLabel}>Contributes</div>
              <div className={styles.contribList}>
                {rows.map((r, n) => (
                  <div key={`${r.kind}-${n}`} className={styles.contribItem}>
                    <span className={styles.contribKind}>{r.kind}</span>
                    <span className={`${styles.contribName} ${r.mono ? styles.contribCmd : ''}`}>
                      {r.name}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          ) : null}
        </aside>
      </div>
    </div>
  );
}
