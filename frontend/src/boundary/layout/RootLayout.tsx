import type { ReactNode } from 'react';

import styles from './RootLayout.module.css';

export type RootLayoutProps = {
  header?: ReactNode;
  sidebar?: ReactNode;
  children?: ReactNode;
  footerPanel?: ReactNode;
};

function defaultHeader() {
  return <p className={styles.placeholder}>Menu bar placeholder</p>;
}

function defaultFooterPanel() {
  return <p className={styles.placeholder}>Terminal placeholder</p>;
}

export function RootLayout({ header = defaultHeader(), sidebar, children, footerPanel = defaultFooterPanel() }: RootLayoutProps) {
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
      <footer className={styles.shellFooterPanel} aria-label="Terminal">
        <div className={styles.shellFooterPanelGrip} aria-hidden="true" />
        <div className={styles.shellFooterInner}>{footerPanel}</div>
      </footer>
    </div>
  );
}
