/** Phase 5: project files, WAV encoding, offline rendering, factory presets. */
import { describe, expect, it } from 'vitest';
import { deserializeProject, ProjectFileError, serializeProject } from '../src/state/serialize';
import { defaultProject, timelineModel } from '../src/state/project';
import { encodePcm, readWavInfo, wavHeader } from '../src/audio/wav';
import { renderMorph } from '../src/audio/offline-render';
import { stateMessages } from '../src/state/sync';
import { FACTORY_PRESETS } from '../src/state/presets';
import { colouredNoise, hasNonFinite, maxSecondDiff, peak, peakFrequency, rms, sine, SR } from './helpers';
import type { ProjectState } from '../src/state/project';

function projectWith(sources: Float32Array[][], patch: Partial<ProjectState> = {}): ProjectState {
  const s = defaultProject();
  s.slots = s.slots.slice(0, Math.max(2, sources.length));
  sources.forEach((chs, i) => {
    const slot = s.slots[i];
    slot.name = `src ${i}`;
    slot.audio = { channels: chs, sampleRate: SR, frames: chs[0].length, peak: peak(chs[0]), originalSampleRate: null };
    slot.trimEnd = slot.loopEnd = chs[0].length / SR;
  });
  return { ...s, ...patch };
}

async function renderAll(state: ProjectState, tail = 1) {
  const L: Float32Array[] = [];
  const R: Float32Array[] = [];
  await renderMorph(stateMessages(state), { sampleRate: SR, tailSeconds: tail }, (l, r) => {
    L.push(l);
    R.push(r);
  });
  const join = (xs: Float32Array[]) => {
    const out = new Float32Array(xs.reduce((a, x) => a + x.length, 0));
    let o = 0;
    for (const x of xs) {
      out.set(x, o);
      o += x.length;
    }
    return out;
  };
  return { L: join(L), R: join(R) };
}

describe('project files', () => {
  it('round-trips settings and audio losslessly', () => {
    const a = sine(220, 0.5, 0.4);
    const b = [colouredNoise(0.7, 0.5, 0.3, 1), colouredNoise(0.7, 0.2, 0.3, 2)];
    const st = projectWith([[a], b], { durationSec: 1234, engine: 'spectral', character: 0.66 });
    st.slots[1].reverse = true;
    st.slots[1].loopStart = 0.1;
    st.granular.sizeMs = 77;
    st.spectral.warp = -0.3;
    st.effects.delaySend = 0.4;
    st.drift.enabled = true;
    st.midi = [{ param: 'character', channel: 2, type: 'cc', number: 74 }];
    st.timeline.autoSpace = false;
    st.timeline.positions[st.slots[1].id] = 0.7;
    const back = deserializeProject(serializeProject(st));
    expect(back.durationSec).toBe(1234);
    expect(back.engine).toBe('spectral');
    expect(back.character).toBeCloseTo(0.66);
    expect(back.granular.sizeMs).toBe(77);
    expect(back.spectral.warp).toBe(-0.3);
    expect(back.effects.delaySend).toBe(0.4);
    expect(back.drift.enabled).toBe(true);
    expect(back.midi).toEqual(st.midi);
    expect(back.slots[1].reverse).toBe(true);
    expect(back.slots[1].loopStart).toBeCloseTo(0.1);
    expect(back.slots[1].id).toBe(st.slots[1].id);
    expect(timelineModel(back)).toEqual(timelineModel(st));
    expect(Array.from(back.slots[0].audio!.channels[0])).toEqual(Array.from(a));
    expect(back.slots[1].audio!.channels.length).toBe(2);
    expect(Array.from(back.slots[1].audio!.channels[1])).toEqual(Array.from(b[1]));
    expect(back.slots[2]?.audio ?? null).toBeNull();
  });

  it('rejects files that are not projects and reports truncation', () => {
    expect(() => deserializeProject(new TextEncoder().encode('RIFF....WAVEfmt ').buffer)).toThrow(ProjectFileError);
    const good = serializeProject(projectWith([[sine(300, 0.3)], [sine(400, 0.3)]]));
    const truncated = deserializeProject(good.slice(0, good.byteLength - 1000));
    expect(truncated.slots[1].audio).toBeNull();
    expect(truncated.slots[1].error).toMatch(/missing/);
  });

  it('fills missing fields from defaults (older or hand-edited files)', () => {
    const buf = serializeProject(projectWith([[sine(300, 0.2)], [sine(400, 0.2)]]));
    // rewrite the JSON without the granular block and with a bogus field type
    const u8 = new Uint8Array(buf);
    const v = new DataView(buf);
    const len = v.getUint32(14, true);
    const json = JSON.parse(new TextDecoder().decode(u8.subarray(18, 18 + len)));
    delete json.settings.granular;
    json.settings.durationSec = 'ten minutes';
    const nj = new TextEncoder().encode(JSON.stringify(json));
    const head = 18 + nj.length;
    const pad = (4 - (head % 4)) % 4;
    const oldData = 18 + len + ((4 - ((18 + len) % 4)) % 4);
    const out = new Uint8Array(head + pad + (buf.byteLength - oldData));
    out.set(u8.subarray(0, 14));
    new DataView(out.buffer).setUint32(14, nj.length, true);
    out.set(nj, 18);
    out.set(u8.subarray(oldData), head + pad);
    const st = deserializeProject(out.buffer);
    expect(st.granular.sizeMs).toBe(defaultProject().granular.sizeMs);
    expect(st.durationSec).toBe(defaultProject().durationSec);
    expect(st.slots[0].audio).not.toBeNull();
  });
});

