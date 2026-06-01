const PROJECT_NAME_PATTERN = /^[A-Za-z0-9_-]+$/;

export type CreateProjectFieldError = 'nameEmpty' | 'nameInvalid' | 'pathMissing' | 'pathNotDirectory' | 'targetExists' | 'createFailed';

export type CreateProjectValidationIssue = {
  field: 'name' | 'path' | 'general';
  code: CreateProjectFieldError;
};

export function isValidProjectFolderName(name: string): boolean {
  const t = name.trim();
  return t !== '' && PROJECT_NAME_PATTERN.test(t);
}

export function validateCreateProjectDraft(input: {
  projectName: string;
  parentDirectoryPath: string;
}): CreateProjectValidationIssue | null {
  const name = input.projectName.trim();
  if (name === '') {
    return { field: 'name', code: 'nameEmpty' };
  }
  if (!isValidProjectFolderName(name)) {
    return { field: 'name', code: 'nameInvalid' };
  }
  const parent = input.parentDirectoryPath.trim();
  if (parent === '') {
    return { field: 'path', code: 'pathMissing' };
  }
  return null;
}

export function createProjectIssueMessage(issue: CreateProjectValidationIssue): string {
  switch (issue.code) {
    case 'nameEmpty':
      return 'Please enter project name';
    case 'nameInvalid':
      return 'Project name may only contain letters, digits, underscore, and hyphen';
    case 'pathMissing':
      return 'Choose a parent directory';
    case 'pathNotDirectory':
      return 'Directory is invalid';
    case 'targetExists':
      return 'Directory already exists';
    case 'createFailed':
      return 'Failed to create project directory';
  }
}
