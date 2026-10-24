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

import { FFT } from '../src/dsp/fft';

/** Hann-windowed magnitude spectrum of x[from .. from+n). */
export function spectrum(x: Float32Array, from: number, n = 8192): Float64Array {
  const fft = new FFT(n);
  const re = new Float64Array(n);
  const im = new Float64Array(n);
  for (let i = 0; i < n; i++) re[i] = (x[from + i] ?? 0) * (0.5 - 0.5 * Math.cos((2 * Math.PI * i) / n));
  fft.transform(re, im);
  const out = new Float64Array(n / 2);
  for (let k = 0; k < n / 2; k++) out[k] = Math.hypot(re[k], im[k]);
  return out;
}

/** Frequency (Hz) of the strongest bin. */
export function peakFrequency(x: Float32Array, from: number, sr = SR, n = 8192): number {
  const s = spectrum(x, from, n);
  let best = 1;
  for (let k = 2; k < s.length; k++) if (s[k] > s[best]) best = k;
  return (best * sr) / n;
}

/** Share of spectral energy within ±bw Hz of the given frequencies. */
export function energyNear(x: Float32Array, from: number, freqs: number[], bw = 25, sr = SR, n = 8192): number {
  const s = spectrum(x, from, n);
  let near = 0;
  let total = 0;
  for (let k = 1; k < s.length; k++) {
    const e = s[k] * s[k];
    total += e;
    const f = (k * sr) / n;
    if (freqs.some((c) => Math.abs(f - c) <= bw)) near += e;
  }
  return near / total;
}

/** One-pole filtered noise: lowpass (coef > 0) or highpass (coef < 0). */
export function colouredNoise(seconds: number, coef: number, amp = 0.3, seed = 1): Float32Array {
  const w = noise(seconds, amp, seed);
  const out = new Float32Array(w.length);
  let y = 0;
  let prev = 0;
  for (let i = 0; i < w.length; i++) {
    if (coef >= 0) y = y * coef + w[i] * (1 - coef);
    else {
      y = -coef * (y + w[i] - prev);
      prev = w[i];
    }
    out[i] = y;
  }
  const r = Math.sqrt(out.reduce((a, b) => a + b * b, 0) / out.length);
  return out.map((v) => (v / r) * amp * 0.5);
}
