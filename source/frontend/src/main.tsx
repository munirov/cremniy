import './index.css';

import editorWorker from 'monaco-editor/esm/vs/editor/editor.worker?worker';

import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';

import App from './App.tsx';
import { installAgentBridge } from '@shared/agent/agentBridge';

self.MonacoEnvironment = {
  getWorker() {
    return new editorWorker();
  },
};

// Install window.cremniy before mount so components register into it.
// Docs: documentation/EN/agent_control_surface.md
installAgentBridge();

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
