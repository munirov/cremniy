import { useEffect, useState } from 'react';

import { convertFileSrc } from '@tauri-apps/api/core';

import { fileNameFromPath } from '@domain/workspace/paths';

import styles from './ImageTab.module.css';

type Dimensions = { width: number; height: number };

/**
 * Shown in the editor slot when the active tab is a raster image (png, jpg,
 * gif, …). Images are binary, so they'd otherwise fall through to the
 * BinaryFilePlaceholder — here we actually render the picture instead.
 *
 * The image is loaded through Tauri's asset protocol (`convertFileSrc` turns
 * the absolute path into an `asset://` URL the webview fetches natively), so no
 * bytes travel over the IPC bridge and the file can be any size.
 */
export function ImageTab({ filePath }: { filePath: string | null }) {
  const [dimensions, setDimensions] = useState<Dimensions | null>(null);
  const [failed, setFailed] = useState(false);

  // New file → drop the previous load's state so dimensions / error don't leak
  // across tabs.
  useEffect(() => {
    setDimensions(null);
    setFailed(false);
  }, [filePath]);

  const name = filePath != null ? fileNameFromPath(filePath) : '';
  const src = filePath != null && filePath !== '' ? convertFileSrc(filePath) : '';

  if (failed) {
    return (
      <div className={styles.root} role="status" aria-label="Image file">
        <p className={styles.error}>
          Couldn&apos;t preview this image — {name}
        </p>
      </div>
    );
  }

  return (
    <div className={styles.root} role="img" aria-label={name || 'Image'}>
      <div className={styles.stage}>
        {src !== '' ? (
          <img
            className={styles.image}
            src={src}
            alt={name}
            onLoad={(ev) => {
              const img = ev.currentTarget;
              setDimensions({ width: img.naturalWidth, height: img.naturalHeight });
            }}
            onError={() => setFailed(true)}
          />
        ) : null}
      </div>
      <div className={styles.footer} title={filePath ?? undefined}>
        {name}
        {dimensions != null ? (
          <span className={styles.dims}>
            {' · '}
            {dimensions.width} × {dimensions.height}
          </span>
        ) : null}
      </div>
    </div>
  );
}
