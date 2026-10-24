/**
 * Mode B: granular morphing.
 *
 * Sound is rebuilt from short overlapping grains read from the loaded
 * samples. The morph timeline does not change grain *volume*: it changes
 * the *probability* that each new grain is taken from each sample. At a
 * 30 % / 70 % point, about 30 % of grains come from the first sample.
 *
 * - Each sample has a scan head moving through its loop region at the
 *   playback speed; a grain starts at its sample's scan head plus position
 *   randomness, so short samples turn into long, non-repeating textures.
 * - Grains are scheduled sample-accurately. Onset interval = size ÷ overlap,
 *   with a little random jitter so the cloud has no fixed pulse; density is
 *   the probability that each scheduled grain sounds.
 * - Each grain has its own pitch (transpose + random cents), pan (stereo
 *   spread) and window. Reads use 4-point cubic interpolation.
 * - Output is normalised for the expected overlap: by amplitude when grains
 *   are coherent (little randomness) and by power when they are not, so
 *   size/overlap/density changes do not change loudness much.
 * - Freeze stops the scan heads while grains keep spawning, so the cloud
 *   continues around the frozen moment.
 *
 * Voices come from a fixed pool; nothing is allocated while playing.
 */

import { Random } from '../random';
import { windowTable, WINDOW_SIZE, type WindowType } from '../windows';
import type { EngineBlock, MorphEngine } from './types';
import type { SlotRuntime } from '../slot';

export interface GranularParams {
  /** Grain length, ms (10–500). */
  sizeMs: number;
  /** Average number of overlapping grains (1–16). */
  overlap: number;
  /** Probability that a scheduled grain sounds (0–1). */
  density: number;
  /** Start-position randomness as a fraction of the loop length (0–1). */
  positionRandom: number;
  /** Random pitch deviation per grain, ± cents (0–1200). */
  pitchRandomCents: number;
  /** Fixed transposition of every grain, semitones (−24..24). */
  transpose: number;
  /** Scan-head speed through each sample (−2..2; 1 = original tempo). */
  speed: number;
  /** Stereo spread of grain pans (0–1). */
  spread: number;
  window: WindowType;
  /** Random seed: same seed, same settings → same texture. */
  seed: number;
  /** Onset jitter (0–1): 0 = regular pulse, 1 = fully asynchronous. */
  jitter: number;
}

export function defaultGranularParams(): GranularParams {
  return {
    sizeMs: 120,
    overlap: 4,
    density: 1,
    positionRandom: 0.05,
    pitchRandomCents: 0,
    transpose: 0,
    speed: 1,
    spread: 0.4,
    window: 'hann',
    seed: 1,
    jitter: 0.3,
  };
}

const MAX_VOICES = 160;
const MAX_SLOTS = 8;

class Grain {
  active = false;
  slot = 0;
  pos = 0; // virtual read position (frames, fractional)
  rate = 1;
  age = 0;
  length = 1;
  gainL = 0;
  gainR = 0;
  /** Delay (frames) before the grain starts within the current block. */
  delay = 0;
}

/** Region of a slot as seen by the granular engine (virtual, direction-adjusted). */
interface Region {
  start: number;
  end: number;
}

export class GranularEngine implements MorphEngine {
  readonly id = 'granular';
  params: GranularParams = defaultGranularParams();
  private readonly voices: Grain[] = [];
  private readonly rng = new Random(1);
  private readonly heads = new Float64Array(MAX_SLOTS);
  private readonly headInit = new Uint8Array(MAX_SLOTS);
  private readonly weights = new Float32Array(MAX_SLOTS);
  private readonly regions: Region[] = [];
  private countdown = 0;
  /** Number of grains currently sounding (for meters/tests). */
  activeCount = 0;
  /** Grains started per slot since the last reset (for tests and display). */
  readonly spawnCounts = new Float64Array(MAX_SLOTS);

  constructor(private readonly sampleRate: number) {
    for (let i = 0; i < MAX_VOICES; i++) this.voices.push(new Grain());
    for (let i = 0; i < MAX_SLOTS; i++) this.regions.push({ start: 0, end: 0 });
  }

