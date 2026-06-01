import { afterEach, describe, expect, it } from 'vitest';

import {
  AGENT_BRIDGE_VERSION,
  agentStateSnapshot,
  createAgentBridgeApi,
  installAgentBridge,
  listAgentCommands,
  registerAgentCommands,
  registerAgentState,
  resetAgentBridgeForTests,
  runAgentCommand,
} from './agentBridge';

afterEach(() => {
  resetAgentBridgeForTests();
});

describe('agentBridge', () => {
  it('lists registered commands sorted by name', () => {
    registerAgentCommands([
      { name: 'file.save', description: 'Save', run: () => undefined },
      { name: 'edit.find', description: 'Find', run: () => undefined },
    ]);

    expect(listAgentCommands()).toEqual([
      { name: 'edit.find', description: 'Find' },
      { name: 'file.save', description: 'Save' },
    ]);
  });

  it('runs a command with args and returns its result', async () => {
    registerAgentCommands([
      { name: 'session.openFile', description: 'Open', run: (args) => `opened:${String(args.path)}` },
    ]);

    await expect(runAgentCommand('session.openFile', { path: '/a/b.c' })).resolves.toBe(
      'opened:/a/b.c',
    );
  });

  it('rejects unknown commands with a discoverable hint', async () => {
    await expect(runAgentCommand('does.not.exist')).rejects.toThrow(/cremniy\.commands\(\)/);
  });

  it('snapshots registered state slices and isolates producer errors', () => {
    registerAgentState('session', () => ({ activeFilePath: '/x' }));
    registerAgentState('broken', () => {
      throw new Error('boom');
    });

    expect(agentStateSnapshot()).toEqual({
      session: { activeFilePath: '/x' },
      broken: { error: 'boom' },
    });
  });

  it('unregister cleanup removes only its own registrations', () => {
    const unregister = registerAgentCommands([
      { name: 'tmp.cmd', description: 'tmp', run: () => undefined },
    ]);
    registerAgentCommands([{ name: 'keep.cmd', description: 'keep', run: () => undefined }]);

    unregister();

    expect(listAgentCommands().map((c) => c.name)).toEqual(['keep.cmd']);
  });

  it('installs a versioned api on the target global', () => {
    const target = {} as Window;
    installAgentBridge(target);

    expect(target.cremniy?.version).toBe(AGENT_BRIDGE_VERSION);
    expect(typeof target.cremniy?.run).toBe('function');
  });

  it('exposes commands/state/run through the api object', async () => {
    registerAgentCommands([{ name: 'a.b', description: 'AB', run: () => 42 }]);
    registerAgentState('ui', () => ({ route: 'ide' }));
    const api = createAgentBridgeApi();

    expect(api.commands()).toEqual([{ name: 'a.b', description: 'AB' }]);
    expect(api.state()).toEqual({ ui: { route: 'ide' } });
    await expect(api.run('a.b')).resolves.toBe(42);
  });
});
