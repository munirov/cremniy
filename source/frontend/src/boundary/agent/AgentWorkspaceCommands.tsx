import { useEffect, useRef } from 'react';

import { processSucceeded, summarizeProcessResult } from '@domain/process/workspaceProcess';
import { joinFilePath } from '@domain/workspace/paths';
import {
  createDirectoryUnderWorkspace,
  createEmptyFileUnderWorkspace,
  deleteUnderWorkspace,
  listDirectoryEntries,
  readWorkspaceFileBytes,
  readWorkspaceUserFile,
  renameUnderWorkspace,
  runWorkspaceCommand,
  writeWorkspaceFileBytes,
} from '@infrastructure/tauri/bridge';
import { registerAgentCommands } from '@shared/agent/agentBridge';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

type RunResult = Awaited<ReturnType<typeof runWorkspaceCommand>>;

/**
 * Registers the `fs.*` / `process.*` commands. Mounted in the IDE shell to share
 * its workspace/session context; renders nothing. Args are explicit (no prompts).
 * Docs: documentation/architecture/AGENT_CONTROL.md
 */
export function AgentWorkspaceCommands() {
  const workspaceRoot = useWorkspaceRoot();
  const ide = useIdeSession();

  const ctxRef = useRef({ workspaceRoot, ide });
  useEffect(() => {
    ctxRef.current = { workspaceRoot, ide };
  });

  useEffect(() => {
    const requireRoot = (): string => {
      const root = ctxRef.current.workspaceRoot?.path?.trim() ?? '';
      if (root === '') {
        throw new Error('No workspace is open. Open a folder first (file.openFolder).');
      }
      return root;
    };

    const requireString = (value: unknown, field: string): string => {
      if (typeof value !== 'string' || value.trim() === '') {
        throw new Error(`${field} is required (non-empty string).`);
      }
      return value;
    };

    const toAbsoluteUnderRoot = (root: string, pathArg: unknown): string => {
      const p = requireString(pathArg, 'path');
      // Accept either an absolute path or one relative to the workspace root.
      const looksAbsolute = /^([a-zA-Z]:[\\/]|[\\/])/.test(p);
      return looksAbsolute ? p : joinFilePath(root, p);
    };

    const decodeBytes = (value: unknown): Uint8Array => {
      if (Array.isArray(value)) {
        return Uint8Array.from(value.map((n) => Number(n) & 0xff));
      }
      if (typeof value === 'string') {
        // hex string, e.g. "deadbeef" or "de ad be ef"
        const clean = value.replace(/\s+/g, '');
        if (clean.length % 2 !== 0 || /[^0-9a-fA-F]/.test(clean)) {
          throw new Error('bytes string must be hex (even length, 0-9a-f).');
        }
        const out = new Uint8Array(clean.length / 2);
        for (let i = 0; i < out.length; i += 1) {
          out[i] = Number.parseInt(clean.slice(i * 2, i * 2 + 2), 16);
        }
        return out;
      }
      throw new Error('bytes must be a number[] or a hex string.');
    };

    const summarizeRun = (result: RunResult) => ({
      summary: summarizeProcessResult(result),
      ok: processSucceeded(result),
      statusCode: result.statusCode,
      timedOut: result.timedOut,
      durationMs: result.durationMs,
      stdout: result.stdout,
      stderr: result.stderr,
    });

    const unregister = registerAgentCommands([
      {
        name: 'fs.list',
        description: 'List directory entries { path? } (defaults to workspace root).',
        run: (args) => {
          const root = requireRoot();
          const dir = args.path == null ? root : toAbsoluteUnderRoot(root, args.path);
          return listDirectoryEntries(root, dir);
        },
      },
      {
        name: 'fs.readText',
        description: 'Read a workspace file as text { path }.',
        run: (args) => {
          const root = requireRoot();
          return readWorkspaceUserFile(root, toAbsoluteUnderRoot(root, args.path));
        },
      },
      {
        name: 'fs.readBytes',
        description: 'Read a workspace file as a byte array { path }.',
        run: async (args) => {
          const root = requireRoot();
          const bytes = await readWorkspaceFileBytes(root, toAbsoluteUnderRoot(root, args.path));
          return Array.from(bytes);
        },
      },
      {
        name: 'fs.createFile',
        description: 'Create an empty file { path }. Optionally open it { open?: true }.',
        run: async (args) => {
          const root = requireRoot();
          const full = toAbsoluteUnderRoot(root, args.path);
          await createEmptyFileUnderWorkspace(root, full);
          ctxRef.current.ide.bumpFileTreeRevision();
          if (args.open === true) {
            await ctxRef.current.ide.openFileFromWorkspace(full);
          }
          return { created: full };
        },
      },
      {
        name: 'fs.writeBytes',
        description:
          'Write bytes to a workspace file { path, bytes } (bytes: number[] or hex string). File must exist (use fs.createFile first).',
        run: async (args) => {
          const root = requireRoot();
          const full = toAbsoluteUnderRoot(root, args.path);
          const bytes = decodeBytes(args.bytes);
          await writeWorkspaceFileBytes(root, full, bytes);
          ctxRef.current.ide.bumpFileTreeRevision();
          return { wrote: full, byteLength: bytes.length };
        },
      },
      {
        name: 'fs.writeText',
        description: 'Create-or-overwrite a workspace text file { path, text }.',
        run: async (args) => {
          const root = requireRoot();
          const full = toAbsoluteUnderRoot(root, args.path);
          const text = typeof args.text === 'string' ? args.text : '';
          // Ensure the file exists, then write its bytes (UTF-8).
          await createEmptyFileUnderWorkspace(root, full).catch(() => undefined);
          const bytes = new TextEncoder().encode(text);
          await writeWorkspaceFileBytes(root, full, bytes);
          ctxRef.current.ide.bumpFileTreeRevision();
          return { wrote: full, byteLength: bytes.length };
        },
      },
      {
        name: 'fs.createFolder',
        description: 'Create a directory { path }.',
        run: async (args) => {
          const root = requireRoot();
          const full = toAbsoluteUnderRoot(root, args.path);
          await createDirectoryUnderWorkspace(root, full);
          ctxRef.current.ide.bumpFileTreeRevision();
          return { created: full };
        },
      },
      {
        name: 'fs.rename',
        description: 'Rename/move within the workspace { from, to }.',
        run: async (args) => {
          const root = requireRoot();
          const from = toAbsoluteUnderRoot(root, args.from);
          const to = toAbsoluteUnderRoot(root, args.to);
          await renameUnderWorkspace(root, from, to);
          ctxRef.current.ide.bumpFileTreeRevision();
          return { from, to };
        },
      },
      {
        name: 'fs.delete',
        description: 'Delete a file or folder in the workspace { path }.',
        run: async (args) => {
          const root = requireRoot();
          const full = toAbsoluteUnderRoot(root, args.path);
          await deleteUnderWorkspace(root, full);
          ctxRef.current.ide.bumpFileTreeRevision();
          return { deleted: full };
        },
      },
      {
        name: 'process.run',
        description:
          'Run a program in the workspace { program, args?: string[], relativeCwd?, timeoutMs? }. Captures stdout/stderr/exit.',
        run: async (args) => {
          const root = requireRoot();
          const program = requireString(args.program, 'program');
          const argv = Array.isArray(args.args) ? args.args.map((a) => String(a)) : [];
          const result = await runWorkspaceCommand(root, program, {
            args: argv,
            relativeCwd: typeof args.relativeCwd === 'string' ? args.relativeCwd : null,
            timeoutMs: typeof args.timeoutMs === 'number' ? args.timeoutMs : null,
          });
          return summarizeRun(result);
        },
      },
      {
        name: 'process.build',
        description:
          'Convenience build: rustc { source, output? } in the workspace (output defaults to source without extension).',
        run: async (args) => {
          const root = requireRoot();
          const source = requireString(args.source, 'source');
          const output =
            typeof args.output === 'string' && args.output.trim() !== ''
              ? args.output
              : source.replace(/\.[^.]+$/, '');
          const result = await runWorkspaceCommand(root, 'rustc', {
            args: [source, '-o', output],
            timeoutMs: typeof args.timeoutMs === 'number' ? args.timeoutMs : 120_000,
          });
          ctxRef.current.ide.bumpFileTreeRevision();
          return { output, ...summarizeRun(result) };
        },
      },
    ]);

    return unregister;
  }, []);

  return null;
}
