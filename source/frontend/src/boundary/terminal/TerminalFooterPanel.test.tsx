import { act, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import type { TerminalOutputEvent } from '@domain/terminal/terminalSession';

const terminalBridge = vi.hoisted(() => ({
  listeners: [] as Array<(event: TerminalOutputEvent) => void>,
  unlisten: vi.fn(),
  listenTerminalOutput: vi.fn(),
  startTerminalSession: vi.fn(),
  stopTerminalSession: vi.fn(),
  writeTerminalInput: vi.fn(),
  interruptTerminalSession: vi.fn(),
}));

vi.mock('@infrastructure/tauri/bridge', () => ({
  listenTerminalOutput: terminalBridge.listenTerminalOutput,
  startTerminalSession: terminalBridge.startTerminalSession,
  stopTerminalSession: terminalBridge.stopTerminalSession,
  writeTerminalInput: terminalBridge.writeTerminalInput,
  interruptTerminalSession: terminalBridge.interruptTerminalSession,
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

    terminalBridge.listenTerminalOutput.mockImplementation(
      async (listener: (event: TerminalOutputEvent) => void) => {
        terminalBridge.listeners.push(listener);
        return terminalBridge.unlisten;
      },
    );
    terminalBridge.startTerminalSession.mockResolvedValue({
      sessionId: 'terminal-1',
      shell: 'powershell.exe',
      cwd: 'C:\\work',
      supportsInterrupt: false,
    });
    terminalBridge.stopTerminalSession.mockResolvedValue(undefined);
    terminalBridge.writeTerminalInput.mockResolvedValue(undefined);
    terminalBridge.interruptTerminalSession.mockRejectedValue('unsupported');
  });

  it('starts a session for the workspace root and stops it on unmount', async () => {
    const { unmount } = render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);

    await screen.findByText(/powershell\.exe - C:\\work/i);

    expect(terminalBridge.listenTerminalOutput).toHaveBeenCalledTimes(1);
    expect(terminalBridge.startTerminalSession).toHaveBeenCalledWith('C:\\work');

    unmount();

    expect(terminalBridge.unlisten).toHaveBeenCalledTimes(1);
    expect(terminalBridge.stopTerminalSession).toHaveBeenCalledWith('terminal-1');
  });

  it('renders streamed output events without polling process APIs', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByText(/powershell\.exe/i);

    act(() => {
      terminalBridge.listeners[0]?.({
        sessionId: 'terminal-1',
        stream: 'stdout',
        data: 'hello from shell\n',
      });
    });

    expect(screen.getByLabelText(/terminal output/i)).toHaveTextContent('hello from shell');
  });

  it('prefixes stderr output events in the terminal output', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByText(/powershell\.exe/i);

    act(() => {
      terminalBridge.listeners[0]?.({
        sessionId: 'terminal-1',
        stream: 'stderr',
        data: 'command failed\n',
      });
    });

    expect(screen.getByLabelText(/terminal output/i)).toHaveTextContent('[stderr] command failed');
  });

  it('marks the session stopped when the bridge emits an exit event', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByText(/powershell\.exe/i);

    act(() => {
      terminalBridge.listeners[0]?.({
        sessionId: 'terminal-1',
        stream: 'exit',
        data: 'Terminal exited (exit status: 0).\n',
      });
    });

    expect(screen.getByText(/terminal is not running/i)).toBeInTheDocument();
    expect(screen.getByLabelText(/terminal output/i)).toHaveTextContent('Terminal exited');
    expect(screen.getByLabelText(/terminal input/i)).toBeDisabled();
  });

  it('sends input with Enter through the terminal bridge', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByText(/powershell\.exe/i);

    fireEvent.change(screen.getByLabelText(/terminal input/i), {
      target: { value: 'dir' },
    });
    fireEvent.click(screen.getByRole('button', { name: /^enter$/i }));

    await waitFor(() => {
      expect(terminalBridge.writeTerminalInput).toHaveBeenCalledWith('terminal-1', 'dir\n');
    });
  });

  it('recalls local command history with arrow keys', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);
    await screen.findByText(/powershell\.exe/i);

    const input = screen.getByLabelText(/terminal input/i);

    fireEvent.change(input, { target: { value: 'dir' } });
    fireEvent.click(screen.getByRole('button', { name: /^enter$/i }));
    fireEvent.change(input, { target: { value: 'echo hi' } });
    fireEvent.click(screen.getByRole('button', { name: /^enter$/i }));

    await waitFor(() => {
      expect(terminalBridge.writeTerminalInput).toHaveBeenCalledWith('terminal-1', 'echo hi\n');
    });

    fireEvent.keyDown(input, { key: 'ArrowUp' });
    expect(input).toHaveValue('echo hi');

    fireEvent.keyDown(input, { key: 'ArrowUp' });
    expect(input).toHaveValue('dir');

    fireEvent.keyDown(input, { key: 'ArrowDown' });
    expect(input).toHaveValue('echo hi');

    fireEvent.keyDown(input, { key: 'ArrowDown' });
    expect(input).toHaveValue('');
  });

  it('surfaces the unsupported Ctrl+C state', async () => {
    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);

    await screen.findByRole('button', { name: /ctrl\+c unsupported/i });
    fireEvent.click(screen.getByRole('button', { name: /ctrl\+c unsupported/i }));

    expect(screen.getByLabelText(/terminal output/i)).toHaveTextContent('Ctrl+C is unsupported');
    expect(terminalBridge.interruptTerminalSession).not.toHaveBeenCalled();
  });

  it('does not start a session without a workspace root', () => {
    render(<TerminalFooterPanel workspaceRoot={null} />);

    expect(screen.getByText(/open a workspace folder/i)).toBeInTheDocument();
    expect(terminalBridge.startTerminalSession).not.toHaveBeenCalled();
  });

  it('announces start errors accessibly', async () => {
    terminalBridge.startTerminalSession.mockRejectedValueOnce(new Error('spawn failed'));

    render(<TerminalFooterPanel workspaceRoot={'C:\\work'} />);

    expect(await screen.findByRole('alert')).toHaveTextContent('spawn failed');
    expect(screen.getByRole('status')).toHaveTextContent('spawn failed');
    expect(screen.getByLabelText(/terminal input/i)).toBeDisabled();
  });
});
