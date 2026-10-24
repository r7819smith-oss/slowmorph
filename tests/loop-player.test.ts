import { describe, expect, it } from 'vitest';
import { LoopPlayer } from '../src/dsp/loop-player';
import { maxSecondDiff, sine, SR } from './helpers';

function play(p: LoopPlayer, frames: number): Float32Array {
  const L = new Float32Array(frames);
  const R = new Float32Array(frames);
  for (let off = 0; off < frames; off += 128) p.process(L, R, off, Math.min(128, frames - off), 1, 1);
  return L;
}

describe('loop player', () => {
  // 440 Hz with a loop length that is not a whole number of cycles: a hard
  // loop would jump; the crossfaded loop must not.
  const src = sine(440, 0.5037);
  const smooth = maxSecondDiff(src); // ≈ (2π·440/48000)² · 0.5

  it('a hard loop of this sine would click (control)', () => {
    const p = new LoopPlayer();
    p.setBuffer([src]);
    p.setRegion({ trimStart: 0, loopStart: 0, loopEnd: src.length }, false, 0);
    expect(maxSecondDiff(play(p, SR * 3))).toBeGreaterThan(smooth * 10);
  });

  it('crossfaded loop seam stays smooth over many repetitions', () => {
    const p = new LoopPlayer();
    p.setBuffer([src]);
    p.setRegion({ trimStart: 0, loopStart: 0, loopEnd: src.length }, false, SR * 0.05);
    const out = play(p, SR * 10);
    expect(maxSecondDiff(out)).toBeLessThan(smooth * 2.5);
  });

  it('loops an inner region and starts from the trim start', () => {
    const ramp = new Float32Array(1000).map((_, i) => i / 1000);
    const p = new LoopPlayer();
    p.setBuffer([ramp]);
    p.setRegion({ trimStart: 100, loopStart: 400, loopEnd: 600 }, false, 0);
    p.rewind();
    const out = play(p, 1000);
    expect(out[0]).toBeCloseTo(0.1);
    for (let i = 600; i < 1000; i++) {
      expect(out[i]).toBeGreaterThanOrEqual(0.399);
      expect(out[i]).toBeLessThan(0.6);
    }
  });

  it('reverse playback reads the region backwards', () => {
    const ramp = new Float32Array(1000).map((_, i) => i / 1000);
    const p = new LoopPlayer();
    p.setBuffer([ramp]);
    p.setRegion({ trimStart: 0, loopStart: 0, loopEnd: 1000 }, true, 0);
    p.rewind();
    const out = play(p, 10);
    expect(out[0]).toBeCloseTo(0.999);
    expect(out[1]).toBeLessThan(out[0]);
  });

  it('skip() lands on the same position as playing', () => {
    const a = new LoopPlayer();
    const b = new LoopPlayer();
    for (const p of [a, b]) {
      p.setBuffer([src]);
      p.setRegion({ trimStart: 0, loopStart: 1000, loopEnd: 9000 }, false, 500);
      p.rewind();
    }
    play(a, 77777);
    for (let n = 0; n < 77777; n += 128) b.skip(Math.min(128, 77777 - n));
    expect(b.pos).toBe(a.pos);
  });

  it('mono sources play on both channels', () => {
    const p = new LoopPlayer();
    p.setBuffer([src]);
    const L = new Float32Array(64);
    const R = new Float32Array(64);
    p.process(L, R, 0, 64, 1, 1);
    expect(Array.from(R)).toEqual(Array.from(L));
  });
});
