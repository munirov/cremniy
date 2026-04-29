export type ThemePreference = 'dark' | 'light';

export type AppPreferences = {
  theme: ThemePreference;
  recentWorkspacePaths: string[];
};

export const MAX_RECENT_WORKSPACES = 10;

export const DEFAULT_APP_PREFERENCES: AppPreferences = {
  theme: 'dark',
  recentWorkspacePaths: [],
};

function isThemePreference(v: unknown): v is ThemePreference {
  return v === 'dark' || v === 'light';
}

export function normalizeAppPreferences(parsed: unknown): AppPreferences {
  if (parsed == null || typeof parsed !== 'object' || Array.isArray(parsed)) {
    return DEFAULT_APP_PREFERENCES;
  }
  const o = parsed as Record<string, unknown>;

  let theme = DEFAULT_APP_PREFERENCES.theme;
  if (isThemePreference(o.theme)) {
    theme = o.theme;
  }

  let recentWorkspacePaths = DEFAULT_APP_PREFERENCES.recentWorkspacePaths;
  if (Array.isArray(o.recentWorkspacePaths)) {
    recentWorkspacePaths = o.recentWorkspacePaths.filter((p): p is string => typeof p === 'string');
  }

  return { theme, recentWorkspacePaths };
}

export function parseAppPreferences(json: string): AppPreferences {
  try {
    const raw: unknown = JSON.parse(json);
    return normalizeAppPreferences(raw);
  } catch {
    return DEFAULT_APP_PREFERENCES;
  }
}

export function stringifyAppPreferences(prefs: AppPreferences): string {
  return `${JSON.stringify(
    {
      theme: prefs.theme,
      recentWorkspacePaths: prefs.recentWorkspacePaths,
    },
    null,
    0,
  )}\n`;
}

export function withOpenedWorkspacePinned(prefs: AppPreferences, openedPath: string): AppPreferences {
  const trimmed = openedPath.trim();
  if (trimmed === '') {
    return prefs;
  }
  const rest = prefs.recentWorkspacePaths.filter((p) => p !== trimmed);
  const next = [trimmed, ...rest].slice(0, MAX_RECENT_WORKSPACES);
  return {
    ...prefs,
    recentWorkspacePaths: next,
  };
}
