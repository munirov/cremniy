/** Validates a single path segment for create/rename under a workspace tree. */
export function validateWorkspaceEntryName(name: string): string | null {
  const t = name.trim();
  if (t === '') {
    return 'Name must not be empty';
  }
  if (t === '.' || t === '..') {
    return 'Invalid name';
  }
  if (/[/\\]/.test(t)) {
    return 'Name must not contain path separators';
  }
  return null;
}
