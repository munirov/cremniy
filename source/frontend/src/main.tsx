import './index.css';
// VS Code / Cursor icon set (Codicons) — used app-wide via `codicon codicon-*`.
import '@vscode/codicons/dist/codicon.css';

import editorWorker from 'monaco-editor/esm/vs/editor/editor.worker?worker';
import * as monaco from 'monaco-editor';
import { loader } from '@monaco-editor/react';

import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';

import App from './App.tsx';
import { installAgentBridge } from '@shared/agent/agentBridge';

self.MonacoEnvironment = {
  getWorker() {
    return new editorWorker();
  },
};

// Use the locally bundled monaco instead of @monaco-editor/react's default
// CDN AMD-loader. Inside the Tauri WebView the CDN fetch hangs forever
// (CSP / offline), which manifests as the editor stuck on "Loading...".
loader.config({ monaco });

// Install window.cremniy before mount so components register into it.
// Docs: documentation/architecture/AGENT_CONTROL.md
installAgentBridge();

// Suppress the WebView's default right-click menu (Back/Reload/Save As/Print/Inspect).
// Components with their own onContextMenu handlers keep working — those run before
// this listener and own their preventDefault. This catches the rest.
document.addEventListener('contextmenu', (event) => {
  event.preventDefault();
});

// Suppress WebView2 DevTools shortcuts so they don't shadow app bindings:
//   Ctrl+Shift+C — used by our terminal as "force copy", DevTools wants
//                  "inspect element" (same key in Chromium).
//   Ctrl+Shift+I / J — DevTools open / console.
//   F12 — DevTools toggle.
// Capture-phase so we win against the WebView2 default handler.
document.addEventListener(
  'keydown',
  (event) => {
    const ctrl = event.ctrlKey || event.metaKey;
    const key = event.key;
    if (event.key === 'F12') {
      event.preventDefault();
      return;
    }
    if (ctrl && event.shiftKey && (key === 'C' || key === 'I' || key === 'J' || key === 'c' || key === 'i' || key === 'j')) {
      event.preventDefault();
    }
  },
  true,
);

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
