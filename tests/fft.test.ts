import { describe, expect, it } from 'vitest';
import { FFT } from '../src/dsp/fft';

describe('FFT', () => {
  it('finds a sine in the right bin and round-trips', () => {
    const n = 1024;
    const fft = new FFT(n);
    const re = new Float64Array(n);
    const im = new Float64Array(n);
    for (let i = 0; i < n; i++) re[i] = Math.cos((2 * Math.PI * 37 * i) / n);
    const orig = re.slice();
    fft.transform(re, im);
    expect(Math.hypot(re[37], im[37])).toBeCloseTo(n / 2, 6);
    expect(Math.hypot(re[38], im[38])).toBeLessThan(1e-6);
    fft.transform(re, im, true);
    for (let i = 0; i < n; i++) expect(re[i]).toBeCloseTo(orig[i], 9);
  });

  it('matches a direct DFT on random data', () => {
    const n = 64;
    const fft = new FFT(n);
    const x = Array.from({ length: n }, (_, i) => Math.sin(i * 1.7) + Math.cos(i * 0.3));
    const re = Float64Array.from(x);
    const im = new Float64Array(n);
    fft.transform(re, im);
    for (const k of [0, 1, 5, 31, 63]) {
      let dr = 0, di = 0;
      for (let t = 0; t < n; t++) {
        dr += x[t] * Math.cos((-2 * Math.PI * k * t) / n);
        di += x[t] * Math.sin((-2 * Math.PI * k * t) / n);
      }
      expect(re[k]).toBeCloseTo(dr, 9);
      expect(im[k]).toBeCloseTo(di, 9);
    }
  });

  it('rejects sizes that are not powers of two', () => {
    expect(() => new FFT(1000)).toThrow();
  });
});
