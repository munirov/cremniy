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
  // `dot === 0` is a dotfile (.cremniy, .env, .gitignore) — use the part after
  // the leading dot as the "extension" so known ones still get highlighted.
  if (dot < 0 || dot === name.length - 1) {
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
  // Assembly
  s: 'mips',
  asm: 'mips',
  // Rust / Go / other systems
  rs: 'rust',
  go: 'go',
  // JVM / .NET / scripting
  java: 'java',
  kt: 'kotlin',
  kts: 'kotlin',
  scala: 'scala',
  cs: 'csharp',
  fs: 'fsharp',
  vb: 'vb',
  // Shell / dotfiles
  sh: 'shell',
  bash: 'shell',
  zsh: 'shell',
  ps1: 'powershell',
  bat: 'bat',
  cmd: 'bat',
  // Web
  js: 'javascript',
  jsx: 'javascript',
  ts: 'typescript',
  tsx: 'typescript',
  css: 'css',
  scss: 'scss',
  less: 'less',
  html: 'html',
  htm: 'html',
  php: 'php',
  // Markup
  xml: 'xml',
  svg: 'xml',
  xaml: 'xml',
  sln: 'plaintext',
  // Data / config
  json: 'json',
  cremniy: 'json', // our .cremniy project file is JSON
  yaml: 'yaml',
  yml: 'yaml',
  toml: 'ini',
  ini: 'ini',
  env: 'ini',
  // Docs / build
  md: 'markdown',
  markdown: 'markdown',
  mk: 'makefile',
  makefile: 'makefile',
  cmake: 'plaintext',
  // SQL / data languages
  sql: 'sql',
  // Scripting
  py: 'python',
  python: 'python',
  lua: 'lua',
  rb: 'ruby',
  pl: 'perl',
};
