import { describe, expect, it } from 'vitest';

import { fileNameFromPath, joinFilePath, normalizeFsPath, parentDirectoryPath } from './paths';

describe('fileNameFromPath', () => {
  it('returns filename for backslash path', () => {
    expect(fileNameFromPath('C:\\a\\b.txt')).toBe('b.txt');
  });

  it('returns filename for forward slash path', () => {
    expect(fileNameFromPath('/home/u/readme.md')).toBe('readme.md');
  });

  it('returns segment when no directory', () => {
    expect(fileNameFromPath('notes.txt')).toBe('notes.txt');
  });

  it('returns empty for whitespace-only input', () => {
    expect(fileNameFromPath('   ')).toBe('');
  });
});

describe('parentDirectoryPath', () => {
  it('returns parent for backslash path', () => {
    expect(parentDirectoryPath('C:\\a\\b.txt')).toBe('C:\\a');
  });

  it('returns parent for forward slash path', () => {
    expect(parentDirectoryPath('/home/u/f.txt')).toBe('/home/u');
  });

  it('returns empty for bare filename', () => {
    expect(parentDirectoryPath('readme.txt')).toBe('');
  });

  it('returns empty for whitespace-only input', () => {
    expect(parentDirectoryPath('   ')).toBe('');
  });

  it('trims surrounding whitespace before resolving parent', () => {
    expect(parentDirectoryPath('  /a/b.txt  ')).toBe('/a');
  });

  it('uses the rightmost separator when mixing slashes and backslashes', () => {
    expect(parentDirectoryPath('C:/dir\\file.txt')).toBe('C:/dir');
  });

  it('returns empty when the only separator is at index 0', () => {
    expect(parentDirectoryPath('/segment')).toBe('');
  });

  it('returns empty for file at filesystem root (POSIX)', () => {
    expect(parentDirectoryPath('/file')).toBe('');
  });

  it('returns drive root for shallow Windows path on a letter drive', () => {
    expect(parentDirectoryPath('D:\\a.txt')).toBe('D:\\');
  });

  it('returns parent folder for Windows path with one directory level', () => {
    expect(parentDirectoryPath('C:\\Users\\x.txt')).toBe('C:\\Users');
  });

  it('returns UNC parent for //server/share/file style path', () => {
    expect(parentDirectoryPath('//server/share/file.txt')).toBe('//server/share');
  });
});

describe('normalizeFsPath', () => {
  it('treats Windows drive paths differing only by slash style as equal', () => {
    expect(normalizeFsPath('C:/Users/proj')).toBe(normalizeFsPath('c:\\users\\proj'));
  });

  it('lowerCases Windows-like paths for comparison', () => {
    expect(normalizeFsPath('D:\\Work')).toBe('d:\\work');
  });

  it('does not lowerCase POSIX absolute paths', () => {
    expect(normalizeFsPath('/Home/User')).toBe('/Home/User');
  });

  it('normalizes UNC with forward slashes', () => {
    expect(normalizeFsPath('//SERVER/Share/Folder')).toBe('\\\\server\\share\\folder');
  });
});

describe('joinFilePath', () => {
  it('uses backslash when dir uses backslashes', () => {
    expect(joinFilePath('C:\\proj\\src', 'main.c')).toBe('C:\\proj\\src\\main.c');
  });

  it('uses slash for posix-style dirs', () => {
    expect(joinFilePath('/home/u/proj', 'README')).toBe('/home/u/proj/README');
  });

  it('strips trailing separators on dir', () => {
    expect(joinFilePath('C:\\proj\\', 'out.txt')).toBe('C:\\proj\\out.txt');
  });
});
