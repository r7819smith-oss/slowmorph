import { describe, expect, it } from 'vitest';
import { computeFractions, defaultSegment, equalSpacing, moveNode, type TimelineModel } from '../src/dsp/timeline';

function model(n: number, overrides: Partial<TimelineModel> = {}): TimelineModel {
  return {
    order: Array.from({ length: n }, (_, i) => i),
    positions: equalSpacing(n),
    segments: Array.from({ length: Math.max(0, n - 1) }, defaultSegment),
    ...overrides,
  };
}

describe('timeline', () => {
  it('equal spacing spans 0..1', () => {
    expect(equalSpacing(3)).toEqual([0, 0.5, 1]);
    expect(equalSpacing(1)).toEqual([0]);
    expect(equalSpacing(0)).toEqual([]);
  });

  it('A → B → C with linear transitions', () => {
    const out = new Float32Array(8);
    const m = model(3);
    computeFractions(m, 0, out);
    expect(Array.from(out.slice(0, 3))).toEqual([1, 0, 0]);
    computeFractions(m, 0.25, out);
    expect(out[0]).toBeCloseTo(0.5);
    expect(out[1]).toBeCloseTo(0.5);
    computeFractions(m, 0.5, out);
    expect(out[1]).toBeCloseTo(1);
    computeFractions(m, 0.75, out);
    expect(out[1]).toBeCloseTo(0.5);
    expect(out[2]).toBeCloseTo(0.5);
    computeFractions(m, 1, out);
    expect(out[2]).toBeCloseTo(1);
  });

  it('fractions always sum to 1 and change continuously', () => {
    const out = new Float32Array(8);
    const m = model(5, {
      positions: [0.1, 0.2, 0.55, 0.6, 0.9],
      segments: [
        { curve: 'sCurve', shape: 0.9, tStart: 0, tEnd: 1 },
        { curve: 'easeIn', shape: 0.5, tStart: 0.3, tEnd: 0.7 },
        { curve: 'easeOut', shape: 1, tStart: 0, tEnd: 0.2 },
        { curve: 'linear', shape: 0, tStart: 0.5, tEnd: 1 },
      ],
    });
    let prev: number[] | null = null;
    const steps = 20000;
    for (let i = 0; i <= steps; i++) {
      computeFractions(m, i / steps, out);
      const sum = out.reduce((a, b) => a + b, 0);
      expect(sum).toBeCloseTo(1, 5);
      if (prev) {
        // A single 1/20000 step should never move a fraction by more than
        // the steepest curve allows (easeOut k=5 over a 0.01-wide window).
        for (let s = 0; s < 5; s++) expect(Math.abs(out[s] - prev[s])).toBeLessThan(0.03);
      }
      prev = Array.from(out);
    }
  });

  it('transition windows hold each sample outside the window', () => {
    const out = new Float32Array(8);
    const m = model(2, { segments: [{ curve: 'linear', shape: 0, tStart: 0.4, tEnd: 0.6 }] });
    computeFractions(m, 0.3, out);
    expect(out[0]).toBe(1);
    computeFractions(m, 0.5, out);
    expect(out[0]).toBeCloseTo(0.5);
    computeFractions(m, 0.7, out);
    expect(out[1]).toBe(1);
  });

  it('uses slot indices from order (empty slots skipped)', () => {
    const out = new Float32Array(8);
    computeFractions({ order: [0, 3], positions: [0, 1], segments: [defaultSegment()] }, 0.5, out);
    expect(out[0]).toBeCloseTo(0.5);
    expect(out[1]).toBe(0);
    expect(out[3]).toBeCloseTo(0.5);
  });

  it('before the first and after the last node play a single sample', () => {
    const out = new Float32Array(8);
    const m = model(2, { positions: [0.3, 0.7] });
    computeFractions(m, 0.1, out);
    expect(out[0]).toBe(1);
    computeFractions(m, 0.95, out);
    expect(out[1]).toBe(1);
  });

  it('moveNode keeps nodes ordered and inside 0..1', () => {
    const p = [0, 0.5, 1];
    expect(moveNode(p, 1, 2)[1]).toBeLessThan(1);
    expect(moveNode(p, 1, -1)[1]).toBeGreaterThan(0);
    expect(moveNode(p, 0, 0.2)[0]).toBeCloseTo(0.2);
    expect(moveNode(p, 2, 0.4)[2]).toBeGreaterThan(0.5);
  });
});
