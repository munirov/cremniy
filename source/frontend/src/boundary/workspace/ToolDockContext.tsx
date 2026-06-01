import { createContext, useCallback, useContext, useMemo, useState, type ReactNode } from 'react';

import type { ToolTabId } from '@domain/toolTabs/toolTabId';

type ToolDockContextValue = {
  activeToolTab: ToolTabId | null;
  setActiveToolTab: (id: ToolTabId | null) => void;
  selectToolTab: (id: ToolTabId) => void;
};

const ToolDockContext = createContext<ToolDockContextValue | null>(null);

export function ToolDockProvider({ children }: { children: ReactNode }) {
  const [activeToolTab, setActiveToolTab] = useState<ToolTabId | null>(null);

  const selectToolTab = useCallback((id: ToolTabId) => {
    setActiveToolTab((cur) => (cur === id ? null : id));
  }, []);

  const value = useMemo(
    () => ({
      activeToolTab,
      setActiveToolTab,
      selectToolTab,
    }),
    [activeToolTab, selectToolTab],
  );

  return <ToolDockContext.Provider value={value}>{children}</ToolDockContext.Provider>;
}

export function useToolDock(): ToolDockContextValue {
  const ctx = useContext(ToolDockContext);
  if (ctx == null) {
    throw new Error('useToolDock must be used within ToolDockProvider');
  }
  return ctx;
}
