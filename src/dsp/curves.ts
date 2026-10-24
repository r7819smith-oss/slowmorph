/**
 * Transition curves and gain laws.
 *
 * Two separate ideas live here:
 *  - A *transition curve* reshapes the local progress `t` (0..1) of a
 *    transition between two timeline nodes (linear, S-curve, ease in/out).
 *  - A *gain law* converts a mix fraction (0..1) into an amplitude gain.
 *    Equal-power keeps the summed power of uncorrelated sources constant;
 *    linear keeps the summed amplitude of correlated sources constant.
 *
 * Every curve is monotonic, maps 0→0 and 1→1, and is allocation-free so it
 * can run on the audio thread.
 */

export type CurveType = 'linear' | 'sCurve' | 'easeIn' | 'easeOut';
export type GainLaw = 'equalPower' | 'linear';

export const CURVE_TYPES: readonly CurveType[] = ['linear', 'sCurve', 'easeIn', 'easeOut'];

export function clamp01(x: number): number {
  return x < 0 ? 0 : x > 1 ? 1 : x;
}

/**
 * Reshape t with the given curve. `shape` (0..1) sets the strength: at 0
 * every curve is linear, at 1 the exponent is 5.
 */
export function applyCurve(t: number, type: CurveType, shape = 0.5): number {
  t = clamp01(t);
  if (type === 'linear') return t;
  const k = 1 + clamp01(shape) * 4;
  switch (type) {
    case 'easeIn':
      return Math.pow(t, k);
    case 'easeOut':
      return 1 - Math.pow(1 - t, k);
    case 'sCurve':
      return t < 0.5 ? 0.5 * Math.pow(2 * t, k) : 1 - 0.5 * Math.pow(2 - 2 * t, k);
  }
  return t;
}

/** Convert a mix fraction to an amplitude gain. */
export function fractionToGain(fraction: number, law: GainLaw): number {
  const f = clamp01(fraction);
  return law === 'equalPower' ? Math.sqrt(f) : f;
}

export function dbToGain(db: number): number {
  return db <= -96 ? 0 : Math.pow(10, db / 20);
}

export function gainToDb(gain: number): number {
  return gain <= 0.0000158 ? -96 : 20 * Math.log10(gain);
}
