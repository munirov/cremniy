import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useState,
  type ReactNode,
} from 'react';

/**
 * Tiny portal-like slot for the top titlebar. The TitleBar lives one level
 * above the React Router routes (so the window chrome is visible even on the
 * Welcome screen), while the MenuBar itself only exists inside RootApp
 * (`/ide`). This context lets RootApp publish its menu node — and a settings
 * action for the titlebar's gear — without a real DOM portal.
 */
type Ctx = {
  menu: ReactNode;
  setMenu: (node: ReactNode) => void;
  /** Opens preferences; the TitleBar renders a gear when this is set. */
  settingsAction: (() => void) | null;
  setSettingsAction: (fn: (() => void) | null) => void;
};

const MenuSlotContext = createContext<Ctx | null>(null);

export function MenuSlotProvider({ children }: { children: ReactNode }) {
  const [menu, setMenuState] = useState<ReactNode>(null);
  const [settingsAction, setSettingsActionState] = useState<(() => void) | null>(null);
  const setMenu = useCallback((node: ReactNode) => setMenuState(node), []);
  // Wrap in an updater so a function value is stored, not invoked.
  const setSettingsAction = useCallback(
    (fn: (() => void) | null) => setSettingsActionState(() => fn),
    [],
  );
  const value = useMemo<Ctx>(
    () => ({ menu, setMenu, settingsAction, setSettingsAction }),
    [menu, setMenu, settingsAction, setSettingsAction],
  );
  return <MenuSlotContext.Provider value={value}>{children}</MenuSlotContext.Provider>;
}

export function useMenuSlot(): Ctx {
  const v = useContext(MenuSlotContext);
  if (v == null) {
    return {
      menu: null,
      setMenu: () => undefined,
      settingsAction: null,
      setSettingsAction: () => undefined,
    };
  }
  return v;
}
