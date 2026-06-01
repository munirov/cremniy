import { createContext, useContext, useMemo, type ReactNode } from 'react';
import { useSearchParams } from 'react-router-dom';

import type { WorkspaceRoot } from '@domain/workspace/types';

const WorkspaceContext = createContext<WorkspaceRoot | null>(null);

export function WorkspaceProvider({ children }: { children: ReactNode }) {
  const [searchParams] = useSearchParams();
  const workspaceRoot = useMemo<WorkspaceRoot | null>(() => {
    const raw = searchParams.get('root');
    if (raw == null || raw === '') {
      return null;
    }
    return { path: raw };
  }, [searchParams]);

  return (
    <WorkspaceContext.Provider value={workspaceRoot}>{children}</WorkspaceContext.Provider>
  );
}

export function useWorkspaceRoot(): WorkspaceRoot | null {
  return useContext(WorkspaceContext);
}
