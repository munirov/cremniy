import { fileNameFromPath } from '@domain/workspace/paths';

/**
 * Map a file path to a Monaco language id, mirroring the Qt
 * `QCodeEditor::setFileExt` highlighting (see EXTENSION_TO_LANGUAGE below).
 * GLSL has no Monaco grammar, so it maps to C++; unknown -> plaintext.
 */
export function monacoLanguageForPath(path: string | null): string {
  if (path == null) {
    return 'plaintext';
  }
  const name = fileNameFromPath(path);
  const dot = name.lastIndexOf('.');
  if (dot <= 0 || dot === name.length - 1) {
    return 'plaintext';
  }
  const ext = name.slice(dot + 1).toLowerCase();
  return EXTENSION_TO_LANGUAGE[ext] ?? 'plaintext';
}

const EXTENSION_TO_LANGUAGE: Readonly<Record<string, string>> = {
  // C / C++ — Qt used the C++ highlighter for the whole family.
  c: 'c',
  h: 'cpp',
  hpp: 'cpp',
  hh: 'cpp',
  cpp: 'cpp',
  cc: 'cpp',
  cxx: 'cpp',
  // GLSL — no native Monaco grammar; C++ is the closest visual match.
  glsl: 'cpp',
  vert: 'cpp',
  frag: 'cpp',
  // Markup
  xml: 'xml',
  svg: 'xml',
  html: 'html',
  htm: 'html',
  // Data / scripting
  json: 'json',
  py: 'python',
  python: 'python',
  lua: 'lua',
};
