/**
 * Minimal structural check for the Qt UI parity audit markdown.
 * Run from repo root: node tools/scripts/verify-ui-audit.mjs
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const AUDIT_FILE = path.join(
  ROOT,
  'ai_docs/develop/ui-parity/2026-04-29-qt-ui-audit.md'
);

/** Each rule must match at least one markdown heading line (# … through ###### …). */
const HEADING_RULES = [
  { id: 'welcome', describe: 'Welcome', test: (text) => /Welcome/.test(text) },
  { id: 'ide', describe: 'IDE', test: (text) => /\bIDE\b/.test(text) },
  { id: 'menu', describe: 'Menu', test: (text) => /\bMenu\b/.test(text) },
  { id: 'styles', describe: 'Styles/style', test: (text) => /style/i.test(text) },
];

function extractHeadingTexts(markdown) {
  const headings = [];
  for (const line of markdown.split(/\r?\n/)) {
    const m = /^(#{1,6})\s+(.*)$/.exec(line);
    if (m) headings.push(m[2].trim());
  }
  return headings;
}

function main() {
  if (!fs.existsSync(AUDIT_FILE)) {
    console.error(`verify-ui-audit: missing file ${AUDIT_FILE}`);
    process.exit(1);
  }

  const raw = fs.readFileSync(AUDIT_FILE, 'utf8');
  const headings = extractHeadingTexts(raw);

  const missing = [];
  for (const rule of HEADING_RULES) {
    const ok = headings.some((h) => rule.test(h));
    if (!ok) missing.push(rule.describe);
  }

  if (missing.length > 0) {
    console.error(
      `verify-ui-audit: required heading themes not found: ${missing.join(', ')}`
    );
    process.exit(1);
  }

  console.log(`verify-ui-audit: OK (${AUDIT_FILE})`);
}

main();
