/**
 * Original Character ←→ Strange Hybrid.
 *
 * One control, 0..1, that moves each engine from faithful to radical. At 0
 * every engine uses exactly the user's own settings; raising it adds a
 * fixed, documented set of changes on top. Nothing here is random by
 * itself: randomness only increases through the engines' own seeded
 * generators, and loudness is held by the engines' normalisation.
 *
 * Crossfade (stays amplitude-based): each slot gets a gentle low-pass that
 * slowly wanders (down to 1.5 kHz at 100 %) and a slow amplitude shimmer
 * (up to ±2.5 dB). The timeline weights are untouched.
 *
 * Granular:   grain size × (1 − 0.6c) · position random + 0.8c·(1 − base)
 *             · pitch variation + 400c² cents · onset jitter + c·(1 − base)
 *             · timeline smear 0.2c (sources further away on the timeline
 *               are interleaved, blurring sample boundaries)
 *
 * Spectral:   interpolation + c·(1 − base) (toward hybrid) · blur + 0.6c·(1 − base)
 *             · smoothing + 0.3c · phase random + 0.45c² · inharmonic warp + 0.6c²
 *             · timeline smear 0.15c
 */

import type { GranularParams } from './engines/granular';
import type { SpectralParams } from './engines/spectral';

export const CHARACTER_SUMMARY = {
  crossfade: 'Adds a slowly wandering low-pass and a gentle amplitude shimmer per sample. Still a pure amplitude crossfade.',
  granular: 'Smaller grains, more position, pitch and timing randomness, and more interleaving of neighbouring samples.',
  spectral: 'Toward hybrid interpolation, more blur and smoothing, then phase randomisation and inharmonic warping.',
} as const;

const clamp = (x: number, lo: number, hi: number) => (x < lo ? lo : x > hi ? hi : x);

export function granularWithCharacter(base: GranularParams, c: number, out: GranularParams): void {
  out.sizeMs = clamp(base.sizeMs * (1 - 0.6 * c), 10, 500);
  out.overlap = base.overlap;
  out.density = base.density;
  out.positionRandom = clamp(base.positionRandom + 0.8 * c * (1 - base.positionRandom), 0, 1);
  out.pitchRandomCents = clamp(base.pitchRandomCents + 400 * c * c, 0, 1200);
  out.transpose = base.transpose;
  out.speed = base.speed;
  out.spread = base.spread;
  out.window = base.window;
  out.seed = base.seed;
  out.jitter = clamp(base.jitter + c * (1 - base.jitter), 0, 1);
}

export function spectralWithCharacter(base: SpectralParams, c: number, out: SpectralParams): void {
  out.fftSize = base.fftSize;
  out.overlap = base.overlap;
  out.interp = clamp(base.interp + c * (1 - base.interp), 0, 1);
  out.envelopeLead = base.envelopeLead;
  out.blur = clamp(base.blur + 0.6 * c * (1 - base.blur), 0, 1);
  out.smoothing = clamp(base.smoothing + 0.3 * c, 0, 1);
  out.phaseRandom = clamp(base.phaseRandom + 0.45 * c * c, 0, 1);
  out.speed = base.speed;
  out.width = base.width;
  out.warp = clamp(base.warp + 0.6 * c * c, -1, 1);
}

/** How far (in timeline units) each engine smears the timeline weights. */
export function timelineSmear(engine: string, c: number): number {
  if (engine === 'granular') return 0.2 * c;
  if (engine === 'spectral') return 0.15 * c;
  return 0;
}
