/** Phase 4: Character/Hybrid, drift and the effects section. */
import { describe, expect, it } from 'vitest';
import { MorphCore, type EngineId } from '../src/dsp/core';
import { granularWithCharacter, spectralWithCharacter } from '../src/dsp/character';
import { defaultGranularParams } from '../src/dsp/engines/granular';
import { defaultSpectralParams } from '../src/dsp/engines/spectral';
import { defaultSegment } from '../src/dsp/timeline';
import { Effects } from '../src/dsp/effects';
import { colouredNoise, hasNonFinite, maxSecondDiff, noise, peak, render, rms, settingsFor, sine, spectrum, SR } from './helpers';

function core2(a: Float32Array, b: Float32Array, engine: EngineId, duration = 600): MorphCore {
  const core = new MorphCore(SR);
  core.setSample(0, [a], settingsFor(a.length));
  core.setSample(1, [b], settingsFor(b.length));
  core.setTimeline({ order: [0, 1], positions: [0, 1], segments: [defaultSegment()] });
  core.setDuration(duration);
  core.setEngine(engine);
  return core;
}

describe('character mapping', () => {
  it('is the identity at 0 and moves documented parameters toward strange at 1', () => {
    const gb = defaultGranularParams();
    const g = { ...gb };
    granularWithCharacter(gb, 0, g);
    expect(g).toEqual(gb);
    granularWithCharacter(gb, 1, g);
    expect(g.sizeMs).toBeLessThan(gb.sizeMs);
    expect(g.positionRandom).toBeGreaterThan(gb.positionRandom);
    expect(g.pitchRandomCents).toBeGreaterThan(gb.pitchRandomCents);
    expect(g.jitter).toBeGreaterThan(gb.jitter);
    expect(g.density).toBe(gb.density);

    const sb = defaultSpectralParams();
    const s = { ...sb };
    spectralWithCharacter(sb, 0, s);
    expect(s).toEqual(sb);
    spectralWithCharacter(sb, 1, s);
    expect(s.interp).toBe(1);
    expect(s.blur).toBeGreaterThan(sb.blur);
    expect(s.phaseRandom).toBeGreaterThan(0);
    expect(s.warp).toBeGreaterThan(0);
  });

  for (const engine of ['crossfade', 'granular', 'spectral'] as const) {
    it(`${engine}: sweeping Character 0 → 100 % keeps the level steady`, () => {
      const core = core2(colouredNoise(1.3, 0.9, 0.3, 1), colouredNoise(1.1, 0.3, 0.3, 2), engine);
      core.seek(0.4);
      core.play();
      render(core, 0.5);
      const steps = 8;
      const levels: number[] = [];
      for (let i = 0; i <= steps; i++) {
        core.setCharacter(i / steps);
        const { L } = render(core, 0.75);
        levels.push(rms(L, Math.round(0.25 * SR)));
      }
      const db = levels.map((l) => 20 * Math.log10(l / levels[0]));
      for (const d of db) expect(Math.abs(d), `${engine}: ${db.map((x) => x.toFixed(1)).join(' ')}`).toBeLessThan(3.5);
    });
  }

  it('crossfade character changes are click-free on tonal material', () => {
    const core = core2(sine(220, 1.13, 0.3), sine(330, 0.97, 0.3), 'crossfade', 8);
    core.play();
    const { L } = render(core, 6, (b) => {
      if (b === Math.round(SR / 128)) core.setCharacter(1);
      if (b === Math.round((3 * SR) / 128)) core.setCharacter(0);
    });
    const ref = 0.3 * (2 * Math.PI * 330 / SR) ** 2;
    expect(maxSecondDiff(L, 2000)).toBeLessThan(ref * 2.5);
  });

  it('granular character interleaves neighbouring samples sooner', () => {
    const counts = (c: number) => {
      const core = core2(noise(1, 0.3, 3), noise(1, 0.3, 4), 'granular');
      core.setCharacter(c);
      core.seek(0.05);
      core.setHold(true);
      core.play();
      render(core, 0.5);
      core.granular.reset();
      render(core, 5);
      return core.granular.spawnCounts[1];
    };
    expect(counts(0)).toBeLessThan(counts(1));
  });
});

describe('drift', () => {
  it('wanders smoothly around the clock position within the amount', () => {
    const core = core2(noise(1, 0.3, 5), noise(1, 0.3, 6), 'crossfade', 600);
    core.seek(0.5);
    core.setHold(true);
    core.setDrift({ enabled: true, amount: 0.1, speed: 30, smoothness: 0.8 });
    core.play();
    const pos: number[] = [];
    render(core, 30, (b) => b > 0 && pos.push(core.effectivePosition));
    expect(core.clock.position).toBe(0.5);
    const lo = Math.min(...pos);
    const hi = Math.max(...pos);
    expect(lo).toBeGreaterThanOrEqual(0.4 - 1e-9);
    expect(hi).toBeLessThanOrEqual(0.6 + 1e-9);
    expect(hi - lo).toBeGreaterThan(0.05); // it actually moves
    let maxStep = 0;
    for (let i = 1; i < pos.length; i++) maxStep = Math.max(maxStep, Math.abs(pos[i] - pos[i - 1]));
    expect(maxStep).toBeLessThan(0.0005); // continuous: tiny per-block steps
  });

  it('turning drift off glides back instead of jumping', () => {
    const core = core2(noise(1, 0.3, 5), noise(1, 0.3, 6), 'crossfade', 600);
    core.seek(0.5);
    core.setHold(true);
    core.setDrift({ enabled: true, amount: 0.2, speed: 60, smoothness: 0.5 });
    core.play();
    render(core, 5);
    core.setDrift({ enabled: false });
    const pos: number[] = [];
    render(core, 4, () => pos.push(core.effectivePosition));
    let maxStep = 0;
    for (let i = 1; i < pos.length; i++) maxStep = Math.max(maxStep, Math.abs(pos[i] - pos[i - 1]));
    expect(maxStep).toBeLessThan(0.002);
    expect(Math.abs(pos.at(-1)! - 0.5)).toBeLessThan(0.001);
  });
});

