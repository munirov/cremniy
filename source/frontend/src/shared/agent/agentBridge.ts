// Scriptable control layer (`window.cremniy`): registry + API.
// Docs: documentation/EN/agent_control_surface.md

export type AgentArgs = Record<string, unknown>;

export type AgentCommand = {
  /** Stable, namespaced command id, e.g. `file.save`, `session.openFile`. */
  name: string;
  /** One-line human/agent-readable description, including expected args. */
  description: string;
  /** Invoked with the caller args; may return a value or a promise. */
  run: (args: AgentArgs) => unknown;
};

export type AgentStateProducer = () => unknown;

export type AgentCommandInfo = {
  name: string;
  description: string;
};

const commandRegistry = new Map<string, AgentCommand>();
const stateRegistry = new Map<string, AgentStateProducer>();

/** Register one or more commands; returns an unregister cleanup for effects. */
export function registerAgentCommands(commands: readonly AgentCommand[]): () => void {
  for (const command of commands) {
    commandRegistry.set(command.name, command);
  }
  return () => {
    for (const command of commands) {
      if (commandRegistry.get(command.name) === command) {
        commandRegistry.delete(command.name);
      }
    }
  };
}

/** Register a named slice of on-screen state; returns an unregister cleanup. */
export function registerAgentState(key: string, producer: AgentStateProducer): () => void {
  stateRegistry.set(key, producer);
  return () => {
    if (stateRegistry.get(key) === producer) {
      stateRegistry.delete(key);
    }
  };
}

/** Currently available commands on the mounted screen. */
export function listAgentCommands(): AgentCommandInfo[] {
  return [...commandRegistry.values()]
    .map(({ name, description }) => ({ name, description }))
    .sort((a, b) => a.name.localeCompare(b.name));
}

/** Snapshot every registered state slice. */
export function agentStateSnapshot(): Record<string, unknown> {
  const snapshot: Record<string, unknown> = {};
  for (const [key, produce] of stateRegistry) {
    try {
      snapshot[key] = produce();
    } catch (error) {
      snapshot[key] = { error: error instanceof Error ? error.message : String(error) };
    }
  }
  return snapshot;
}

/** Run a command by name. Rejects on unknown command; resolves to its result. */
export async function runAgentCommand(name: string, args: AgentArgs = {}): Promise<unknown> {
  const command = commandRegistry.get(name);
  if (command == null) {
    throw new Error(
      `Unknown agent command: ${name}. Call cremniy.commands() to list available commands.`,
    );
  }
  return command.run(args);
}

export const AGENT_BRIDGE_VERSION = 1;

export type AgentBridgeApi = {
  version: number;
  /** Commands available on the current screen. */
  commands: () => AgentCommandInfo[];
  /** Structured snapshot of the current on-screen state. */
  state: () => Record<string, unknown>;
  /** Invoke a command by name. */
  run: (name: string, args?: AgentArgs) => Promise<unknown>;
};

export function createAgentBridgeApi(): AgentBridgeApi {
  return {
    version: AGENT_BRIDGE_VERSION,
    commands: listAgentCommands,
    state: agentStateSnapshot,
    run: (name, args) => runAgentCommand(name, args ?? {}),
  };
}

declare global {
  interface Window {
    cremniy?: AgentBridgeApi;
  }
}

/** Install `window.cremniy` (call once at startup). */
export function installAgentBridge(target: Window = window): void {
  target.cremniy = createAgentBridgeApi();
}

/** Test-only: clear all registrations. */
export function resetAgentBridgeForTests(): void {
  commandRegistry.clear();
  stateRegistry.clear();
}
