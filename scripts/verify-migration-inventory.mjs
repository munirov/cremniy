/**
 * Minimal structural check for Qt→Tauri migration inventory markdown.
 * Run from repo root: node scripts/verify-migration-inventory.mjs
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const INVENTORY_FILE = path.join(
  ROOT,
  'ai_docs/develop/migration/2026-04-30-qt-to-tauri-inventory.md'
);

const HEADING_RULES = [
  { id: 'executive', describe: 'Executive', test: (t) => /executive/i.test(t) },
  {
    id: 'table',
    describe: 'Table or matrix',
    test: (t) => /\b(table|matrix)\b/i.test(t),
  },
  {
    id: 'phase-wave',
    describe: 'Phase or wave',
    test: (t) => /\b(phase|wave)\d*\b/i.test(t),
  },
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
  if (!fs.existsSync(INVENTORY_FILE)) {
    console.error(`verify-migration-inventory: missing file ${INVENTORY_FILE}`);
    process.exit(1);
  }

  const raw = fs.readFileSync(INVENTORY_FILE, 'utf8');
  const headings = extractHeadingTexts(raw);

  const missing = [];
  for (const rule of HEADING_RULES) {
    if (!headings.some((h) => rule.test(h))) missing.push(rule.describe);
  }

  if (missing.length > 0) {
    console.error(
      `verify-migration-inventory: required heading themes not found: ${missing.join(', ')}`
    );
    process.exit(1);
  }

  console.log(`verify-migration-inventory: OK (${INVENTORY_FILE})`);
}

main();
