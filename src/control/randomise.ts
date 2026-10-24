/**
 * Musical randomisation. Each group draws from ranges chosen to stay
 * useful (no extreme or broken settings), and the app keeps an undo stack.
 */

import type { GranularParams } from '../dsp/engines/granular';
import type { SpectralParams, FftSize } from '../dsp/engines/spectral';
import type { EffectsParams } from '../dsp/effects';
import type { Segment } from '../dsp/timeline';
import { CURVE_TYPES } from '../dsp/curves';
import { WINDOW_TYPES } from '../dsp/windows';

export type RandomGroup = 'granular' | 'spectral' | 'curves' | 'effects';

type Rand = () => number;
const pick = <T>(r: Rand, xs: readonly T[]): T => xs[Math.floor(r() * xs.length) % xs.length];
const range = (r: Rand, lo: number, hi: number) => lo + (hi - lo) * r();
const logRange = (r: Rand, lo: number, hi: number) => lo * Math.pow(hi / lo, r());
const round = (x: number, step: number) => Math.round(x / step) * step;

export function randomGranular(r: Rand): Partial<GranularParams> {
  return {
    sizeMs: Math.round(logRange(r, 30, 400)),
    overlap: round(range(r, 2, 10), 0.5),
    density: round(range(r, 0.6, 1), 0.01),
    positionRandom: round(range(r, 0, 0.6), 0.01),
    pitchRandomCents: r() < 0.5 ? 0 : Math.round(range(r, 5, 300)),
    transpose: r() < 0.7 ? 0 : pick(r, [-12, -7, -5, 5, 7, 12]),
    speed: pick(r, [0, 0.25, 0.5, 1, 1, -0.5]),
    spread: round(range(r, 0.2, 0.9), 0.01),
    window: pick(r, WINDOW_TYPES),
    jitter: round(range(r, 0, 0.8), 0.01),
    seed: 1 + Math.floor(r() * 99999),
  };
}

export function randomSpectral(r: Rand): Partial<SpectralParams> {
  return {
    fftSize: pick(r, [1024, 2048, 2048, 4096] as FftSize[]),
    interp: round(r(), 0.01),
    envelopeLead: round(range(r, -0.8, 0.8), 0.01),
    blur: round(range(r, 0, 0.7), 0.01),
    smoothing: round(range(r, 0, 0.4), 0.01),
    phaseRandom: round(range(r, 0, 0.3), 0.01),
    speed: pick(r, [0.25, 0.5, 1, 1, -0.5]),
    width: round(range(r, 0.1, 0.7), 0.01),
    warp: r() < 0.6 ? 0 : round(range(r, -0.3, 0.3), 0.01),
  };
}

export function randomSegment(r: Rand): Segment {
  const start = round(range(r, 0, 0.4), 0.01);
  return {
    curve: pick(r, CURVE_TYPES),
    shape: round(range(r, 0.2, 0.9), 0.01),
    tStart: start,
    tEnd: Math.max(start + 0.1, round(range(r, 0.6, 1), 0.01)),
  };
}

export function randomEffects(r: Rand): Partial<EffectsParams> {
  return {
    lowpassHz: Math.round(logRange(r, 1500, 20000)),
    highpassHz: Math.round(logRange(r, 20, 300)),
    resonance: round(range(r, 0, 0.5), 0.01),
    width: round(range(r, 0.7, 1.6), 0.01),
    reverbSend: round(range(r, 0, 0.5), 0.01),
    reverbSize: round(range(r, 0.4, 0.95), 0.01),
    delaySend: round(range(r, 0, 0.3), 0.01),
    delayTime: round(range(r, 0.1, 1.2), 0.01),
    delayFeedback: round(range(r, 0, 0.6), 0.01),
  };
}
