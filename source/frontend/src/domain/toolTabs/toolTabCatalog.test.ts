import { describe, expect, it } from 'vitest';

import { toolTabEntry, TOOL_TAB_CATALOG } from '@domain/toolTabs/toolTabCatalog';

describe('toolTabCatalog', () => {
  it('lists three tools matching Qt tool tab areas', () => {
    expect(TOOL_TAB_CATALOG.map((e) => e.id)).toEqual(['binary', 'codeEditor', 'disassembler']);
  });

  it('toolTabEntry returns catalog row', () => {
    expect(toolTabEntry('disassembler').qtPath).toContain('Disassembler');
  });
});