  setParams(p: Partial<GranularParams>): void {
    const seedChanged = p.seed !== undefined && p.seed !== this.params.seed;
    this.params = { ...this.params, ...p };
    if (seedChanged) this.reset();
  }

  reset(): void {
    this.rng.reseed(this.params.seed * 2654435761);
    for (const v of this.voices) v.active = false;
    this.headInit.fill(0);
    this.spawnCounts.fill(0);
    this.activeCount = 0;
    this.countdown = 0;
  }

  /** Normalisation for the current settings (see class comment). */
  normalisation(loopFrames = this.sampleRate): number {
    const p = this.params;
    const w = windowTable(p.window);
    const n = Math.max(0.25, p.overlap * Math.max(0.05, p.density));
    const sizeFrames = (p.sizeMs / 1000) * this.sampleRate;
    // Coherence estimate: grains that start near the same place and play at
    // the same pitch add like one signal; random ones add like noise.
    const posSpread = p.positionRandom * loopFrames;
    const incoherence = Math.min(1, posSpread / (0.25 * sizeFrames) + p.pitchRandomCents / 30);
    const ampNorm = n * w.meanW;
    const powNorm = Math.sqrt(n * w.meanW2);
    return 1 / Math.pow(ampNorm, 1 - incoherence) / Math.pow(powNorm, incoherence);
  }

  private updateRegions(slots: readonly SlotRuntime[]): void {
    for (let s = 0; s < slots.length; s++) {
      const slot = slots[s];
      const r = this.regions[s];
      const len = slot.channels[0]?.length ?? 0;
      if (!len) {
        r.start = r.end = 0;
        continue;
      }
      let ls = slot.settings.region.loopStart;
      let le = slot.settings.region.loopEnd;
      if (le - ls < 2) {
        ls = 0;
        le = len;
      }
      if (slot.settings.reverse) {
        const a = len - le;
        le = len - ls;
        ls = a;
      }
      r.start = ls;
      r.end = le;
      if (!this.headInit[s] || this.heads[s] < ls || this.heads[s] >= le) {
        this.heads[s] = ls;
        this.headInit[s] = 1;
      }
    }
  }

