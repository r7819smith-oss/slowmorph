import { describe, expect, it } from 'vitest';
import { parseMidi, PARAMS, routeMidi, type MidiMapping } from '../src/control/midi';
import { randomEffects, randomGranular, randomSegment, randomSpectral } from '../src/control/randomise';
import { Random } from '../src/dsp/random';

describe('MIDI', () => {
  it('parses CC, note-on, note-off (incl. velocity-0 note-on)', () => {
    expect(parseMidi([0xb3, 7, 127])).toMatchObject({ type: 'cc', channel: 3, number: 7, value: 1 });
    expect(parseMidi([0x90, 60, 100])).toMatchObject({ type: 'note', on: true, number: 60 });
    expect(parseMidi([0x90, 60, 0])).toMatchObject({ type: 'note', on: false });
    expect(parseMidi([0x80, 60, 10])).toMatchObject({ type: 'note', on: false });
    expect(parseMidi([0xe0, 0, 64])).toBeNull();
    expect(parseMidi([0xb0, 1])).toBeNull();
  });

  it('learns a mapping and then applies it', () => {
    const applied: [string, number][] = [];
    const apply = (d: { id: string }, v: number) => applied.push([d.id, v]);
    let maps: MidiMapping[] = [];
    const r1 = routeMidi(parseMidi([0xb0, 21, 5])!, maps, 'character', apply, () => false);
    expect(r1.learned).toBe(true);
    maps = r1.mappings;
    expect(maps).toEqual([{ param: 'character', channel: 0, type: 'cc', number: 21 }]);
    routeMidi(parseMidi([0xb0, 21, 127])!, maps, null, apply, () => false);
    routeMidi(parseMidi([0xb1, 21, 127])!, maps, null, apply, () => false); // other channel: ignored
    expect(applied).toEqual([['character', 1]]);
  });

  it('re-learning replaces the old binding for the param and the control', () => {
    let maps: MidiMapping[] = [{ param: 'character', channel: 0, type: 'cc', number: 21 }, { param: 'grainSize', channel: 0, type: 'cc', number: 22 }];
    maps = routeMidi(parseMidi([0xb0, 22, 0])!, maps, 'character', () => {}, () => false).mappings;
    expect(maps).toEqual([{ param: 'character', channel: 0, type: 'cc', number: 22 }]);
  });

  it('notes toggle toggle-params and ignore note-off', () => {
    const applied: number[] = [];
    let state = false;
    const maps: MidiMapping[] = [{ param: 'freeze', channel: 0, type: 'note', number: 36 }];
    const apply = (_d: unknown, v: number) => {
      applied.push(v);
      state = v >= 0.5;
    };
    routeMidi(parseMidi([0x90, 36, 100])!, maps, null, apply, () => state);
    routeMidi(parseMidi([0x80, 36, 0])!, maps, null, apply, () => state);
    routeMidi(parseMidi([0x90, 36, 100])!, maps, null, apply, () => state);
    expect(applied).toEqual([1, 0]);
  });

  it('covers every priority parameter from the brief', () => {
    const ids = PARAMS.map((p) => p.id);
    for (const id of ['morphPosition', 'character', 'grainSize', 'grainDensity', 'spectralBlur', 'filterCutoff', 'freeze', 'playback']) expect(ids).toContain(id);
  });
});

describe('randomise', () => {
  it('stays inside musical, valid ranges', () => {
    const r = new Random(99);
    const rand = () => r.next();
    for (let i = 0; i < 500; i++) {
      const g = randomGranular(rand);
      expect(g.sizeMs!).toBeGreaterThanOrEqual(30);
      expect(g.sizeMs!).toBeLessThanOrEqual(400);
      expect(g.density!).toBeGreaterThanOrEqual(0.6);
      const s = randomSpectral(rand);
      expect([1024, 2048, 4096]).toContain(s.fftSize);
      expect(Math.abs(s.envelopeLead!)).toBeLessThanOrEqual(0.8);
      const seg = randomSegment(rand);
      expect(seg.tEnd).toBeGreaterThan(seg.tStart);
      expect(seg.tEnd).toBeLessThanOrEqual(1);
      const e = randomEffects(rand);
      expect(e.delayFeedback!).toBeLessThanOrEqual(0.6);
      expect(e.lowpassHz!).toBeGreaterThanOrEqual(1500);
    }
  });
});
