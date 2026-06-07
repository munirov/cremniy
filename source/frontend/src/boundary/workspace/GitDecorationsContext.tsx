import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from 'react';

import { gitRepos, gitStatus, type GitRepoRef, type GitStatus } from '@infrastructure/tauri/bridge';

import { useIdeSession } from './IdeSessionContext';
import {
  buildDecorations,
  decorationFor,
  EMPTY_DECO_MAPS,
  type GitDecoMaps,
  type GitDecoResult,
} from './gitDecorations';

type GitDecoApi = {
  decorationFor: (absPath: string, isDir: boolean) => GitDecoResult | null;
};

const GitDecoContext = createContext<GitDecoApi | null>(null);

/**
 * Feeds the Explorer tree with git decorations (status badge + colour per row).
 * Repo discovery (the workspace walk) runs only when the workspace changes;
 * per-repo status refreshes on every fileTreeRevision tick (focus/poll), so the
 * badges stay live as files change — no manual refresh.
 */
export function GitDecorationsProvider({
  workspaceRoot,
  children,
}: {
  workspaceRoot: string;
  children: ReactNode;
}) {
  const { fileTreeRevision } = useIdeSession();
  const [repos, setRepos] = useState<GitRepoRef[]>([]);
  const [maps, setMaps] = useState<GitDecoMaps>(EMPTY_DECO_MAPS);

  // Discover repos once per workspace (NOT per refresh tick — this walks the tree).
  useEffect(() => {
    let cancelled = false;
    const root = workspaceRoot.trim();
    if (root === '') {
      setRepos([]);
      return;
    }
    void gitRepos(root)
      .then((r) => {
        if (!cancelled) setRepos(r);
      })
      .catch(() => {
        if (!cancelled) setRepos([]);
      });
    return () => {
      cancelled = true;
    };
  }, [workspaceRoot]);

  // Refresh status on every tick (cheap) and rebuild the decoration maps.
  useEffect(() => {
    let cancelled = false;
    const root = workspaceRoot.trim();
    if (root === '' || repos.length === 0) {
      setMaps(EMPTY_DECO_MAPS);
      return;
    }
    void Promise.all(repos.map((r) => gitStatus(r.path).catch(() => null))).then((arr) => {
      if (cancelled) return;
      const statuses = arr.filter((s): s is GitStatus => s != null);
      setMaps(buildDecorations(statuses, root));
    });
    return () => {
      cancelled = true;
    };
  }, [repos, workspaceRoot, fileTreeRevision]);

  const api = useMemo<GitDecoApi>(
    () => ({ decorationFor: (absPath, isDir) => decorationFor(maps, absPath, isDir) }),
    [maps],
  );

  return <GitDecoContext.Provider value={api}>{children}</GitDecoContext.Provider>;
}

/** Decoration for one tree row, or null when there's no provider / no change. */
export function useGitDecoration(absPath: string, isDir: boolean): GitDecoResult | null {
  const ctx = useContext(GitDecoContext);
  if (ctx == null) {
    return null;
  }
  return ctx.decorationFor(absPath, isDir);
}
