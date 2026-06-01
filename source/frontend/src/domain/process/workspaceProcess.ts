/** Result of a one-shot workspace command (build/run/test), from the Rust runner. */
export type WorkspaceProcessResult = {
  program: string;
  args: string[];
  cwd: string;
  stdout: string;
  stderr: string;
  /** Process exit code, or null if it was killed before reporting one. */
  statusCode: number | null;
  /** True when the wall-clock timeout fired and the process tree was killed. */
  timedOut: boolean;
  durationMs: number;
};

export type WorkspaceProcessOptions = {
  args?: string[];
  /** Directory to run in, relative to the workspace root (must stay inside it). */
  relativeCwd?: string | null;
  timeoutMs?: number | null;
};

/** Whether a finished command counts as success (exit 0, not timed out). */
export function processSucceeded(result: WorkspaceProcessResult): boolean {
  return !result.timedOut && result.statusCode === 0;
}

/** A one-line human/agent-readable summary of how a command finished. */
export function summarizeProcessResult(result: WorkspaceProcessResult): string {
  if (result.timedOut) {
    return `Timed out after ${result.durationMs} ms`;
  }
  if (result.statusCode === 0) {
    return `OK (exit 0, ${result.durationMs} ms)`;
  }
  if (result.statusCode == null) {
    return `Terminated without an exit code (${result.durationMs} ms)`;
  }
  return `Exit ${result.statusCode} (${result.durationMs} ms)`;
}
