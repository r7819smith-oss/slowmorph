/**
 * Offline rendering: runs the same MorphCore as the live instrument, as
 * fast as the CPU allows, from the start of the morph to its end plus a
 * tail. Pure TypeScript, so it runs in a Web Worker (and in tests).
 */

import { MorphCore } from '../dsp/core';
import type { ToWorklet } from './messages';
import { applyMessage } from './apply-message';

export interface OfflineOptions {
  sampleRate: number;
  /** Seconds after the morph ends to keep rendering (reverb/delay tails, final sample). */
  tailSeconds: number;
  /** Frames per emitted chunk. */
  chunkFrames?: number;
}

export interface OfflineProgress {
  framesDone: number;
  framesTotal: number;
}

/**
 * Render a full morph. `messages` reproduce the project (see stateMessages);
 * the morph is forced to play forwards once from the start. Calls `onChunk`
 * for every chunk and returns false if cancelled.
 */
export async function renderMorph(
  messages: ToWorklet[],
  opts: OfflineOptions,
  onChunk: (L: Float32Array, R: Float32Array, p: OfflineProgress) => void | Promise<void>,
  cancelled: () => boolean = () => false,
): Promise<boolean> {
  const core = new MorphCore(opts.sampleRate);
  for (const m of messages) applyMessage(core, m);
  core.setPlayMode('once');
  core.setDirection(1);
  core.setHold(false);
  core.seek(0);
  core.play();
  const sr = opts.sampleRate;
  const fadeFrames = Math.round(0.1 * sr);
  const total = Math.round((core.clock.duration + opts.tailSeconds) * sr) + fadeFrames;
  const chunk = opts.chunkFrames ?? sr;
  const bl = new Float32Array(128);
  const br = new Float32Array(128);
  let done = 0;
  let stopped = false;
  while (done < total) {
    if (cancelled()) return false;
    const n = Math.min(chunk, total - done);
    const L = new Float32Array(n);
    const R = new Float32Array(n);
    for (let o = 0; o < n; o += 128) {
      if (!stopped && done + o >= total - fadeFrames) {
        core.stop(); // 30 ms fade-out at the very end
        stopped = true;
      }
      const k = Math.min(128, n - o);
      core.process(bl, br, k);
      L.set(bl.subarray(0, k), o);
      R.set(br.subarray(0, k), o);
    }
    done += n;
    await onChunk(L, R, { framesDone: done, framesTotal: total });
  }
  return true;
}
