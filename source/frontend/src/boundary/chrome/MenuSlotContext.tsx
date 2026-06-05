import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useState,
  type ReactNode,
} from 'react';

/**
 * Tiny portal-like slot for the top titlebar's menu area. The TitleBar lives
 * one level above the React Router routes (so the window chrome is visible
 * even on the Welcome screen), while the MenuBar itself only exists inside
 * RootApp (`/ide`). This context lets RootApp publish its menu node and the
 * TitleBar render it without needing a real DOM portal.
 */
type Ctx = {
  menu: ReactNode;
  setMenu: (node: ReactNode) => void;
};

const MenuSlotContext = createContext<Ctx | null>(null);

export function MenuSlotProvider({ children }: { children: ReactNode }) {
  const [menu, setMenuState] = useState<ReactNode>(null);
  const setMenu = useCallback((node: ReactNode) => setMenuState(node), []);
  const value = useMemo<Ctx>(() => ({ menu, setMenu }), [menu, setMenu]);
  return <MenuSlotContext.Provider value={value}>{children}</MenuSlotContext.Provider>;
}

export function useMenuSlot(): Ctx {
  const v = useContext(MenuSlotContext);
  if (v == null) {
    return { menu: null, setMenu: () => undefined };
  }
  return v;
}
