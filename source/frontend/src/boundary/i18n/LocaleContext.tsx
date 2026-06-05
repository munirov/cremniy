import { createContext, useCallback, useContext, useMemo, useState, type ReactNode } from 'react';

import { translate, type LocaleId } from '@domain/i18n/translations';

type Ctx = {
  locale: LocaleId;
  setLocale: (locale: LocaleId) => void;
  t: (key: string) => string;
};

const LocaleContext = createContext<Ctx | null>(null);

export function LocaleProvider({
  children,
  initial = 'en',
}: {
  children: ReactNode;
  initial?: LocaleId;
}) {
  const [locale, setLocale] = useState<LocaleId>(initial);
  const value = useMemo<Ctx>(
    () => ({
      locale,
      setLocale,
      t: (key: string) => translate(key, locale),
    }),
    [locale],
  );
  return <LocaleContext.Provider value={value}>{children}</LocaleContext.Provider>;
}

export function useLocale(): Ctx {
  const v = useContext(LocaleContext);
  if (v == null) {
    // Fallback to English t() when no provider (e.g. unit tests) — never throws.
    return useMemo<Ctx>(
      () => ({
        locale: 'en',
        setLocale: () => undefined,
        t: (key: string) => translate(key, 'en'),
      }),
      [],
    );
  }
  return v;
}

export function useT(): (key: string) => string {
  return useLocale().t;
}

export function useChangeLocale(): (locale: LocaleId) => void {
  return useLocale().setLocale;
}
