/** Signal analysis helpers shared by the audio-rendering tests. */

import { MorphCore } from '../src/dsp/core';
import type { SlotSettings } from '../src/dsp/slot';

export const SR = 48000;

export function sine(freq: number, seconds: number, amp = 0.5, sr = SR): Float32Array {
  const n = Math.round(seconds * sr);
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) out[i] = amp * Math.sin((2 * Math.PI * freq * i) / sr);
  return out;
}

/** Deterministic white noise (mulberry32). */
export function noise(seconds: number, amp = 0.3, seed = 1, sr = SR): Float32Array {
  let a = seed >>> 0;
  const n = Math.round(seconds * sr);
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    out[i] = amp * ((((t ^ (t >>> 14)) >>> 0) / 4294967296) * 2 - 1);
  }
  return out;
}

export function settingsFor(frames: number, extra: Partial<SlotSettings> = {}): SlotSettings {
  return {
    gain: 1,
    mute: false,
    solo: false,
    reverse: false,
    region: { trimStart: 0, loopStart: 0, loopEnd: frames },
    ...extra,
  };
}

/** Render `seconds` from the core in 128-frame blocks, with an optional hook per block. */
export function render(core: MorphCore, seconds: number, onBlock?: (blockIndex: number, time: number) => void) {
  const frames = Math.round(seconds * core.sampleRate);
  const L = new Float32Array(frames);
  const R = new Float32Array(frames);
  const bl = new Float32Array(128);
  const br = new Float32Array(128);
  let b = 0;
  for (let off = 0; off < frames; off += 128, b++) {
    onBlock?.(b, off / core.sampleRate);
    const n = Math.min(128, frames - off);
    core.process(bl, br, n);
    L.set(bl.subarray(0, n), off);
    R.set(br.subarray(0, n), off);
  }
  return { L, R };
}

/** Largest absolute second difference: a click detector for smooth signals. */
export function maxSecondDiff(x: Float32Array, from = 2, to = x.length): number {
  let m = 0;
  for (let i = Math.max(2, from); i < to; i++) {
    const d = Math.abs(x[i] - 2 * x[i - 1] + x[i - 2]);
    if (d > m) m = d;
  }
  return m;
}

export function rms(x: Float32Array, from = 0, to = x.length): number {
  let s = 0;
  for (let i = from; i < to; i++) s += x[i] * x[i];
  return Math.sqrt(s / Math.max(1, to - from));
}

export function peak(x: Float32Array): number {
  let m = 0;
  for (let i = 0; i < x.length; i++) m = Math.max(m, Math.abs(x[i]));
  return m;
}

export function hasNonFinite(x: Float32Array): boolean {
  for (let i = 0; i < x.length; i++) if (!Number.isFinite(x[i])) return true;
  return false;
}
