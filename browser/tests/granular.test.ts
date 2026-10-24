/** Rendering tests for the granular engine (Mode B). */
import { describe, expect, it } from 'vitest';
import { MorphCore } from '../src/dsp/core';
import { defaultSegment, equalSpacing } from '../src/dsp/timeline';
import type { GranularParams } from '../src/dsp/engines/granular';
import { hasNonFinite, maxSecondDiff, noise, peak, render, rms, settingsFor, sine, SR } from './helpers';

function granularCore(sources: Float32Array[], duration: number, params: Partial<GranularParams> = {}): MorphCore {
  const core = new MorphCore(SR);
  sources.forEach((s, i) => core.setSample(i, [s], settingsFor(s.length)));
  core.setTimeline({ order: sources.map((_, i) => i), positions: equalSpacing(sources.length), segments: sources.slice(1).map(defaultSegment) });
  core.setDuration(duration);
  core.setEngine('granular');
  core.setGranularParams(params);
  return core;
}

describe('granular engine', () => {
  it('chooses grain sources with the timeline probabilities', () => {
    const core = granularCore([noise(1, 0.3, 1), noise(1, 0.3, 2)], 100, { sizeMs: 50, overlap: 4 });
    core.seek(0.3);
    core.setHold(true);
    core.play();
    render(core, 20);
    const [a, b] = core.granular.spawnCounts;
    const share = b / (a + b);
    expect(a + b).toBeGreaterThan(1500);
    expect(share).toBeGreaterThan(0.27);
    expect(share).toBeLessThan(0.33);
  });

  it('starts 100 % on A and ends 100 % on B', () => {
    const core = granularCore([sine(220, 1), sine(330, 1)], 100);
    core.setHold(true);
    core.play();
    render(core, 2);
    expect(core.granular.spawnCounts[1]).toBe(0);
    core.seek(1);
    render(core, 0.6); // let the seek glide finish
    core.granular.reset();
    render(core, 2);
    expect(core.granular.spawnCounts[0]).toBe(0);
  });

  it('is click-free with sine sources across a full morph', () => {
    for (const window of ['hann', 'tukey', 'gaussian', 'trapezoid'] as const) {
      // Quiet sources so the safety limiter never engages: random-phase
      // sums of a pure sine can peak well above the source level.
      const core = granularCore([sine(220, 1.13, 0.2), sine(330, 0.97, 0.2)], 4, { window, sizeMs: 80, positionRandom: 0.3, spread: 0 });
      core.play();
      const { L } = render(core, 4);
      expect(hasNonFinite(L)).toBe(false);
      expect(peak(L)).toBeLessThan(0.85);
      const ref = peak(L) * (2 * Math.PI * 330 / SR) ** 2;
      // Grain windows add curvature but no steps; a raw grain edge would be
      // orders of magnitude above this.
      expect(maxSecondDiff(L, 2000)).toBeLessThan(ref * 3);
    }
  });

  it('sustains a continuous texture from a short sample for a long time', () => {
    const core = granularCore([noise(0.4, 0.3, 7), noise(0.6, 0.3, 8)], 60, { sizeMs: 150, positionRandom: 0.5 });
    core.play();
    const { L } = render(core, 60);
    const w = Math.round(0.05 * SR);
    const levels: number[] = [];
    for (let a = SR; a + w <= L.length; a += w) levels.push(rms(L, a, a + w));
    const mean = levels.reduce((x, y) => x + y) / levels.length;
    expect(Math.min(...levels)).toBeGreaterThan(mean * 0.5); // no gaps
    expect(Math.max(...levels)).toBeLessThan(mean * 1.7); // no bursts
    expect(peak(L)).toBeLessThan(0.89);
  });

  it('keeps loudness close to crossfade mode across grain settings', () => {
    const src = [noise(1.3, 0.3, 11), noise(1.1, 0.3, 12)];
    const ref = new MorphCore(SR);
    src.forEach((s, i) => ref.setSample(i, [s], settingsFor(s.length)));
    ref.setTimeline({ order: [0, 1], positions: [0, 1], segments: [defaultSegment()] });
    ref.play();
    const refRms = rms(render(ref, 3).L, SR);
    const cases: Partial<GranularParams>[] = [
      { sizeMs: 20, overlap: 2, positionRandom: 0 },
      { sizeMs: 120, overlap: 4, positionRandom: 0.05 },
      { sizeMs: 400, overlap: 8, positionRandom: 0.5 },
      { sizeMs: 60, overlap: 16, positionRandom: 1, pitchRandomCents: 300 },
      { sizeMs: 250, overlap: 1, positionRandom: 0.2, window: 'tukey' },
      { sizeMs: 100, overlap: 4, density: 0.5, positionRandom: 0.3 },
    ];
    for (const c of cases) {
      const core = granularCore(src, 600, c);
      core.play();
      const level = rms(render(core, 3).L, SR);
      const db = 20 * Math.log10(level / refRms);
      expect(Math.abs(db), JSON.stringify(c) + ` → ${db.toFixed(1)} dB`).toBeLessThan(4.5);
    }
  });

  it('stays under the limiter with typical material at default settings', () => {
    // ≈ −18 dBFS RMS sources and the app's default −3 dB output level
    const core = granularCore([noise(1.3, 0.22, 21), noise(0.9, 0.22, 22)], 10);
    core.setMasterGain(Math.pow(10, -3 / 20));
    core.play();
    const { L } = render(core, 10);
    expect(peak(L)).toBeLessThan(0.85);
  });

  it('freeze keeps the cloud going and holds the scan position', () => {
    const core = granularCore([noise(1, 0.3, 3), noise(1, 0.3, 4)], 30, { positionRandom: 0 });
    core.seek(0.5);
    core.play();
    render(core, 1);
    core.setHold(true);
    const pos = core.clock.position;
    const { L } = render(core, 3);
    expect(core.clock.position).toBe(pos);
    expect(rms(L, SR)).toBeGreaterThan(0.05);
  });

  it('the same seed reproduces the same texture; a new seed changes it', () => {
    const make = (seed: number) => {
      const core = granularCore([noise(1, 0.3, 3), sine(300, 1)], 20, { seed, positionRandom: 0.4, pitchRandomCents: 200 });
      core.play();
      return render(core, 1).L;
    };
    const a = make(5);
    const b = make(5);
    const c = make(6);
    expect(Array.from(a.subarray(20000, 20100))).toEqual(Array.from(b.subarray(20000, 20100)));
    expect(Array.from(a.subarray(20000, 20100))).not.toEqual(Array.from(c.subarray(20000, 20100)));
  });

  it('survives extreme settings without NaN, overload or pool exhaustion errors', () => {
    const core = granularCore([sine(220, 0.05), noise(0.03, 0.3, 9)], 5, {
      sizeMs: 10, overlap: 16, pitchRandomCents: 1200, transpose: 24, speed: -2, spread: 1, positionRandom: 1, jitter: 1,
    });
    core.play();
    const { L, R } = render(core, 5);
    expect(hasNonFinite(L) || hasNonFinite(R)).toBe(false);
    expect(peak(L)).toBeLessThanOrEqual(0.8913);
    core.setGranularParams({ sizeMs: 500, overlap: 16, transpose: -24 });
    const out = render(core, 3);
    expect(hasNonFinite(out.L)).toBe(false);
  });

  it('switching engines while playing is click-free', () => {
    const core = granularCore([sine(220, 1.13), sine(330, 0.97)], 60, { sizeMs: 200, overlap: 4, positionRandom: 0, spread: 0 });
    core.setEngine('crossfade');
    core.play();
    const { L } = render(core, 3, (b) => {
      if (b === Math.round(SR / 128)) core.setEngine('granular');
      if (b === Math.round((2 * SR) / 128)) core.setEngine('crossfade');
    });
    const ref = 0.5 * (2 * Math.PI * 330 / SR) ** 2;
    expect(maxSecondDiff(L, 2000)).toBeLessThan(ref * 4);
  });

  it('reverse slots read backwards in granular mode too', () => {
    const ramp = new Float32Array(SR).map((_, i) => i / SR - 0.5);
    const core = new MorphCore(SR);
    core.setSample(0, [ramp], settingsFor(SR, { reverse: true }));
    core.setTimeline({ order: [0], positions: [0], segments: [] });
    core.setEngine('granular');
    core.setGranularParams({ sizeMs: 200, overlap: 1, jitter: 0, positionRandom: 0, spread: 0 });
    core.play();
    const { L } = render(core, 0.3);
    // During the first grain the ramp must be descending (reversed).
    const i = Math.round(0.1 * SR);
    expect(L[i + 100] - L[i]).toBeLessThan(0);
  });
});
