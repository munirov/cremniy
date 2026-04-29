import { describe, expect, it } from 'vitest';

import {
  normalizeAppPreferences,
  parseAppPreferences,
  stringifyAppPreferences,
  withOpenedWorkspacePinned,
  DEFAULT_APP_PREFERENCES,
  MAX_RECENT_WORKSPACES,
} from './appPreferences';

describe('appPreferences', () => {
  it('normalizes malformed input to defaults', () => {
    expect(normalizeAppPreferences(undefined)).toEqual(DEFAULT_APP_PREFERENCES);
    expect(normalizeAppPreferences([])).toEqual(DEFAULT_APP_PREFERENCES);
  });

  it('parseAppPreferences survives invalid JSON', () => {
    expect(parseAppPreferences('not-json')).toEqual(DEFAULT_APP_PREFERENCES);
  });

  it('roundtrips known preferences via JSON', () => {
    const prefs = {
      theme: 'light' as const,
      recentWorkspacePaths: ['C:\\a', 'D:\\b'],
    };
    const again = parseAppPreferences(stringifyAppPreferences(prefs));
    expect(again.theme).toBe(prefs.theme);
    expect(again.recentWorkspacePaths).toEqual(prefs.recentWorkspacePaths);
  });

  it('withOpenedWorkspacePinned dedupes, pins most recent, and caps list', () => {
    let p = DEFAULT_APP_PREFERENCES;
    p = withOpenedWorkspacePinned(p, '/first');
    p = withOpenedWorkspacePinned(p, '/second');
    p = withOpenedWorkspacePinned(p, '/first');

    expect(p.recentWorkspacePaths[0]).toBe('/first');
    expect(new Set(p.recentWorkspacePaths).size).toBe(p.recentWorkspacePaths.length);

    const manyPaths = [...Array(MAX_RECENT_WORKSPACES + 6)].map((_, i) => `/p${i}`);
    let wide = DEFAULT_APP_PREFERENCES;
    for (const path of manyPaths) {
      wide = withOpenedWorkspacePinned(wide, path);
    }
    expect(wide.recentWorkspacePaths.length).toBe(MAX_RECENT_WORKSPACES);
  });
});
