import { describe, expect, it } from 'vitest';

import {
  MAX_HISTORY_ENTRIES,
  appendHistoryEntry,
  parseTerminalHistory,
  serializeTerminalHistory,
} from './terminalHistory';

describe('terminalHistory', () => {
  it('parses newline-separated commands, trimming blanks', () => {
    expect(parseTerminalHistory('ls\n  pwd  \n\ncd /tmp\n')).toEqual(['ls', 'pwd', 'cd /tmp']);
  });

  it('serializes with a trailing newline and caps to the last N', () => {
    expect(serializeTerminalHistory(['a', 'b'])).toBe('a\nb\n');
    expect(serializeTerminalHistory([])).toBe('');

    const many = Array.from({ length: MAX_HISTORY_ENTRIES + 5 }, (_, i) => `cmd${i}`);
    const lines = serializeTerminalHistory(many).trimEnd().split('\n');
    expect(lines).toHaveLength(MAX_HISTORY_ENTRIES);
    expect(lines[0]).toBe('cmd5');
  });

  it('appends commands, skipping consecutive duplicates and blanks', () => {
    expect(appendHistoryEntry(['ls'], 'ls')).toEqual(['ls']);
    expect(appendHistoryEntry(['ls'], '  ')).toEqual(['ls']);
    expect(appendHistoryEntry(['ls'], 'pwd')).toEqual(['ls', 'pwd']);
    expect(appendHistoryEntry(['ls'], '  pwd  ')).toEqual(['ls', 'pwd']);
  });

  it('caps appended history to the maximum size', () => {
    const full = Array.from({ length: MAX_HISTORY_ENTRIES }, (_, i) => `cmd${i}`);
    const next = appendHistoryEntry(full, 'newest');
    expect(next).toHaveLength(MAX_HISTORY_ENTRIES);
    expect(next[next.length - 1]).toBe('newest');
    expect(next[0]).toBe('cmd1');
  });
});
