/**
 * Destinations for long WAV files. In Chrome and Edge the file is streamed
 * straight to disk (File System Access API), so hour-long recordings do not
 * have to fit in memory. Elsewhere, or if the user cancels the file dialog,
 * the audio is kept in memory and downloaded at the end.
 */

import { wavHeader, type WavFormat } from './wav';

export interface WavSink {
  readonly kind: 'file' | 'memory';
  readonly name: string;
  write(bytes: Uint8Array): Promise<void>;
  /** Write the header and close; returns a Blob for memory sinks. */
  finish(frames: number, sampleRate: number, format: WavFormat): Promise<Blob | null>;
  abort(): Promise<void>;
}

interface SaveFilePickerOptions {
  suggestedName?: string;
  types?: { description: string; accept: Record<string, string[]> }[];
}
interface WritableLike {
  write(data: { type: 'write'; position: number; data: Uint8Array } | Uint8Array): Promise<void>;
  close(): Promise<void>;
  abort(): Promise<void>;
}
interface HandleLike {
  name: string;
  createWritable(): Promise<WritableLike>;
}
type PickerWindow = Window & { showSaveFilePicker?: (o: SaveFilePickerOptions) => Promise<HandleLike> };

export function canStreamToDisk(): boolean {
  return typeof (window as PickerWindow).showSaveFilePicker === 'function';
}

class FileSink implements WavSink {
  readonly kind = 'file' as const;
  private pos = 44;
  private chain: Promise<void> = Promise.resolve();
  constructor(private readonly w: WritableLike, readonly name: string) {}
  write(bytes: Uint8Array): Promise<void> {
    const at = this.pos;
    this.pos += bytes.length;
    this.chain = this.chain.then(() => this.w.write({ type: 'write', position: at, data: bytes }));
    return this.chain;
  }
  async finish(frames: number, sampleRate: number, format: WavFormat): Promise<null> {
    await this.chain;
    await this.w.write({ type: 'write', position: 0, data: wavHeader(frames, 2, sampleRate, format) });
    await this.w.close();
    return null;
  }
  async abort(): Promise<void> {
    await this.w.abort().catch(() => {});
  }
}

class MemorySink implements WavSink {
  readonly kind = 'memory' as const;
  private chunks: Uint8Array[] = [];
  constructor(readonly name: string) {}
  async write(bytes: Uint8Array): Promise<void> {
    this.chunks.push(bytes);
  }
  async finish(frames: number, sampleRate: number, format: WavFormat): Promise<Blob> {
    const blob = new Blob([wavHeader(frames, 2, sampleRate, format), ...this.chunks] as BlobPart[], { type: 'audio/wav' });
    this.chunks = [];
    return blob;
  }
  async abort(): Promise<void> {
    this.chunks = [];
  }
}

/**
 * Open a sink. Must be called from a user gesture for the file dialog to
 * appear. Returns null if the user cancelled the dialog.
 */
export async function openWavSink(suggestedName: string, preferDisk = true): Promise<WavSink | null> {
  const w = window as PickerWindow;
  if (preferDisk && w.showSaveFilePicker) {
    try {
      const handle = await w.showSaveFilePicker({ suggestedName, types: [{ description: 'WAV audio', accept: { 'audio/wav': ['.wav'] } }] });
      const writable = await handle.createWritable();
      return new FileSink(writable, handle.name);
    } catch (e) {
      if ((e as Error).name === 'AbortError') return null;
      // fall through to memory if the API is blocked
    }
  }
  return new MemorySink(suggestedName);
}

export function downloadBlob(blob: Blob, name: string): void {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60_000);
}
