import { useMemo } from 'react';

import { buildStubTopMenuHandlers } from '@boundary/menu/menuActionsRegistry';

import { mainMenuEntries } from '@domain/menu/mainMenu';

import styles from './MenuBar.module.css';

export function MenuBar() {
  const entries = useMemo(() => mainMenuEntries(), []);
  const topMenuHandlers = useMemo(() => buildStubTopMenuHandlers(), []);

  return (
    <nav className={styles.menuBar} aria-label="Main menu">
      <ul className={styles.menuList}>
        {entries.map(({ id, label }) => (
          <li key={id} className={styles.menuItemWrap}>
            <button type="button" className={styles.menuItem} onClick={() => topMenuHandlers[id]()}>
              {label}
            </button>
          </li>
        ))}
      </ul>
    </nav>
  );
}
