import { describe, expect, it } from 'vitest';

import { monacoLanguageForPath } from './editorLanguage';

describe('monacoLanguageForPath', () => {
  it('maps the Qt C/C++ family to C/C++ highlighting', () => {
    expect(monacoLanguageForPath('/w/main.c')).toBe('c');
    expect(monacoLanguageForPath('/w/main.cpp')).toBe('cpp');
    expect(monacoLanguageForPath('/w/util.hpp')).toBe('cpp');
    expect(monacoLanguageForPath('C:\\w\\a.cc')).toBe('cpp');
    expect(monacoLanguageForPath('/w/api.h')).toBe('cpp');
  });

  it('maps the remaining Qt-supported types', () => {
    expect(monacoLanguageForPath('/w/shader.glsl')).toBe('cpp');
    expect(monacoLanguageForPath('/w/data.xml')).toBe('xml');
    expect(monacoLanguageForPath('/w/page.html')).toBe('html');
    expect(monacoLanguageForPath('/w/conf.json')).toBe('json');
    expect(monacoLanguageForPath('/w/script.py')).toBe('python');
    expect(monacoLanguageForPath('/w/mod.lua')).toBe('lua');
  });

  it('is case-insensitive on the extension', () => {
    expect(monacoLanguageForPath('/w/MAIN.CPP')).toBe('cpp');
    expect(monacoLanguageForPath('/w/Conf.JSON')).toBe('json');
  });

  it('falls back to plaintext for unknown or extension-less files', () => {
    expect(monacoLanguageForPath('/w/notes.txt')).toBe('plaintext');
    expect(monacoLanguageForPath('/w/Makefile')).toBe('plaintext');
    expect(monacoLanguageForPath('/w/.gitignore')).toBe('plaintext');
    expect(monacoLanguageForPath('/w/trailing.')).toBe('plaintext');
    expect(monacoLanguageForPath(null)).toBe('plaintext');
  });
});
