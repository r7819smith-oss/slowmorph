import { describe, expect, it } from 'vitest';
import { applyCurve, CURVE_TYPES, dbToGain, fractionToGain, gainToDb } from '../src/dsp/curves';

describe('transition curves', () => {
  for (const type of CURVE_TYPES) {
    it(`${type} maps 0→0, 1→1 and is monotonic`, () => {
      for (const shape of [0, 0.3, 0.5, 1]) {
        expect(applyCurve(0, type, shape)).toBeCloseTo(0, 9);
        expect(applyCurve(1, type, shape)).toBeCloseTo(1, 9);
        let prev = -1;
        for (let i = 0; i <= 1000; i++) {
          const v = applyCurve(i / 1000, type, shape);
          expect(v).toBeGreaterThanOrEqual(prev);
          prev = v;
        }
      }
    });
  }

  it('clamps out-of-range input', () => {
    expect(applyCurve(-1, 'sCurve')).toBe(0);
    expect(applyCurve(2, 'easeIn')).toBe(1);
  });

  it('shape 0 is linear for every curve', () => {
    for (const type of CURVE_TYPES) expect(applyCurve(0.3, type, 0)).toBeCloseTo(0.3, 9);
  });

  it('s-curve is symmetric about the midpoint', () => {
    expect(applyCurve(0.5, 'sCurve', 0.8)).toBeCloseTo(0.5, 9);
    expect(applyCurve(0.2, 'sCurve', 0.8) + applyCurve(0.8, 'sCurve', 0.8)).toBeCloseTo(1, 9);
  });
});

describe('gain laws', () => {
  it('equal-power keeps summed power constant', () => {
    for (let i = 0; i <= 100; i++) {
      const f = i / 100;
      const a = fractionToGain(1 - f, 'equalPower');
      const b = fractionToGain(f, 'equalPower');
      expect(a * a + b * b).toBeCloseTo(1, 9);
    }
  });
  it('linear keeps summed amplitude constant', () => {
    expect(fractionToGain(0.25, 'linear') + fractionToGain(0.75, 'linear')).toBeCloseTo(1, 9);
  });
  it('dB conversions round-trip', () => {
    for (const db of [-40, -6, 0, 6]) expect(gainToDb(dbToGain(db))).toBeCloseTo(db, 6);
    expect(dbToGain(-120)).toBe(0);
  });
});
