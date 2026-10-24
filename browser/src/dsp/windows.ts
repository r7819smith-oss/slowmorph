/**
 * Grain windows as lookup tables. All start and end at zero so grains
 * never click; they differ in how much of the grain plays at full level.
 */

export type WindowType = 'hann' | 'tukey' | 'gaussian' | 'trapezoid';
export const WINDOW_TYPES: readonly WindowType[] = ['hann', 'tukey', 'gaussian', 'trapezoid'];

export const WINDOW_SIZE = 2048;

export function windowValue(type: WindowType, t: number): number {
  if (t <= 0 || t >= 1) return 0;
  switch (type) {
    case 'hann':
      return 0.5 - 0.5 * Math.cos(2 * Math.PI * t);
    case 'tukey': {
      // 50 % cosine taper
      const a = 0.5;
      if (t < a / 2) return 0.5 - 0.5 * Math.cos((2 * Math.PI * t) / a);
      if (t > 1 - a / 2) return 0.5 - 0.5 * Math.cos((2 * Math.PI * (1 - t)) / a);
      return 1;
    }
    case 'gaussian': {
      // σ = 0.15, shifted and scaled so the ends are exactly zero
      const g = (x: number) => Math.exp(-0.5 * ((x - 0.5) / 0.15) ** 2);
      const e = g(0);
      return (g(t) - e) / (1 - e);
    }
    case 'trapezoid': {
      // 15 % raised-cosine ramps (smooth corners, so no spectral splatter)
      const r = 0.15;
      if (t < r) return 0.5 - 0.5 * Math.cos((Math.PI * t) / r);
      if (t > 1 - r) return 0.5 - 0.5 * Math.cos((Math.PI * (1 - t)) / r);
      return 1;
    }
  }
}

export interface WindowTable {
  table: Float32Array; // WINDOW_SIZE + 1 points
  meanW: number; // mean of w
  meanW2: number; // mean of w²
}

const cache = new Map<WindowType, WindowTable>();

export function windowTable(type: WindowType): WindowTable {
  let t = cache.get(type);
  if (!t) {
    const table = new Float32Array(WINDOW_SIZE + 1);
    let s1 = 0;
    let s2 = 0;
    for (let i = 0; i <= WINDOW_SIZE; i++) {
      const v = windowValue(type, i / WINDOW_SIZE);
      table[i] = v;
      if (i < WINDOW_SIZE) {
        s1 += v;
        s2 += v * v;
      }
    }
    t = { table, meanW: s1 / WINDOW_SIZE, meanW2: s2 / WINDOW_SIZE };
    cache.set(type, t);
  }
  return t;
}
