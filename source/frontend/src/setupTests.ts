import '@testing-library/jest-dom/vitest';
// Registers RTL's automatic afterEach(cleanup) via global afterEach (requires test.globals).
import '@testing-library/react';

// jsdom doesn't implement matchMedia or ResizeObserver; xterm.js (terminal) and
// some panels need them. Provide inert shims so components can mount under test.
if (typeof window !== 'undefined') {
  if (typeof window.matchMedia !== 'function') {
    window.matchMedia = (query: string): MediaQueryList =>
      ({
        matches: false,
        media: query,
        onchange: null,
        addListener: () => {},
        removeListener: () => {},
        addEventListener: () => {},
        removeEventListener: () => {},
        dispatchEvent: () => false,
      }) as MediaQueryList;
  }
  if (typeof globalThis.ResizeObserver === 'undefined') {
    globalThis.ResizeObserver = class {
      observe(): void {}
      unobserve(): void {}
      disconnect(): void {}
    } as typeof ResizeObserver;
  }
}
