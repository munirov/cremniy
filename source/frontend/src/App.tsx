import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom';

import { RootApp } from '@boundary/RootApp';
import { MenuSlotProvider, useMenuSlot } from '@boundary/chrome/MenuSlotContext';
import { TitleBar } from '@boundary/chrome/TitleBar';
import { LocaleProvider } from '@boundary/i18n/LocaleContext';
import { PaneRegistryProvider } from '@boundary/layout/Pane';
import { PopoutView } from '@boundary/layout/PopoutView';
import { NotificationProvider } from '@boundary/notifications/NotificationContext';
import { WelcomeView } from '@boundary/welcome/WelcomeView';
import { BinarySelectionProvider } from '@boundary/workspace/BinarySelectionContext';
import { WorkspaceProvider } from '@boundary/workspace/WorkspaceContext';
import { settingsService } from '@infrastructure/settings/settingsService';

/**
 * Global window chrome — the titlebar must render on EVERY route (Welcome,
 * IDE, popout) because Tauri's native window decorations are disabled. If
 * the bar only existed under one route, the user would see a windowless
 * window with no min / max / close anywhere else.
 */
function ChromeShell() {
  const { menu, settingsAction } = useMenuSlot();
  return (
    <div style={{ display: 'flex', flexDirection: 'column', width: '100%', height: '100vh' }}>
      <TitleBar menu={menu} onOpenSettings={settingsAction ?? undefined} />
      <div style={{ flex: 1, minHeight: 0, display: 'flex', flexDirection: 'column' }}>
        <Routes>
          <Route path="/" element={<WelcomeView />} />
          <Route path="/ide" element={<RootApp settingsService={settingsService} />} />
          <Route path="/popout/:id" element={<PopoutView />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </div>
    </div>
  );
}

export default function App() {
  return (
    <BrowserRouter>
      <LocaleProvider>
        <NotificationProvider>
          <WorkspaceProvider>
            <PaneRegistryProvider>
              <BinarySelectionProvider>
                <MenuSlotProvider>
                  <ChromeShell />
                </MenuSlotProvider>
              </BinarySelectionProvider>
            </PaneRegistryProvider>
          </WorkspaceProvider>
        </NotificationProvider>
      </LocaleProvider>
    </BrowserRouter>
  );
}
