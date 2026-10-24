/** Rendering tests for the spectral engine (Mode C). */
import { describe, expect, it } from 'vitest';
import { MorphCore } from '../src/dsp/core';
import { defaultSegment, equalSpacing } from '../src/dsp/timeline';
import type { SpectralParams } from '../src/dsp/engines/spectral';
import {
  colouredNoise, energyNear, hasNonFinite, maxSecondDiff, peak, peakFrequency, render, rms, settingsFor, sine, spectrum, SR,
} from './helpers';

function spectralCore(sources: Float32Array[], duration: number, params: Partial<SpectralParams> = {}): MorphCore {
  const core = new MorphCore(SR);
  sources.forEach((s, i) => core.setSample(i, [s], settingsFor(s.length)));
  core.setTimeline({ order: sources.map((_, i) => i), positions: equalSpacing(sources.length), segments: sources.slice(1).map(defaultSegment) });
  core.setDuration(duration);
  core.setEngine('spectral');
  core.setSpectralParams({ width: 0, blur: 0, ...params });
  return core;
}

function partials(f0: number, seconds: number, n: number, amp = 0.3): Float32Array {
  const len = Math.round(seconds * SR);
  const out = new Float32Array(len);
  for (let h = 1; h <= n; h++) {
    const f = f0 * h;
    for (let i = 0; i < len; i++) out[i] += (amp / h) * Math.sin((2 * Math.PI * f * i) / SR);
  }
  return out;
}