  process(block: EngineBlock, outL: Float32Array, outR: Float32Array, frames: number): void {
    const { slots, fractionEnd } = block;
    const p = this.params;
    const sr = this.sampleRate;
    this.updateRegions(slots);

    // Selection weights: mix fraction × audible (mute/solo/declick).
    let total = 0;
    let spanSum = 0;
    for (let s = 0; s < slots.length; s++) {
      const r = this.regions[s];
      const audible = slots[s].blockGainEnd > 1e-4 && r.end > r.start ? 1 : 0;
      const w = fractionEnd[s] * audible;
      this.weights[s] = w;
      total += w;
      spanSum += w * (r.end - r.start);
    }

    // Schedule new grains within this block.
    const sizeFrames = Math.max(8, Math.round((p.sizeMs / 1000) * sr));
    const interval = Math.max(1, sizeFrames / Math.max(0.25, p.overlap));
    let t = this.countdown;
    while (t < frames) {
      if (total > 1e-6 && this.rng.next() < p.density) this.spawn(Math.max(0, Math.floor(t)), sizeFrames, total, slots);
      t += interval * (1 + p.jitter * (this.rng.next() - 0.5) * 1.6);
    }
    this.countdown = t - frames;

    // Render active grains.
    const norm = this.normalisation(total > 0 ? spanSum / total : sr);
    const wt = windowTable(p.window).table;
    let count = 0;
    for (const v of this.voices) {
      if (!v.active) continue;
      count++;
      const slot = slots[v.slot];
      const ch = slot.channels;
      const L = ch[0];
      const R = ch[1] ?? ch[0];
      if (!L) {
        v.active = false;
        continue;
      }
      const len = L.length;
      const last = len - 1;
      const rev = slot.settings.reverse;
      const g0 = slot.blockGainStart * norm;
      const dg = (slot.blockGainEnd * norm - g0) / frames;
      const invLen = WINDOW_SIZE / v.length;
      let i = v.delay;
      v.delay = 0;
      let pos = v.pos;
      let age = v.age;
      const rate = v.rate;
      for (; i < frames && age < v.length; i++) {
        // window lookup (linear)
        const wp = age * invLen;
        const wi = wp | 0;
        const wf = wp - wi;
        const w = wt[wi] + (wt[wi + 1] - wt[wi]) * wf;
        // 4-point cubic Hermite read
        const ip = Math.floor(pos);
        const f = pos - ip;
        const g = (g0 + dg * i) * w;
        let i0 = ip - 1, i1 = ip, i2 = ip + 1, i3 = ip + 2;
        i0 = i0 < 0 ? 0 : i0 > last ? last : i0;
        i1 = i1 > last ? last : i1;
        i2 = i2 > last ? last : i2;
        i3 = i3 > last ? last : i3;
        if (rev) {
          i0 = last - i0; i1 = last - i1; i2 = last - i2; i3 = last - i3;
        }
        const l = hermite(L[i0], L[i1], L[i2], L[i3], f);
        const r = R === L ? l : hermite(R[i0], R[i1], R[i2], R[i3], f);
        outL[i] += l * g * v.gainL;
        outR[i] += r * g * v.gainR;
        pos += rate;
        age++;
      }
      v.pos = pos;
      v.age = age;
      if (age >= v.length) v.active = false;
    }
    this.activeCount = count;

    // Advance scan heads (held while frozen).
    if (!block.frozen) {
      const adv = p.speed * frames;
      for (let s = 0; s < slots.length; s++) {
        const r = this.regions[s];
        const span = r.end - r.start;
        if (span <= 0) continue;
        let h = this.heads[s] + adv;
        if (h >= r.end || h < r.start) h = r.start + ((((h - r.start) % span) + span) % span);
        this.heads[s] = h;
      }
    }
  }

  private spawn(offset: number, sizeFrames: number, total: number, slots: readonly SlotRuntime[]): void {
    const p = this.params;
    // choose source by weighted probability
    let x = this.rng.next() * total;
    let s = 0;
    for (; s < slots.length - 1; s++) {
      x -= this.weights[s];
      if (x < 0) break;
    }
    while (this.weights[s] <= 0 && s > 0) s--;
    const r = this.regions[s];
    const span = r.end - r.start;
    if (span <= 4) return;
    let v: Grain | null = null;
    for (let i = 0; i < this.voices.length; i++) {
      if (!this.voices[i].active) {
        v = this.voices[i];
        break;
      }
    }
    if (!v) return; // pool full: drop the grain rather than steal (no clicks)
    const cents = p.transpose * 100 + p.pitchRandomCents * this.rng.bipolar();
    const rate = Math.pow(2, cents / 1200);
    const reach = sizeFrames * rate;
    // Start at the scan head ± position randomness; keep the whole grain
    // inside the loop region when it fits so it never crosses the seam.
    let start = this.heads[s] + p.positionRandom * span * this.rng.bipolar();
    const room = span - reach - 2;
    if (room > 0) start = r.start + ((((start - r.start) % room) + room) % room);
    else start = r.start;
    const pan = 0.5 + 0.5 * p.spread * this.rng.bipolar();
    this.spawnCounts[s]++;
    v.active = true;
    v.slot = s;
    v.pos = start;
    v.rate = rate;
    v.age = 0;
    v.length = sizeFrames;
    v.gainL = Math.cos(pan * Math.PI * 0.5) * Math.SQRT2;
    v.gainR = Math.sin(pan * Math.PI * 0.5) * Math.SQRT2;
    v.delay = offset;
  }
}

function hermite(xm1: number, x0: number, x1: number, x2: number, t: number): number {
  const c = (x1 - xm1) * 0.5;
  const v = x0 - x1;
  const w = c + v;
  const a = w + v + (x2 - x0) * 0.5;
  const bNeg = w + a;
  return ((a * t - bNeg) * t + c) * t + x0;
}
