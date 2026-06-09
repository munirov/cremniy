/** Last path segment (file or folder name), supporting `/` and `\\`. */
export function fileNameFromPath(filePath: string): string {
  const t = filePath.trim();
  if (t === '') {
    return '';
  }
  const lastBack = t.lastIndexOf('\\');
  const lastFwd = t.lastIndexOf('/');
  const last = Math.max(lastBack, lastFwd);
  return last >= 0 ? t.slice(last + 1) : t;
}

/** Normalize path for stable equality checks (Windows drive/UNC slash + case). */
export function normalizeFsPath(p: string): string {
  const t = p.trim();
  if (t === '') {
    return '';
  }
  const winLike =
    /^[A-Za-z]:/.test(t) || t.startsWith('\\\\') || /^\/\/[^/]+/.test(t);
  if (winLike) {
    return t.replace(/\//g, '\\').toLowerCase();
  }
  return t;
}

/** Parent directory of a filesystem path (supports `/` and `\\`). */
export function parentDirectoryPath(filePath: string): string {
  const t = filePath.trim();
  if (t === '') {
    return '';
  }
  const lastBack = t.lastIndexOf('\\');
  const lastFwd = t.lastIndexOf('/');
  const last = Math.max(lastBack, lastFwd);
  if (last <= 0) {
    return '';
  }
  let parent = t.slice(0, last);
  if (/^[A-Za-z]:$/.test(parent)) {
    parent += '\\';
  }
  return parent;
}

/**
 * Breadcrumb segments for a file relative to the workspace root, e.g.
 * `C:\…\test-c\src\main.c` under root `C:\…\test-c` → `['test-c', 'src', 'main.c']`
 * (root folder name first, Cursor-style). A file outside the root collapses to
 * just its name so no absolute path leaks into the UI.
 */
export function workspaceBreadcrumb(filePath: string, rootPath: string | null): string[] {
  const stripUnc = (p: string) => p.replace(/^\\\\\?\\/, '').replace(/^\/\/\?\//, '');
  const file = stripUnc(filePath.trim());
  const fileSegs = file.split(/[/\\]+/).filter((s) => s !== '');
  const root = rootPath != null ? stripUnc(rootPath.trim()) : '';
  if (root !== '') {
    const rootSegs = root.split(/[/\\]+/).filter((s) => s !== '');
    const underRoot =
      fileSegs.length >= rootSegs.length &&
      rootSegs.every((seg, i) => (fileSegs[i] ?? '').toLowerCase() === seg.toLowerCase());
    if (underRoot) {
      const rootName = rootSegs[rootSegs.length - 1] ?? '';
      return [rootName, ...fileSegs.slice(rootSegs.length)].filter((s) => s !== '');
    }
  }
  return fileSegs.length > 0 ? [fileSegs[fileSegs.length - 1]!] : [];
}

/** Join directory + file name using the dominant separator style of `dirPath`. */
export function joinFilePath(dirPath: string, fileName: string): string {
  const d = dirPath.trim().replace(/[/\\]+$/, '');
  const name = fileName.trim();
  if (d === '') {
    return name;
  }
  const sep = d.includes('\\') && !d.includes('/') ? '\\' : '/';
  return `${d}${sep}${name}`;
}

/**
 * Resolve a relative reference (Markdown/href style — `/`-separated, may use
 * `./` and `../`) against a base directory, producing an absolute path in the
 * base's separator style. Used to turn a `.md` file's relative image/link
 * targets into real workspace paths. Returns the ref unchanged if either side is
 * empty.
 */
export function resolveRelativePath(baseDir: string, ref: string): string {
  const r = ref.trim();
  const base = baseDir.trim().replace(/[/\\]+$/, '');
  if (r === '' || base === '') {
    return r;
  }
  const sep = base.includes('\\') && !base.includes('/') ? '\\' : '/';
  const parts = base.split(/[/\\]+/);
  for (const seg of r.split(/[/\\]+/)) {
    if (seg === '' || seg === '.') {
      continue;
    }
    if (seg === '..') {
      if (parts.length > 1) {
        parts.pop();
      }
    } else {
      parts.push(seg);
    }
  }
  return parts.join(sep);
}