describe('spectral engine', () => {
  it('resynthesises a single source faithfully at every FFT size', () => {
    for (const fftSize of [512, 1024, 2048, 4096, 8192] as const) {
      for (const overlap of [4, 8] as const) {
        const src = sine(441, 2, 0.3);
        const core = spectralCore([src], 60, { fftSize, overlap, interp: 0.5 });
        core.play();
        const { L } = render(core, 2.5);
        const from = SR; // after latency
        expect(Math.abs(peakFrequency(L, from) - 441)).toBeLessThan(6);
        expect(energyNear(L, from, [441], 30)).toBeGreaterThan(0.97);
        const db = 20 * Math.log10(rms(L, from, from + SR) / rms(src));
        expect(Math.abs(db), `${fftSize}/${overlap}: ${db.toFixed(2)} dB`).toBeLessThan(1);
      }
    }
  });

  it('separates timbre (envelope) from partials (fine structure)', () => {
    // A: harmonic tone on 300 Hz, B: harmonic tone on 520 Hz. Half-way, with
    // the envelope leading, the partials still come from A (shaped by B's
    // envelope); with the envelope lagging, the partials come from B.
    const a = partials(300, 2, 8);
    const b = partials(520, 2, 8);
    const harm = (f0: number) => Array.from({ length: 30 }, (_, i) => f0 * (i + 1)).filter((f) => f < 12000);
    const shares = (lead: number) => {
      const core = spectralCore([a, b], 600, { interp: 1, envelopeLead: lead });
      core.seek(0.5);
      core.play();
      const { L } = render(core, 1.5);
      const from = Math.round(0.5 * SR);
      return { a: energyNear(L, from, harm(300), 12), b: energyNear(L, from, harm(520), 12) };
    };
    const lead = shares(1);
    const lag = shares(-1);
    expect(lead.a).toBeGreaterThan(0.8);
    expect(lag.b).toBeGreaterThan(0.8);
  });

  it('is a single interpolated spectrum, not a mix: phase-inverted copies do not cancel', () => {
    // B = −A. A linear crossfade cancels to silence at the midpoint; a
    // spectral morph has only one phase per bin, so the level holds.
    const a = partials(220, 2, 6);
    const b = a.map((v) => -v);
    const xf = new MorphCore(SR);
    xf.setSample(0, [a], settingsFor(a.length));
    xf.setSample(1, [b], settingsFor(b.length));
    xf.setTimeline({ order: [0, 1], positions: [0, 1], segments: [defaultSegment()] });
    xf.setGainLaw('linear');
    xf.setDuration(600);
    xf.seek(0.5);
    xf.play();
    const cross = rms(render(xf, 1).L, SR / 2);
    const sp = spectralCore([a, b], 600, { interp: 0 });
    sp.seek(0.5);
    sp.play();
    const spec = rms(render(sp, 1.5).L, SR);
    expect(cross).toBeLessThan(rms(a) * 0.01);
    expect(20 * Math.log10(spec / rms(a))).toBeGreaterThan(-1.5);
  });

  it('keeps a stable level while morphing between very different spectra', () => {
    const a = colouredNoise(1.7, 0.97, 0.3, 1); // dark
    const b = colouredNoise(1.3, -0.9, 0.3, 2); // bright
    for (const interp of [0, 0.5, 1]) {
      const core = spectralCore([a, b], 8, { interp });
      core.play();
      const { L } = render(core, 8.3);
      const w = Math.round(0.25 * SR);
      const lv: number[] = [];
      for (let s = Math.round(0.5 * SR); s + w <= L.length; s += w) lv.push(rms(L, s, s + w));
      const mean = lv.reduce((x, y) => x + y) / lv.length;
      for (const l of lv) expect(Math.abs(20 * Math.log10(l / mean)), `interp ${interp}`).toBeLessThan(2);
      expect(peak(L)).toBeLessThan(0.89);
    }
  });

  it('really changes timbre: the midpoint spectrum is between the sources, not their sum', () => {
    const a = colouredNoise(2, 0.97, 0.3, 3);
    const b = colouredNoise(2, -0.9, 0.3, 4);
    const centroid = (x: Float32Array, from: number) => {
      const s = spectrum(x, from);
      let num = 0, den = 0;
      for (let k = 1; k < s.length; k++) { num += k * s[k] * s[k]; den += s[k] * s[k]; }
      return ((num / den) * SR) / 8192;
    };
    const run = (pos: number) => {
      const core = spectralCore([a, b], 600, { interp: 1 });
      core.seek(pos);
      core.play();
      const { L } = render(core, 1.5);
      return centroid(L, SR);
    };
    const c0 = run(0);
    const c5 = run(0.5);
    const c1 = run(1);
    expect(c0).toBeLessThan(c5);
    expect(c5).toBeLessThan(c1);
  });

  it('is click-free across a full morph with tonal sources', () => {
    const a = partials(220, 1.13, 4, 0.2);
    const b = partials(330, 0.97, 4, 0.2);
    const core = spectralCore([a, b], 5, { interp: 0.5, overlap: 8 });
    core.play();
    const { L } = render(core, 5.3);
    expect(hasNonFinite(L)).toBe(false);
    const ref = peak(L) * (2 * Math.PI * 1320 / SR) ** 2;
    expect(maxSecondDiff(L, SR / 2)).toBeLessThan(ref * 1.5);
  });

  it('freeze sustains the current hybrid spectrum', () => {
    const a = colouredNoise(1, 0.97, 0.3, 5);
    const b = partials(250, 1, 8);
    const core = spectralCore([a, b], 30, { interp: 0.7 });
    core.seek(0.5);
    core.play();
    render(core, 1);
    core.setHold(true);
    const frames = core.spectral.framesComputed;
    const { L } = render(core, 4);
    expect(core.spectral.framesComputed).toBe(frames); // no new analysis while frozen
    const s1 = spectrum(L, SR);
    const s2 = spectrum(L, 3 * SR);
    let dot = 0, n1 = 0, n2 = 0;
    for (let k = 0; k < s1.length; k++) { dot += s1[k] * s2[k]; n1 += s1[k] ** 2; n2 += s2[k] ** 2; }
    expect(dot / Math.sqrt(n1 * n2)).toBeGreaterThan(0.95);
    expect(Math.abs(20 * Math.log10(rms(L, SR, 2 * SR) / rms(L, 3 * SR, 4 * SR)))).toBeLessThan(1);
  });

  it('changing FFT size while playing is click-free', () => {
    const core = spectralCore([sine(300, 1.3, 0.2), sine(450, 1.1, 0.2)], 60);
    core.play();
    const { L } = render(core, 3, (blk) => {
      if (blk === Math.round(SR / 128)) core.setSpectralParams({ fftSize: 4096 });
      if (blk === Math.round((2 * SR) / 128)) core.setSpectralParams({ fftSize: 1024, overlap: 8 });
    });
    expect(hasNonFinite(L)).toBe(false);
    const ref = 0.2 * (2 * Math.PI * 450 / SR) ** 2;
    expect(maxSecondDiff(L, SR / 4)).toBeLessThan(ref * 3);
  });

  it('blur, smoothing, phase randomisation and width stay finite and bounded', () => {
    const core = spectralCore([colouredNoise(1, 0.9, 0.3, 6), partials(180, 1, 10)], 5, {
      blur: 1, smoothing: 1, phaseRandom: 1, width: 1, interp: 1, envelopeLead: 1, speed: -1.5,
    });
    core.play();
    const { L, R } = render(core, 5);
    expect(hasNonFinite(L) || hasNonFinite(R)).toBe(false);
    expect(rms(L, SR)).toBeGreaterThan(0.01);
    expect(peak(L)).toBeLessThanOrEqual(0.8913);
  });

  it('runs comfortably faster than real time', () => {
    const a = colouredNoise(1.5, 0.9, 0.3, 7);
    const b = partials(200, 1.2, 10);
    const results: string[] = [];
    for (const [fftSize, overlap] of [[2048, 4], [4096, 8], [8192, 8]] as const) {
      const core = spectralCore([a, b], 10, { fftSize, overlap, width: 0.5 });
      core.seek(0.5);
      core.play();
      render(core, 0.5); // warm-up
      const t0 = performance.now();
      render(core, 4);
      const ms = performance.now() - t0;
      const load = ms / 4000;
      results.push(`${fftSize}/${overlap}: ${(load * 100).toFixed(1)}% of real time`);
      expect(load).toBeLessThan(0.5);
    }
    console.log(results.join(', '));
  });
});
