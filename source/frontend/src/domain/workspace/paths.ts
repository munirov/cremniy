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
