import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

const { mockConnList, mockConnSave, mockConnDelete, mockSerialPorts, mockOpenConnection } =
  vi.hoisted(() => ({
    mockConnList: vi.fn(),
    mockConnSave: vi.fn(),
    mockConnDelete: vi.fn(),
    mockSerialPorts: vi.fn(),
    mockOpenConnection: vi.fn(),
  }));

vi.mock('@infrastructure/tauri/bridge', () => ({
  connList: mockConnList,
  connSave: mockConnSave,
  connDelete: mockConnDelete,
  serialPorts: mockSerialPorts,
}));

vi.mock('@shared/connections/connectionBus', () => ({
  openConnection: mockOpenConnection,
}));

import { ConnectionsPanel } from './ConnectionsPanel';

describe('ConnectionsPanel', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    mockConnList.mockResolvedValue([]);
    mockSerialPorts.mockResolvedValue([{ name: 'COM3' }, { name: 'COM7' }]);
    mockConnSave.mockResolvedValue(undefined);
    mockConnDelete.mockResolvedValue(undefined);
  });

  it('shows an empty state when there are no saved hosts', async () => {
    render(<ConnectionsPanel />);
    expect(await screen.findByText(/no saved connections/i)).toBeInTheDocument();
  });

  it('creates a serial host: New serial → fill → Save calls connSave', async () => {
    render(<ConnectionsPanel />);
    await screen.findByText(/no saved connections/i);

    fireEvent.click(screen.getByRole('button', { name: /new serial/i }));
    // Form is shown.
    expect(await screen.findByText(/new connection/i)).toBeInTheDocument();

    fireEvent.change(screen.getByPlaceholderText(/STM32/i), { target: { value: 'MCU' } });
    fireEvent.change(screen.getByPlaceholderText(/COM3 or/i), { target: { value: 'COM3' } });
    fireEvent.click(screen.getByRole('button', { name: /^save$/i }));

    await waitFor(() => expect(mockConnSave).toHaveBeenCalledTimes(1));
    const saved = mockConnSave.mock.calls[0][0];
    expect(saved).toMatchObject({ label: 'MCU', kind: 'serial', serial: { port: 'COM3' } });
    expect(saved.id).toBeTruthy(); // a fresh uuid was minted
  });

  it('connects a saved serial host through the connection bus', async () => {
    mockConnList.mockResolvedValue([
      { id: 'c1', label: 'Board', kind: 'serial', tags: [], serial: { port: 'COM7', baud: 115200 } },
    ]);
    render(<ConnectionsPanel />);

    fireEvent.click(await screen.findByRole('button', { name: /^connect$/i }));
    expect(mockOpenConnection).toHaveBeenCalledWith({
      connId: 'c1',
      label: 'Board',
      serial: { port: 'COM7', baud: 115200 },
    });
  });

  it('connects a saved SSH host through the connection bus', async () => {
    mockConnList.mockResolvedValue([
      {
        id: 's1',
        label: 'prod',
        kind: 'ssh',
        tags: [],
        ssh: { address: '10.0.0.1', port: 22, username: 'root', password: 'hunter2' },
      },
    ]);
    render(<ConnectionsPanel />);

    const connect = await screen.findByRole('button', { name: /^connect$/i });
    expect(connect).not.toBeDisabled();

    fireEvent.click(connect);
    expect(mockOpenConnection).toHaveBeenCalledWith({
      connId: 's1',
      label: 'prod',
      ssh: { address: '10.0.0.1', port: 22, username: 'root', password: 'hunter2' },
    });
  });
});
