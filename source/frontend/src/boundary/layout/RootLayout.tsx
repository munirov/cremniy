import type { ReactNode } from 'react';

import styles from './RootLayout.module.css';

export type RootLayoutProps = {
  header?: ReactNode;
  sidebar?: ReactNode;
  children?: ReactNode;
  footerPanel?: ReactNode;
  /** When false, the terminal footer region is collapsed (View menu parity). */
  footerVisible?: boolean;
};

function defaultHeader() {
  return <p className={styles.placeholder}>Menu bar placeholder</p>;
}

function defaultFooterPanel() {
  return <p className={styles.placeholder}>Terminal panel unavailable</p>;
}

export function RootLayout({
  header = defaultHeader(),
  sidebar,
  children,
  footerPanel = defaultFooterPanel(),
  footerVisible = true,
}: RootLayoutProps) {
  const hasSidebar = sidebar != null;

  return (
    <div className={styles.rootShell}>
      <header className={styles.shellHeader}>{header}</header>
      <main className={styles.shellMainOuter}>
        <div className={styles.shellMainRow}>
          {hasSidebar ? <aside className={styles.shellSidebar}>{sidebar}</aside> : null}
          <section className={styles.shellContent} aria-label="Editor area">
            {children}
          </section>
        </div>
      </main>
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
    </div>
  );
}
