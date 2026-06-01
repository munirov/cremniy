import { describe, expect, it } from 'vitest';

import {
  createProjectIssueMessage,
  isValidProjectFolderName,
  validateCreateProjectDraft,
} from './createProjectValidation';

describe('createProjectValidation', () => {
  it('rejects empty name', () => {
    expect(validateCreateProjectDraft({ projectName: '', parentDirectoryPath: '/x' })).toEqual({
      field: 'name',
      code: 'nameEmpty',
    });
  });

  it('rejects invalid characters in name', () => {
    expect(validateCreateProjectDraft({ projectName: 'bad name', parentDirectoryPath: '/x' })).toEqual({
      field: 'name',
      code: 'nameInvalid',
    });
  });

  it('rejects missing parent path', () => {
    expect(validateCreateProjectDraft({ projectName: 'ok_name', parentDirectoryPath: '  ' })).toEqual({
      field: 'path',
      code: 'pathMissing',
    });
  });

  it('accepts valid draft', () => {
    expect(validateCreateProjectDraft({ projectName: 'MyProject_1', parentDirectoryPath: '/home/user' })).toBeNull();
  });

  it('isValidProjectFolderName matches Qt validator', () => {
    expect(isValidProjectFolderName('abc')).toBe(true);
    expect(isValidProjectFolderName('A-z_09')).toBe(true);
    expect(isValidProjectFolderName('a b')).toBe(false);
    expect(isValidProjectFolderName('x/y')).toBe(false);
  });

  it('maps issues to user-facing strings', () => {
    expect(createProjectIssueMessage({ field: 'name', code: 'nameEmpty' })).toMatch(/enter project name/i);
  });
});
