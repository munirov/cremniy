import { useId, useMemo, useRef, useState, type KeyboardEvent } from 'react';

import {
  analyzeBinaryFormat,
  type BinaryFormatId,
  type BinaryFormatPageSummary,
} from '@domain/binaryFormat/binaryFormat';

import styles from './BinaryFormatPanel.module.css';

const FORMAT_TABS: readonly BinaryFormatId[] = ['raw', 'elf', 'pe', 'mbr'];

type BinaryFormatPanelProps = Readonly<{
  bytes: Uint8Array;
}>;

export function BinaryFormatPanel({ bytes }: BinaryFormatPanelProps) {
  const tabBaseId = useId();
  const [activeFormat, setActiveFormat] = useState<BinaryFormatId>('raw');
  const tabRefs = useRef<Partial<Record<BinaryFormatId, HTMLButtonElement | null>>>({});
  const analysis = useMemo(() => analyzeBinaryFormat(bytes), [bytes]);

  const activateTab = (format: BinaryFormatId) => {
    setActiveFormat(format);
    tabRefs.current[format]?.focus();
  };

  const handleTabKeyDown = (format: BinaryFormatId, event: KeyboardEvent<HTMLButtonElement>) => {
    const currentIndex = FORMAT_TABS.indexOf(format);
    if (currentIndex === -1) {
      return;
    }

    if (event.key === 'ArrowLeft') {
      event.preventDefault();
      activateTab(FORMAT_TABS[(currentIndex - 1 + FORMAT_TABS.length) % FORMAT_TABS.length]!);
      return;
    }
    if (event.key === 'ArrowRight') {
      event.preventDefault();
      activateTab(FORMAT_TABS[(currentIndex + 1) % FORMAT_TABS.length]!);
      return;
    }
    if (event.key === 'Home') {
      event.preventDefault();
      activateTab(FORMAT_TABS[0]!);
      return;
    }
    if (event.key === 'End') {
      event.preventDefault();
      activateTab(FORMAT_TABS[FORMAT_TABS.length - 1]!);
    }
  };

  return (
    <section className={styles.root} aria-label="Binary format">
      <div className={styles.header}>
        <h3 className={styles.title}>Format</h3>
        <span className={styles.detected}>
          Detected: {analysis.detected === 'unknown' ? 'Unknown' : analysis.pages[analysis.detected].label}
        </span>
      </div>
      <div className={styles.tabs} role="tablist" aria-label="Binary format pages">
        {FORMAT_TABS.map((format) => {
          const tab = analysis.pages[format];
          const selected = format === activeFormat;
          return (
            <button
              key={format}
              ref={(el) => {
                tabRefs.current[format] = el;
              }}
              type="button"
              id={getTabId(tabBaseId, format)}
              role="tab"
              aria-controls={getPanelId(tabBaseId, format)}
              aria-selected={selected}
              tabIndex={selected ? 0 : -1}
              className={selected ? `${styles.tab} ${styles.tabActive}` : styles.tab}
              onClick={() => setActiveFormat(format)}
              onKeyDown={(event) => handleTabKeyDown(format, event)}
            >
              {tab.label}
            </button>
          );
        })}
      </div>
      {FORMAT_TABS.map((format) => (
        <FormatPage
          key={format}
          page={analysis.pages[format]}
          panelId={getPanelId(tabBaseId, format)}
          tabId={getTabId(tabBaseId, format)}
          hidden={format !== activeFormat}
        />
      ))}
    </section>
  );
}

function getTabId(baseId: string, format: BinaryFormatId): string {
  return `${baseId}-${format}-tab`;
}

function getPanelId(baseId: string, format: BinaryFormatId): string {
  return `${baseId}-${format}-panel`;
}

function FormatPage({
  page,
  panelId,
  tabId,
  hidden,
}: {
  page: BinaryFormatPageSummary;
  panelId: string;
  tabId: string;
  hidden: boolean;
}) {
  return (
    <div className={styles.page} id={panelId} role="tabpanel" aria-labelledby={tabId} hidden={hidden}>
      <p className={page.status === 'supported' ? styles.message : styles.messageMuted}>
        {page.message}
      </p>
      {page.fields.length > 0 ? (
        <dl className={styles.fields}>
          {page.fields.map((field) => (
            <div key={field.label} className={styles.field}>
              <dt>{field.label}</dt>
              <dd>{field.value}</dd>
            </div>
          ))}
        </dl>
      ) : null}
      {page.partitions != null ? (
        <ol className={styles.partitions} aria-label="Partition entries">
          {page.partitions.map((partition) => (
            <li key={partition.index} className={styles.partition}>
              <span>#{partition.index}</span>
              <span>{partition.active === 'Yes' ? 'Active' : 'Inactive'}</span>
              <span>{partition.type}</span>
              <span>LBA {partition.startLba}</span>
              <span>{partition.sectorCount} sectors</span>
              <span>{partition.description}</span>
            </li>
          ))}
        </ol>
      ) : null}
    </div>
  );
}
