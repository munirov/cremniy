/**
 * ЙЦУКЕН → QWERTY by physical key. When the keyboard layout wasn't switched,
 * typing a Latin command on the Russian layout comes out as Cyrillic gibberish
 * (cls → сды, clear → сдуфк, ls → ды). We map it back and, when the first token
 * is a recognised command, rewrite the line so it still runs.
 *
 * Companion to the keybinding fix that compares ev.code (physical key) instead
 * of ev.key — same root cause (layout not switched), different surface.
 */

const RU_TO_EN: Record<string, string> = {
  й: 'q', ц: 'w', у: 'e', к: 'r', е: 't', н: 'y', г: 'u', ш: 'i', щ: 'o', з: 'p', х: '[', ъ: ']',
  ф: 'a', ы: 's', в: 'd', а: 'f', п: 'g', р: 'h', о: 'j', л: 'k', д: 'l', ж: ';', э: "'",
  я: 'z', ч: 'x', с: 'c', м: 'v', и: 'b', т: 'n', ь: 'm', б: ',', ю: '.', ё: '`',
};

/** Commands we're willing to auto-correct into. Generous — false positives only
 *  fire when a Cyrillic command token happens to map to one of these. */
const KNOWN_COMMANDS = new Set([
  // shell / filesystem
  'cls', 'clear', 'ls', 'dir', 'cd', 'pwd', 'echo', 'cat', 'type', 'cp', 'copy',
  'mv', 'move', 'rm', 'del', 'mkdir', 'md', 'rmdir', 'rd', 'ren', 'rename', 'touch',
  'tree', 'find', 'findstr', 'grep', 'where', 'which', 'set', 'export', 'env',
  'history', 'exit', 'whoami', 'hostname', 'man', 'help', 'less', 'more', 'head', 'tail',
  'chmod', 'chown', 'ln', 'stat', 'df', 'du', 'ps', 'kill', 'tasklist', 'taskkill', 'sudo',
  // network
  'ping', 'curl', 'wget', 'ipconfig', 'ifconfig', 'netstat', 'ssh', 'scp', 'nslookup',
  // dev / reverse engineering
  'git', 'npm', 'npx', 'pnpm', 'yarn', 'node', 'python', 'py', 'pip', 'cargo', 'rustc',
  'rustup', 'go', 'make', 'cmake', 'gcc', 'g++', 'clang', 'gdb', 'lldb', 'objdump', 'nm',
  'readelf', 'strings', 'file', 'nasm', 'r2', 'radare2', 'rizin', 'code', 'vim', 'vi',
  'nano', 'docker', 'kubectl', 'cremniy',
]);

const CYRILLIC = /[Ѐ-ӿ]/;

function mapChar(ch: string): string {
  const lower = ch.toLowerCase();
  const en = RU_TO_EN[lower];
  if (en == null) {
    return ch;
  }
  // Preserve case so arguments aren't mangled (only matters for letters).
  return ch === lower ? en : en.toUpperCase();
}

function transliterate(line: string): string {
  let out = '';
  for (const ch of line) {
    out += mapChar(ch);
  }
  return out;
}

/**
 * If `line` is a Latin command mistyped on the Russian layout, return the
 * corrected Latin line; otherwise null (leave the input untouched). Only acts
 * when the command token itself was Cyrillic, so Cyrillic arguments — e.g.
 * `echo "привет"` — are never touched.
 */
export function correctCyrillicCommand(line: string): string | null {
  const originalFirst = line.trimStart().split(/\s+/)[0] ?? '';
  if (!CYRILLIC.test(originalFirst)) {
    return null;
  }
  const fixed = transliterate(line);
  const fixedFirst = (fixed.trimStart().split(/\s+/)[0] ?? '').toLowerCase();
  if (!KNOWN_COMMANDS.has(fixedFirst)) {
    return null;
  }
  return fixed;
}
