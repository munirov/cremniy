import type { ReactNode } from 'react';

import styles from './RootLayout.module.css';

export type RootLayoutProps = {
  /** Optional in-shell header. The window menu now lives in the TitleBar, so
      this is normally omitted — kept for any future in-shell toolbar. */
  header?: ReactNode;
  sidebar?: ReactNode;
  children?: ReactNode;
  footerPanel?: ReactNode;
  /** When false, the terminal footer region is collapsed (View menu parity). */
  footerVisible?: boolean;
};

export function RootLayout({
  header,
  sidebar,
  children,
  footerPanel,
  footerVisible = true,
}: RootLayoutProps) {
  const hasSidebar = sidebar != null;
  const hasFooter = footerPanel != null;

  return (
    <div className={styles.rootShell}>
      {header != null ? <header className={styles.shellHeader}>{header}</header> : null}
      <main className={styles.shellMainOuter}>
        <div className={styles.shellMainRow}>
          {hasSidebar ? <aside className={styles.shellSidebar}>{sidebar}</aside> : null}
          <section className={styles.shellContent} aria-label="Editor area">
            {children}
          </section>
        </div>
      </main>
      {hasFooter ? (
        <footer
          className={`${styles.shellFooterPanel} ${footerVisible ? '' : styles.shellFooterPanelHidden}`}
          data-testid="ide-terminal-footer"
          aria-label="Terminal"
        >
          {footerVisible ? (
            <>
              <div className={styles.shellFooterPanelGrip} aria-hidden="true" />
              <div className={styles.shellFooterInner}>{footerPanel}</div>
            </>
          ) : null}
        </footer>
      ) : null}
    </div>
  );
}
