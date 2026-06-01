export type TerminalOutputStream = "stdout" | "stderr" | "system" | "exit";

export interface TerminalSession {
  sessionId: string;
  shell: string;
  cwd: string;
  supportsInterrupt: boolean;
}

export interface TerminalOutputEvent {
  sessionId: string;
  stream: TerminalOutputStream;
  data: string;
}

export interface TerminalCapabilities {
  supportsInterrupt: boolean;
  reason: string;
}