describe('WAV encoding', () => {
  it('writes valid 16/24-bit and float headers and samples', () => {
    for (const [fmt, bits, code] of [['pcm16', 16, 1], ['pcm24', 24, 1], ['float32', 32, 3]] as const) {
      const h = wavHeader(1000, 2, 48000, fmt);
      const info = readWavInfo(h.buffer as ArrayBuffer);
      expect(info).toMatchObject({ format: code, channels: 2, sampleRate: 48000, bits, frames: 1000 });
    }
    const L = new Float32Array([0, 0.5, -1, 1]);
    const R = new Float32Array([0.25, -0.5, 2, -2]);
    const b = encodePcm(L, R, 'pcm24');
    const s24 = (i: number) => {
      let v = b[i] | (b[i + 1] << 8) | (b[i + 2] << 16);
      if (v & 0x800000) v -= 0x1000000;
      return v;
    };
    expect(s24(3)).toBe(Math.round(0.25 * 8388607));
    expect(s24(9)).toBe(Math.round(-0.5 * 8388608));
    expect(s24(12)).toBe(-8388608); // clipped −1
    expect(s24(15)).toBe(8388607); // clipped +2
  });
});

describe('offline rendering', () => {
  it('renders the whole morph plus tail, deterministically, matching the live engine', async () => {
    const st = projectWith([[sine(220, 1.13, 0.3)], [sine(330, 0.97, 0.3)]], { durationSec: 6 });
    const a = await renderAll(st, 1);
    expect(a.L.length).toBe(Math.round(7 * SR) + Math.round(0.1 * SR));
    expect(Math.abs(peakFrequency(a.L, Math.round(0.2 * SR)) - 220)).toBeLessThan(6);
    expect(Math.abs(peakFrequency(a.L, Math.round(6.1 * SR)) - 330)).toBeLessThan(6);
    const ref = 0.3 * (2 * Math.PI * 330 / SR) ** 2;
    expect(maxSecondDiff(a.L, 2000)).toBeLessThan(ref * 2.5);
    expect(Math.abs(a.L[a.L.length - 1])).toBeLessThan(1e-4); // faded out at the end
    const b = await renderAll(st, 1);
    expect(Array.from(b.L.subarray(100000, 100100))).toEqual(Array.from(a.L.subarray(100000, 100100)));
  });

  it('is much faster than real time', async () => {
    const st = projectWith([[colouredNoise(1.2, 0.8, 0.3, 4)], [colouredNoise(1.5, 0.2, 0.3, 5)], [sine(200, 1.3, 0.3)]], { durationSec: 20, engine: 'granular' });
    const t0 = performance.now();
    await renderAll(st, 0);
    const factor = 20 / ((performance.now() - t0) / 1000);
    console.log(`granular offline render: ${factor.toFixed(0)}× real time`);
    expect(factor).toBeGreaterThan(5);
  });
});

describe('factory presets', () => {
  it('has the six presets from the brief', () => {
    expect(FACTORY_PRESETS.map((p) => p.name)).toEqual([
      'Slow Dissolve', 'Granular Cloud', 'Spectral Ghost', 'Endless Drone', 'Unstable Memory', 'Field Recording Transformation',
    ]);
  });

  for (const p of FACTORY_PRESETS) {
    it(`“${p.name}” produces sound with no gaps, NaN or overload`, async () => {
      const st = projectWith([[colouredNoise(1.4, 0.95, 0.25, 7)], [sine(180, 1.1, 0.25)], [colouredNoise(1.2, -0.6, 0.25, 8)]]);
      const s = p.settings;
      Object.assign(st, { engine: s.engine, playMode: s.playMode, gainLaw: s.gainLaw, loopCrossfadeMs: s.loopCrossfadeMs, character: s.character, granular: s.granular, spectral: s.spectral, drift: s.drift, effects: s.effects });
      st.durationSec = 8; // short version of the morph
      const { L, R } = await renderAll(st, 0.5);
      expect(hasNonFinite(L) || hasNonFinite(R)).toBe(false);
      expect(peak(L)).toBeLessThanOrEqual(0.8913);
      const w = Math.round(0.1 * SR);
      for (let a = Math.round(0.3 * SR); a + w < L.length - SR * 0.2; a += w) expect(rms(L, a, a + w)).toBeGreaterThan(0.01);
    });
  }
});
