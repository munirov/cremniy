/**
 * A tool-dock tab id. The set is now OPEN: rail tools are contributed by plugins
 * (see plugins/tools/), so any registered tool's id is valid. Kept as a plain
 * `string` rather than a closed union — the live set of ids lives in the plugin
 * registry (`pluginToolTabs()`), not here.
 *
 * The core ships one non-rail dock id that is NOT a plugin contribution:
 *   - 'codeEditor' — the editor-split mirror (IdeToolDock renders it directly).
 * The 8 rail tools ('binary', 'disassembler', 'strings', 'symbols', 'memoryMap',
 * 'functions', 'patches', 'resources') come from the Binary Tools plugin.
 */
export type ToolTabId = string;
