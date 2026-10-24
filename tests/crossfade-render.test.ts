/**
 * Audio-rendering tests for the core + crossfade engine. They render real
 * audio and look for clicks, clipping, silence and level instability.
 */
import { describe, expect, it } from 'vitest';
import { MorphCore } from '../src/dsp/core';
import { defaultSegment, equalSpacing } from '../src/dsp/timeline';
import { hasNonFinite, maxSecondDiff, noise, peak, render, rms, settingsFor, sine, SR } from './helpers';

function coreWith(sources: Float32Array[], duration: number): MorphCore {
  const core = new MorphCore(SR);
  sources.forEach((s, i) => core.setSample(i, [s], settingsFor(s.length)));
  core.setTimeline({
    order: sources.map((_, i) => i),
    positions: equalSpacing(sources.length),
    segments: sources.slice(1).map(defaultSegment),
  });
  core.setDuration(duration);
  return core;
}

describe('crossfade engine rendering', () => {
  it('morphs A → B → C: correct sample dominates at start, middle and end', () => {
    const sources = [sine(220, 1.13), sine(330, 0.97), sine(495, 1.31)];
    const core = coreWith(sources, 6);
    core.play();
    const { L } = render(core, 6.2);
    const freqAt = (t: number) => {
      // zero-crossing frequency estimate over 0.2 s
      const a = Math.floor(t * SR);
      let z = 0;
      for (let i = a + 1; i < a + SR * 0.2; i++) if (L[i - 1] <= 0 && L[i] > 0) z++;
      return z / 0.2;
    };
    expect(Math.abs(freqAt(0.1) - 220)).toBeLessThan(12);
    expect(Math.abs(freqAt(2.9) - 330)).toBeLessThan(12);
    expect(Math.abs(freqAt(5.9) - 495)).toBeLessThan(12);
  });

  it('produces no clicks with sine sources (loops + transitions)', () => {
    const sources = [sine(220, 1.13), sine(330, 0.97), sine(495, 1.31)];
    const core = coreWith(sources, 8);
    core.play();
    const { L, R } = render(core, 8);
    expect(hasNonFinite(L)).toBe(false);
    // Smooth reference: the highest sine's own second difference at amp 0.5
    const ref = 0.5 * (2 * Math.PI * 495 / SR) ** 2;
    // skip the 30 ms fade-in
    expect(maxSecondDiff(L, Math.round(0.04 * SR))).toBeLessThan(ref * 2);
    expect(maxSecondDiff(R, Math.round(0.04 * SR))).toBeLessThan(ref * 2);
  });

  it('keeps a stable level across equal-power morphs of uncorrelated noise', () => {
    const sources = [noise(1.5, 0.3, 1), noise(2.1, 0.3, 2), noise(1.7, 0.3, 3)];
    const core = coreWith(sources, 10);
    core.play();
    const { L } = render(core, 10);
    const w = Math.round(0.25 * SR);
    const levels: number[] = [];
    for (let a = w; a + w <= L.length; a += w) levels.push(rms(L, a, a + w));
    const mean = levels.reduce((a, b) => a + b) / levels.length;
    for (const l of levels) {
      expect(20 * Math.log10(l / mean)).toBeGreaterThan(-1.5);
      expect(20 * Math.log10(l / mean)).toBeLessThan(1.5);
    }
    expect(peak(L)).toBeLessThan(0.9);
  });

  it('never exceeds the safety ceiling even with hot sources', () => {
    const sources = [sine(100, 1, 1), sine(150, 1, 1)];
    const core = coreWith(sources, 5);
    core.setSlotSettings(0, settingsFor(SR, { gain: 4 }));
    core.setMasterGain(4);
    core.play();
    const { L } = render(core, 3);
    expect(peak(L)).toBeLessThanOrEqual(0.8913);
  });

  it('never goes silent while playing (no gaps at loop seams or transitions)', () => {
    const sources = [noise(1.0, 0.3, 4), noise(1.0, 0.3, 5)];
    const core = coreWith(sources, 5);
    core.play();
    const { L } = render(core, 5);
    const w = Math.round(0.01 * SR);
    for (let a = Math.round(0.05 * SR); a + w <= L.length; a += w) expect(rms(L, a, a + w)).toBeGreaterThan(0.05);
  });

  it('pause, play, stop, seek and reverse are click-free', () => {
    const sources = [sine(220, 1.13), sine(330, 0.97)];
    const core = coreWith(sources, 6);
    core.play();
    const { L } = render(core, 6, (b, t) => {
      if (b === Math.round((1 * SR) / 128)) core.pause();
      if (b === Math.round((1.5 * SR) / 128)) core.play();
      if (b === Math.round((2 * SR) / 128)) core.seek(1);
      if (b === Math.round((2.5 * SR) / 128)) core.setDirection(-1);
      if (b === Math.round((3 * SR) / 128)) core.seek(0.2);
      if (b === Math.round((4 * SR) / 128)) core.stop();
      if (b === Math.round((4.5 * SR) / 128)) core.play();
      void t;
    });
    const ref = 0.5 * (2 * Math.PI * 330 / SR) ** 2;
    expect(maxSecondDiff(L)).toBeLessThan(ref * 3);
  });

  it('replacing a sample while it is audible is click-free', () => {
    const sources = [sine(220, 1.13), sine(330, 0.97)];
    const core = coreWith(sources, 60);
    core.play();
    const { L } = render(core, 2, (b) => {
      if (b === Math.round((1 * SR) / 128)) {
        const s = sine(277, 0.8);
        core.setSample(0, [s], settingsFor(s.length));
      }
    });
    const ref = 0.5 * (2 * Math.PI * 330 / SR) ** 2;
    // An 8 ms declick fade adds some curvature; a raw swap would be ~100× ref.
    expect(maxSecondDiff(L)).toBeLessThan(ref * 4);
    // and the new sample is audible afterwards
    expect(rms(L, Math.round(1.5 * SR), Math.round(1.6 * SR))).toBeGreaterThan(0.2);
  });

  it('mute and solo fade rather than cut', () => {
    const sources = [sine(220, 1.13), sine(330, 0.97)];
    const core = coreWith(sources, 600);
    core.seek(0.5);
    core.play();
    const { L } = render(core, 2, (b) => {
      if (b === Math.round((0.5 * SR) / 128)) core.setSlotSettings(0, settingsFor(sources[0].length, { mute: true }));
      if (b === Math.round((1 * SR) / 128)) core.setSlotSettings(1, settingsFor(sources[1].length, { solo: true }));
    });
    const ref = 0.5 * (2 * Math.PI * 330 / SR) ** 2;
    expect(maxSecondDiff(L, Math.round(0.1 * SR))).toBeLessThan(ref * 3);
  });

  it('a looping morph glides back to the first sample instead of jumping', () => {
    const sources = [sine(220, 1.13), sine(330, 0.97)];
    const core = coreWith(sources, 5);
    core.setPlayMode('loop');
    core.play();
    const { L } = render(core, 7);
    const ref = 0.5 * (2 * Math.PI * 330 / SR) ** 2;
    expect(maxSecondDiff(L, Math.round(0.05 * SR))).toBeLessThan(ref * 2);
  });

  it('stopped output is silent and stop resets the position', () => {
    const core = coreWith([sine(220, 1), sine(330, 1)], 10);
    core.play();
    render(core, 1);
    core.stop();
    const { L } = render(core, 0.5);
    expect(rms(L, Math.round(0.05 * SR))).toBe(0);
    expect(core.clock.position).toBe(0);
  });
});
