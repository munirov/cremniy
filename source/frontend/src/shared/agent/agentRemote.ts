// Backend ↔ webview bridge for the in-app MCP server (source/backend/src/mcp.rs).
// The backend emits `agent://request`; we run it against the local
// `window.cremniy` registry and reply via the `agent_reply` command.
// Docs: documentation/architecture/AGENT_CONTROL.md

import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';

import { agentStateSnapshot, listAgentCommands, runAgentCommand } from './agentBridge';

type AgentRequest = {
  id: number;
  /** "commands" | "state" | "run" */
  kind: string;
  /** command name for "run" */
  name: string;
  args: Record<string, unknown>;
};

async function reply(id: number, ok: boolean, json: string): Promise<void> {
  try {
    await invoke('agent_reply', { id, ok, json });
  } catch {
    // Backend gone / not Tauri — nothing to resolve.
  }
}

/**
 * Subscribe to bridged MCP requests. No-op (resolves) outside Tauri, where
 * `listen` rejects. Call once at startup, after installAgentBridge().
 */
export async function installAgentRemote(): Promise<void> {
  await listen<AgentRequest>('agent://request', async (event) => {
    const { id, kind, name, args } = event.payload;
    try {
      let result: unknown = null;
      if (kind === 'commands') {
        result = listAgentCommands();
      } else if (kind === 'state') {
        result = agentStateSnapshot();
      } else if (kind === 'run') {
        result = await runAgentCommand(name, args ?? {});
      } else {
        throw new Error(`unknown bridge kind: ${kind}`);
      }
      await reply(id, true, JSON.stringify(result ?? null));
    } catch (e) {
      await reply(id, false, e instanceof Error ? e.message : String(e));
    }
  });
}
