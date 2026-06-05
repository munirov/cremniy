import { createContext, useCallback, useContext, useMemo, useState, type ReactNode } from 'react';

/**
 * Cross-tab binary selection (Qt parity: FormatPage selectionChanged signal,
 * QHexView::setSelection). All binary-aware tabs (HEX, Disasm, BinaryFormat,
 * Symbols, MemoryMap, Functions) read and write this single shared value, so
 * clicking an instruction in Disasm scrolls HEX to the same offset, picking
 * a symbol jumps to its address, etc.
 *
 * This is L1 (centralization) + D4 (propagation) in one — selection is the
 * only piece of binary state that genuinely needs to flow between tabs;
 * everything else (bytes, dirty flag, undo stack) can stay local to the HEX
 * panel.
 */

export type BinarySelection = Readonly<{
  /** Absolute file offset of the first byte. */
  offset: number;
  /** Number of bytes selected. `0` means a caret position with no range. */
  length: number;
  /** Optional source label — useful when one tab needs to ignore its own echo. */
  source?: string;
}>;

type Ctx = {
  selection: BinarySelection | null;
  setSelection: (s: BinarySelection | null) => void;
};

const BinarySelectionContext = createContext<Ctx | null>(null);

export function BinarySelectionProvider({ children }: { children: ReactNode }) {
  const [selection, setSelectionState] = useState<BinarySelection | null>(null);
  const setSelection = useCallback((s: BinarySelection | null) => {
    setSelectionState(s);
  }, []);
  const value = useMemo<Ctx>(() => ({ selection, setSelection }), [selection, setSelection]);
  return (
    <BinarySelectionContext.Provider value={value}>{children}</BinarySelectionContext.Provider>
  );
}

export function useBinarySelection(): BinarySelection | null {
  return useContext(BinarySelectionContext)?.selection ?? null;
}

export function useSetBinarySelection(): (s: BinarySelection | null) => void {
  const ctx = useContext(BinarySelectionContext);
  return ctx?.setSelection ?? (() => undefined);
}
