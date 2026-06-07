import { describe, expect, it } from 'vitest';

import type { GitFileStatus, GitStatus } from '@infrastructure/tauri/bridge';

import { buildDecorations, decorationFor, fileDeco } from './gitDecorations';

function gf(partial: Partial<GitFileStatus> & { absPath: string }): GitFileStatus {
  return {
    path: partial.path ?? partial.absPath,
    absPath: partial.absPath,
    name: partial.name ?? (partial.absPath.split(/[/\\]/).pop() ?? ''),
    indexStatus: partial.indexStatus ?? ' ',
    workStatus: partial.workStatus ?? ' ',
    staged: partial.staged ?? false,
    untracked: partial.untracked ?? false,
    isDir: partial.isDir ?? false,
  };
}

function repo(files: GitFileStatus[]): GitStatus {
  return { isRepo: true, branch: 'main', ahead: 0, behind: 0, files };
}

describe('fileDeco', () => {
  it('untracked → U', () => {
    expect(fileDeco(gf({ absPath: 'C:\\r\\a.txt', untracked: true })).kind).toBe('untracked');
  });
  it('working-tree modified → M', () => {
    expect(fileDeco(gf({ absPath: 'C:\\r\\a.txt', workStatus: 'M' })).letter).toBe('M');
  });
  it('staged add → A', () => {
    expect(fileDeco(gf({ absPath: 'C:\\r\\a.txt', indexStatus: 'A' })).kind).toBe('added');
  });
  it('deleted → D', () => {
    expect(fileDeco(gf({ absPath: 'C:\\r\\a.txt', workStatus: 'D' })).kind).toBe('deleted');
  });
  it('both-added is a conflict', () => {
    expect(fileDeco(gf({ absPath: 'C:\\r\\a.txt', indexStatus: 'A', workStatus: 'A' })).kind).toBe(
      'conflict',
    );
  });
  it('renamed → R', () => {
    expect(fileDeco(gf({ absPath: 'C:\\r\\a.txt', indexStatus: 'R' })).letter).toBe('R');
  });
});

describe('buildDecorations + decorationFor', () => {
  const root = 'C:\\repo';
  const maps = buildDecorations(
    [
      repo([
        gf({ absPath: 'C:\\repo\\src\\main.c', workStatus: 'M' }),
        gf({ absPath: 'C:\\repo\\notes.txt', untracked: true }),
      ]),
    ],
    root,
  );

  it('decorates the changed file directly, with a letter', () => {
    const d = decorationFor(maps, 'C:\\repo\\src\\main.c', false);
    expect(d?.deco.letter).toBe('M');
    expect(d?.rollup).toBe(false);
  });

  it('rolls the change up to ancestor folders as a tint (no letter)', () => {
    const d = decorationFor(maps, 'C:\\repo\\src', true);
    expect(d?.deco.kind).toBe('modified');
    expect(d?.rollup).toBe(true);
  });

  it('matches case- and separator-insensitively', () => {
    expect(decorationFor(maps, 'C:/REPO/SRC/MAIN.C', false)?.deco.letter).toBe('M');
    expect(decorationFor(maps, 'C:\\repo\\src\\main.c', false)?.deco.letter).toBe('M');
  });

  it('returns null for unchanged paths', () => {
    expect(decorationFor(maps, 'C:\\repo\\readme.md', false)).toBeNull();
  });

  it('treats an untracked directory as a direct decoration', () => {
    const m2 = buildDecorations(
      [repo([gf({ absPath: 'C:\\repo\\dist', untracked: true, isDir: true })])],
      root,
    );
    const d = decorationFor(m2, 'C:\\repo\\dist', true);
    expect(d?.rollup).toBe(false);
    expect(d?.deco.kind).toBe('untracked');
  });

  it('folder rollup takes the highest-priority child', () => {
    const m3 = buildDecorations(
      [
        repo([
          gf({ absPath: 'C:\\repo\\src\\a.txt', untracked: true }),
          gf({ absPath: 'C:\\repo\\src\\b.txt', workStatus: 'M' }),
        ]),
      ],
      root,
    );
    // modified (priority 4) beats untracked (priority 1)
    expect(decorationFor(m3, 'C:\\repo\\src', true)?.deco.kind).toBe('modified');
  });
});