describe('effects', () => {
  function run(fx: Effects, input: Float32Array, inputR = input) {
    const L = input.slice();
    const R = inputR.slice();
    for (let o = 0; o < L.length; o += 128) {
      const n = Math.min(128, L.length - o);
      fx.process(L.subarray(o, o + n), R.subarray(o, o + n), n);
    }
    return { L, R };
  }
  const band = (x: Float32Array, lo: number, hi: number) => {
    const s = spectrum(x, SR);
    let e = 0;
    for (let k = Math.round((lo * 8192) / SR); k < Math.round((hi * 8192) / SR); k++) e += s[k] * s[k];
    return e;
  };

  it('starts bypassed and is transparent when bypassed', () => {
    const fx = new Effects(SR);
    const x = noise(1, 0.3, 1);
    expect(Array.from(run(fx, x).L.subarray(0, 100))).toEqual(Array.from(x.subarray(0, 100)));
  });

  it('low-pass removes highs, high-pass removes lows', () => {
    const x = noise(2, 0.3, 2);
    const lp = new Effects(SR);
    lp.setParams({ enabled: true, lowpassHz: 500, reverbSend: 0, delaySend: 0, delayFeedback: 0 });
    const hp = new Effects(SR);
    hp.setParams({ enabled: true, highpassHz: 3000, reverbSend: 0, delaySend: 0, delayFeedback: 0 });
    const yl = run(lp, x).L;
    const yh = run(hp, x).L;
    expect(band(yl, 5000, 15000) / band(x, 5000, 15000)).toBeLessThan(0.01);
    expect(band(yh, 50, 500) / band(x, 50, 500)).toBeLessThan(0.01);
  });

  it('width 0 collapses to mono', () => {
    const fx = new Effects(SR);
    fx.setParams({ enabled: true, width: 0, reverbSend: 0, delaySend: 0, delayFeedback: 0 });
    // let the width smoother settle first
    run(fx, new Float32Array(SR), new Float32Array(SR));
    const { L, R } = run(fx, noise(0.5, 0.3, 3), noise(0.5, 0.3, 4));
    for (let i = 0; i < 1000; i++) expect(L[i]).toBeCloseTo(R[i], 6);
  });

  it('delay repeats at the set time and reverb leaves a tail', () => {
    const imp = new Float32Array(SR * 2);
    imp[100] = 1;
    const dl = new Effects(SR);
    dl.setParams({ enabled: true, delaySend: 1, delayTime: 0.25, delayFeedback: 0, reverbSend: 0 });
    // settle the glide toward 0.25 s before the impulse
    dl.setParams({});
    const warm = new Effects(SR);
    void warm;
    const { L } = run(dl, imp);
    let best = 0;
    for (let i = 200; i < L.length; i++) if (Math.abs(L[i]) > Math.abs(L[best])) best = i;
    expect(Math.abs((best - 100) / SR - 0.25)).toBeLessThan(0.01);

    const rv = new Effects(SR);
    rv.setParams({ enabled: true, reverbSend: 1, reverbSize: 0.9, delaySend: 0, delayFeedback: 0 });
    const out = run(rv, imp).L;
    expect(rms(out, SR, SR + 4800)).toBeGreaterThan(1e-4);
  });

  it('stays bounded with maximum feedback, resonance and sends', () => {
    const fx = new Effects(SR);
    fx.setParams({ enabled: true, delaySend: 1, delayFeedback: 1, delayTime: 0.05, reverbSend: 1, reverbSize: 1, resonance: 1, lowpassHz: 800, width: 2 });
    const { L, R } = run(fx, noise(10, 0.3, 5));
    expect(hasNonFinite(L) || hasNonFinite(R)).toBe(false);
    expect(peak(L)).toBeLessThan(20);
    expect(rms(L, 9 * SR)).toBeLessThan(3);
  });

  it('bypass toggling and cutoff sweeps are click-free in the core', () => {
    const core = core2(sine(220, 1.13, 0.3), sine(330, 0.97, 0.3), 'crossfade', 60);
    core.setEffects({ enabled: true, reverbSend: 0, delaySend: 0, delayFeedback: 0, lowpassHz: 20000 });
    core.play();
    const { L } = render(core, 4, (b) => {
      if (b === Math.round(SR / 128)) core.setEffects({ lowpassHz: 300 });
      if (b === Math.round((2 * SR) / 128)) core.setEffects({ enabled: false });
      if (b === Math.round((3 * SR) / 128)) core.setEffects({ enabled: true });
    });
    const ref = 0.3 * (2 * Math.PI * 330 / SR) ** 2;
    expect(maxSecondDiff(L, 2000)).toBeLessThan(ref * 2.5);
  });
});
