import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom';

import { RootApp } from '@boundary/RootApp';
import { WelcomeView } from '@boundary/welcome/WelcomeView';
import { WorkspaceProvider } from '@boundary/workspace/WorkspaceContext';
import { settingsService } from '@infrastructure/settings/settingsService';

export default function App() {
  return (
    <BrowserRouter>
      <WorkspaceProvider>
        <Routes>
          <Route path="/" element={<WelcomeView />} />
          <Route path="/ide" element={<RootApp settingsService={settingsService} />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </WorkspaceProvider>
    </BrowserRouter>
  );
}
