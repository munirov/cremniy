import { describe, expect, it } from 'vitest';

import {
  processSucceeded,
  summarizeProcessResult,
  type WorkspaceProcessResult,
} from './workspaceProcess';

function make(partial: Partial<WorkspaceProcessResult>): WorkspaceProcessResult {
  return {
    program: 'rustc',
    args: [],
    cwd: '/ws',
    stdout: '',
    stderr: '',
    statusCode: 0,
    timedOut: false,
    durationMs: 12,
    ...partial,
  };
}

describe('workspaceProcess', () => {
  it('treats exit 0 without timeout as success', () => {
    expect(processSucceeded(make({ statusCode: 0 }))).toBe(true);
  });

  it('treats non-zero exit as failure', () => {
    expect(processSucceeded(make({ statusCode: 1 }))).toBe(false);
  });

  it('treats a timeout as failure even with exit 0', () => {
    expect(processSucceeded(make({ statusCode: 0, timedOut: true }))).toBe(false);
  });

  it('summarizes each terminal state distinctly', () => {
    expect(summarizeProcessResult(make({ statusCode: 0, durationMs: 5 }))).toBe('OK (exit 0, 5 ms)');
    expect(summarizeProcessResult(make({ statusCode: 2, durationMs: 5 }))).toBe('Exit 2 (5 ms)');
    expect(summarizeProcessResult(make({ statusCode: null, durationMs: 5 }))).toBe(
      'Terminated without an exit code (5 ms)',
    );
    expect(summarizeProcessResult(make({ timedOut: true, durationMs: 1000 }))).toBe(
      'Timed out after 1000 ms',
    );
  });
});
