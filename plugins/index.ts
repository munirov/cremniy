import type { PluginManifest } from '@shared/plugins/contributions';

import connections from './connections';
import git from './git';
import tools from './tools';

/**
 * Locally-shipped plugins. To add one: create `plugins/<id>/` with an index that
 * default-exports a PluginManifest, then add it to this list. (Auto-discovery
 * and remote/server-delivered plugins are later steps — see PLUGINS.md.)
 */
export const PLUGINS: PluginManifest[] = [connections, git, tools];
