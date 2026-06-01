/**
 * Minimal contracts aligned with Qt `FileDataBuffer` / tool tabs host — extend when wiring real buffers.
 */
export type FileBufferEncoding = 'utf8' | 'binary';

export type FileBufferRef = {
  path: string;
  encoding: FileBufferEncoding;
};
