import { useMemo } from 'react';
import { useNavigate } from 'react-router-dom';

import { MenuBar } from '@boundary/chrome/MenuBar';
import { closeWorkspaceHandler } from '@boundary/menu/menuActionsRegistry';
import { IdeWorkspace } from '@boundary/layout/IdeWorkspace';
import { RootLayout } from '@boundary/layout/RootLayout';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './RootApp.module.css';

export function RootApp() {
  const navigate = useNavigate();
  const workspaceRoot = useWorkspaceRoot();
  const onCloseWorkspace = useMemo(() => closeWorkspaceHandler(navigate), [navigate]);

  return (
    <RootLayout header={<MenuBar />}>
      <IdeWorkspace workspaceRoot={workspaceRoot} onCloseWorkspace={onCloseWorkspace}>
        <h1 className={styles.title}>Cremniy UI</h1>
      </IdeWorkspace>
    </RootLayout>
  );
}
