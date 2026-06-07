import { act, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import type { TerminalOutputEvent } from '@domain/terminal/terminalSession';
import { resetAgentBridgeForTests } from '@shared/agent/agentBridge';

// The panel mounts real xterm instances (one per tab). xterm paints to a
// <canvas>, which jsdom can't read back — so the scrollback text never lands in
// the DOM and can't be asserted with toHaveTextContent. We therefore verify the
// panel through the surfaces that ARE observable in jsdom: the mocked bridge
// (start/stop/listen/input wiring) and the tab strip / floating status it
// renders. The PTY bridge is fully mocked so nothing touches Tauri.
const terminalBridge = vi.hoisted(() => ({
  listeners: [] as Array<(event: TerminalOutputEvent) => void>,
  unlisten: vi.fn(),
  listenTerminalOutput: vi.fn(),
  startTerminalSession: vi.fn(),
  stopTerminalSession: vi.fn(),
  writeTerminalInput: vi.fn(),
  interruptTerminalSession: vi.fn(),
  resizeTerminalSession: vi.fn(),
  readCremniyMeta: vi.fn(),
  writeCremniyMeta: vi.fn(),
}));

vi.mock('@infrastructure/tauri/bridge', () => ({
  listenTerminalOutput: terminalBridge.listenTerminalOutput,
  startTerminalSession: terminalBridge.startTerminalSession,
  stopTerminalSession: terminalBridge.stopTerminalSession,
  writeTerminalInput: terminalBridge.writeTerminalInput,
  interruptTerminalSession: terminalBridge.interruptTerminalSession,
  resizeTerminalSession: terminalBridge.resizeTerminalSession,
  // The panel restores/persists its tab layout from .cremniy on mount. Default
  // to "no file yet" (readCremniyMeta rejects) so the panel falls back to one
  // fresh tab; writes are no-ops.
  readCremniyMeta: terminalBridge.readCremniyMeta,
  writeCremniyMeta: terminalBridge.writeCremniyMeta,
}));

import { TerminalFooterPanel } from './TerminalFooterPanel';

describe('TerminalFooterPanel', () => {
  beforeEach(() => {
    terminalBridge.listeners = [];
    terminalBridge.unlisten.mockReset();
    terminalBridge.listenTerminalOutput.mockReset();
    terminalBridge.startTerminalSession.mockReset();
    terminalBridge.stopTerminalSession.mockReset();
    terminalBridge.writeTerminalInput.mockReset();
    terminalBridge.interruptTerminalSession.mockReset();
    terminalBridge.resizeTerminalSession.mockReset();
    terminalBridge.readCremniyMeta.mockReset();
    terminalBridge.writeCremniyMeta.mockReset();

    terminalBridge.listenTerminalOutput.mockImplementation(
      async (listener: (event: TerminalOutputEvent) => void) => {
        terminalBridge.listeners.push(listener);
        return terminalBridge.unlisten;
      },
    );
    terminalBridge.startTerminalSession.mockResolvedValue({
      sessionId: 'terminal-1',
      shell: 'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe',
      cwd: 'C:\\work',
      supportsInterrupt: false,
    });
    terminalBridge.stopTerminalSession.mockResolvedValue(undefined);
    terminalBridge.writeTerminalInput.mockResolvedValue(undefined);
    terminalBridge.interruptTerminalSession.mockRejectedValue('unsupported');
    terminalBridge.resizeTerminalSession.mockResolvedValue(undefined);
    // No saved .cremniy by default → the panel opens with a single fresh tab.
    terminalBridge.readCremniyMeta.mockRejectedValue(new Error('not found'));
    terminalBridge.writeCremniyMeta.mockResolvedValue(undefined);
  });

  afterEach(() => {
    // The panel registers `terminal.read` / `terminal.list` agent commands on
    // mount; clear the shared registry between tests so they don't accumulate.
    resetAgentBridgeForTests();
  });

  it('starts a session for the workspace root and stops it on unmount', async () => {
    const { unmount } = render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);

    // The active tab labels itself from the live shell: the shell path is
    // reduced to a friendly basename (…\powershell.exe → "powershell").
    await screen.findByRole('tab', { name: /powershell/i });

    expect(terminalBridge.listenTerminalOutput).toHaveBeenCalledTimes(1);
    expect(terminalBridge.startTerminalSession).toHaveBeenCalledWith('C:\\work');

    unmount();

    expect(terminalBridge.unlisten).toHaveBeenCalledTimes(1);
    expect(terminalBridge.stopTerminalSession).toHaveBeenCalledWith('terminal-1');
  });

  it('subscribes once and routes streamed stdout events to the live terminal', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByRole('tab', { name: /powershell/i });

    expect(terminalBridge.listenTerminalOutput).toHaveBeenCalledTimes(1);

    // Delivering output drives the xterm write path (canvas, so the text isn't
    // DOM-readable in jsdom). Assert it's handled without throwing and the
    // session stays running — i.e. no error/exit status surfaces.
    act(() => {
      terminalBridge.listeners[0]?.({
        sessionId: 'terminal-1',
        stream: 'stdout',
        data: 'hello from shell\n',
      });
    });

    expect(screen.queryByRole('status')).not.toBeInTheDocument();
    expect(screen.getByLabelText(/terminal output/i)).toBeInTheDocument();
  });

  it('handles stderr output events on the same write path as stdout', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByRole('tab', { name: /powershell/i });

    // stderr is no longer prefixed/styled differently by the panel — it goes to
    // the same terminal write path. Just confirm it's accepted without error and
    // the session keeps running.
    act(() => {
      terminalBridge.listeners[0]?.({
        sessionId: 'terminal-1',
        stream: 'stderr',
        data: 'command failed\n',
      });
    });

    expect(screen.queryByRole('status')).not.toBeInTheDocument();
  });

  it('reverts the tab label when the bridge emits an exit event', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByRole('tab', { name: /powershell/i });

    // On exit the instance reports a null shell, so the tab falls back to its
    // positional auto label — the DOM-observable signal that the session ended.
    act(() => {
      terminalBridge.listeners[0]?.({
        sessionId: 'terminal-1',
        stream: 'exit',
        data: 'Terminal exited (exit status: 0).\n',
      });
    });

    await screen.findByRole('tab', { name: /terminal 1/i });
    expect(screen.queryByRole('tab', { name: /powershell/i })).not.toBeInTheDocument();
  });

  it('opens an additional terminal tab when the new-terminal signal bumps', async () => {
    const { rerender } = render(
      <TerminalFooterPanel workspaceRoot={'C:\\work'} newTerminalSignal={0} />,
    );
    await screen.findByRole('tab', { name: /powershell/i });
    expect(screen.getAllByRole('tab')).toHaveLength(1);

    // Each bump of the signal spawns another tab (Terminal → New Terminal).
    rerender(<TerminalFooterPanel workspaceRoot={'C:\\work'} newTerminalSignal={1} />);

    await waitFor(() => {
      expect(screen.getAllByRole('tab').length).toBeGreaterThanOrEqual(2);
    });
    // The freshest tab starts before its shell reports, so it shows the auto
    // label; a second start_terminal_session is spawned for it.
    expect(terminalBridge.startTerminalSession).toHaveBeenCalledTimes(2);
  });

  it('renames a tab on double-click and commits with Enter', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    const tab = await screen.findByRole('tab', { name: /powershell/i });

    // Double-click the label to enter inline-rename mode, type a new name, Enter
    // to commit. (The rename <input> shares xterm's "Terminal input" textarea
    // label space, so target it via its current display value instead.)
    fireEvent.doubleClick(within(tab).getByText(/powershell/i));

    const input = await within(tab).findByDisplayValue(/powershell/i);
    fireEvent.change(input, { target: { value: 'build' } });
    fireEvent.keyDown(input, { key: 'Enter' });

    await screen.findByRole('tab', { name: /build/i });
  });

  it('does not auto-interrupt the session on start (Ctrl+C never sends SIGINT)', async () => {
    // Ctrl+C is copy-only now (interrupt lives on Ctrl+Break / the context
    // menu), and the panel never fires an interrupt on its own. There is no
    // "Ctrl+C unsupported" affordance to click anymore.
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByRole('tab', { name: /powershell/i });

    expect(
      screen.queryByRole('button', { name: /ctrl\+c unsupported/i }),
    ).not.toBeInTheDocument();
    expect(terminalBridge.interruptTerminalSession).not.toHaveBeenCalled();
  });

  it('does not start a session without a workspace root', async () => {
    render(<TerminalFooterPanel workspaceRoot={null} />);

    expect(
      await screen.findByText(/open a workspace folder to start a terminal session/i),
    ).toBeInTheDocument();
    expect(terminalBridge.startTerminalSession).not.toHaveBeenCalled();
  });

  it('surfaces start errors through the live status region', async () => {
    terminalBridge.startTerminalSession.mockRejectedValueOnce(new Error('spawn failed'));

    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);

    // The failure is announced via the polite status region (role="status"),
    // not a role="alert"; the error message is the status text.
    const status = await screen.findByRole('status');
    expect(status).toHaveTextContent('spawn failed');
  });
});
