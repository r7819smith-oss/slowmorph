/**
 * Mode C: spectral morphing (STFT analysis, spectral interpolation,
 * phase-vocoder resynthesis).
 *
 * Every hop, each audible sample is analysed at its own scan head with a
 * Hann-windowed FFT. Two frames one hop apart give each bin's magnitude and
 * instantaneous frequency. The sources are then combined into ONE spectrum:
 *
 *   Linear layer  Σ wᵢ·Mᵢ: transparent magnitude interpolation.
 *   Hybrid layer  each spectrum is split into a smooth spectral envelope
 *                 (log magnitude averaged over a ~⅕-octave sliding band) and
 *                 a fine structure (magnitude ÷ envelope). Envelopes and
 *                 fine structures are interpolated geometrically and with
 *                 separate weights ("timbre lead"), so the resonant shape of
 *                 one sound can be imposed on the partials of another.
 *   The Interpolation control blends the two layers.
 *
 * Phase: source phases are never mixed (that is what causes cancellation).
 * One running phase per output bin is advanced by the magnitude-weighted
 * instantaneous frequency, with identity phase locking (bins around each
 * spectral peak keep the dominant source's phase relationships), which keeps
 * partials coherent and avoids the "phasiness" of a plain phase vocoder.
 *
 * Then: frame energy is normalised to the weighted energy of the sources
 * (stable loudness through the morph), optional smoothing across frequency
 * and blur across time, optional phase randomisation, stereo decorrelation,
 * inverse FFT and windowed overlap-add.
 *
 * Freeze holds the current hybrid spectrum and keeps advancing phases, so
 * the frozen moment sustains as a steady sound.
 *
 * Work is spread over the render blocks of each hop (one FFT per block) so
 * no single 128-frame block carries a whole frame's cost; this adds one hop
 * of latency.
 *
 * Limitations: partials are interpolated in place (a partial of A does not
 * glide to the frequency of a partial in B); the spectral envelope is
 * interpolated by level, not by moving formants along the frequency axis;
 * transients are smeared, more so at large FFT sizes.
 */

import { FFT } from '../fft';
import { Random } from '../random';
import type { SlotRuntime } from '../slot';
import type { EngineBlock, MorphEngine } from './types';

export type FftSize = 512 | 1024 | 2048 | 4096 | 8192;
export const FFT_SIZES: readonly FftSize[] = [512, 1024, 2048, 4096, 8192];

export interface SpectralParams {
  fftSize: FftSize;
  /** Frames per window: 4 (efficient) or 8 (smoother). */
  overlap: 4 | 8;
  /** 0 = linear magnitude interpolation, 1 = envelope/fine-structure hybrid. */
  interp: number;
  /** −1..1: positive moves the spectral envelope (timbre) ahead of the partials. */
  envelopeLead: number;
  /** 0..1 temporal smoothing of the spectrum (smear). */
  blur: number;
  /** 0..1 smoothing across frequency. */
  smoothing: number;
  /** 0..1 random phase added each frame (diffuse, whispering textures). */
  phaseRandom: number;
  /** Scan-head speed through each sample (−2..2). */
  speed: number;
  /** 0..1 stereo decorrelation of the resynthesis. */
  width: number;
}

export function defaultSpectralParams(): SpectralParams {
  return { fftSize: 2048, overlap: 4, interp: 0.5, envelopeLead: 0, blur: 0.2, smoothing: 0, phaseRandom: 0, speed: 1, width: 0.3 };
}

const MAX_SLOTS = 8;
const TWO_PI = Math.PI * 2;
/** Fade time when the FFT size or overlap changes while playing (s). */
const RECONFIG_FADE = 0.02;

function princarg(x: number): number {
  return x - TWO_PI * Math.round(x / TWO_PI);
}

interface SourceState {
  mag: Float32Array;
  phase: Float32Array; // analysis phase of the later frame
  prevPhase: Float32Array; // analysis phase of the earlier frame
  freq: Float32Array; // rad/sample
  logEnv: Float32Array;
  logFine: Float32Array;
  power: number;
  /** Position the cached phase was analysed at (−1 = none). */
  cachedPos: number;
  cachedPhase: Float32Array;
}

/** Work items for one frame, prepared over several render blocks. */
const STAGE_ANALYSE0 = 0;
const STAGE_ANALYSE1 = 1;
const STAGE_MIX = 2;
const STAGE_SYNTH = 3;
const STAGE_SYNTH_R = 4;
const MAX_STAGES = MAX_SLOTS * 2 + 3;

export class SpectralEngine implements MorphEngine {
  readonly id = 'spectral';
  params: SpectralParams = defaultSpectralParams();
  private readonly sampleRate: number;
  private readonly rng = new Random(7);

  // configuration-dependent buffers
  private n = 0;
  private hop = 0;
  private bins = 0;
  private fft!: FFT;
  private win!: Float32Array;
  private re!: Float64Array;
  private im!: Float64Array;
  private sources: SourceState[] = [];
  private magOut!: Float32Array;
  private magBlur!: Float32Array;
  private freqOut!: Float32Array;
  private outPhase!: Float64Array;
  private dominant!: Uint8Array;
  private peaks!: Int32Array;
  private decor!: Float32Array;
  private prefix!: Float64Array;
  private olaL!: Float32Array;
  private olaR!: Float32Array;
  private frameL!: Float32Array;
  private frameR!: Float32Array;
  private outQL!: Float32Array;
  private outQR!: Float32Array;
  private outIndex = 0;

  // frame pipeline
  private readonly stageKind = new Uint8Array(MAX_STAGES);
  private readonly stageSlot = new Uint8Array(MAX_STAGES);
  private stageCount = 0;
  private stageIndex = 0;
  private readonly frameWeights = new Float32Array(MAX_SLOTS);
  private readonly framePos = new Float64Array(MAX_SLOTS);
  private frameFrozen = false;
  private haveSpectrum = false;
  private gainSmooth = 1;

  private readonly heads = new Float64Array(MAX_SLOTS);
  private readonly headInit = new Uint8Array(MAX_SLOTS);
  private readonly regionStart = new Float64Array(MAX_SLOTS);
  private readonly regionEnd = new Float64Array(MAX_SLOTS);

  // reconfiguration fade
  private pending: { fftSize: FftSize; overlap: 4 | 8 } | null = null;
  private fade = 1;
  private running = false;

  /** Frames analysed since reset (for tests). */
  framesComputed = 0;

  constructor(sampleRate: number) {
    this.sampleRate = sampleRate;
    this.configure(this.params.fftSize, this.params.overlap);
  }

  setParams(p: Partial<SpectralParams>): void {
    const next = { ...this.params, ...p };
    const reconfig = next.fftSize !== this.params.fftSize || next.overlap !== this.params.overlap;
    this.params = next;
    if (!reconfig) return;
    if (FFT_SIZES.indexOf(next.fftSize) < 0) this.params.fftSize = 2048;
    if (next.overlap !== 4 && next.overlap !== 8) this.params.overlap = 4;
    if (this.running) this.pending = { fftSize: this.params.fftSize, overlap: this.params.overlap };
    else this.configure(this.params.fftSize, this.params.overlap);
  }

  private configure(n: FftSize, overlap: 4 | 8): void {
    this.n = n;
    this.hop = n / overlap;
    this.bins = n / 2 + 1;
    this.fft = new FFT(n);
    this.win = new Float32Array(n);
    for (let i = 0; i < n; i++) this.win[i] = 0.5 - 0.5 * Math.cos((TWO_PI * i) / n);
    this.re = new Float64Array(n);
    this.im = new Float64Array(n);
    const b = this.bins;
    this.sources = [];
    for (let s = 0; s < MAX_SLOTS; s++) {
      this.sources.push({
        mag: new Float32Array(b),
        phase: new Float32Array(b),
        prevPhase: new Float32Array(b),
        freq: new Float32Array(b),
        logEnv: new Float32Array(b),
        logFine: new Float32Array(b),
        power: 0,
        cachedPos: -1,
        cachedPhase: new Float32Array(b),
      });
    }
    this.magOut = new Float32Array(b);
    this.magBlur = new Float32Array(b);
    this.freqOut = new Float32Array(b);
    this.outPhase = new Float64Array(b);
    this.dominant = new Uint8Array(b);
    this.peaks = new Int32Array(b);
    this.prefix = new Float64Array(b + 1);
    this.decor = new Float32Array(b);
    const r = new Random(12345);
    const lowBin = Math.ceil((200 * n) / this.sampleRate);
    for (let k = 0; k < b; k++) this.decor[k] = k < lowBin ? 0 : r.bipolar();
    this.olaL = new Float32Array(n);
    this.olaR = new Float32Array(n);
    this.frameL = new Float32Array(n);
    this.frameR = new Float32Array(n);
    this.outQL = new Float32Array(this.hop);
    this.outQR = new Float32Array(this.hop);
    this.resetState();
  }

  private resetState(): void {
    this.olaL.fill(0);
    this.olaR.fill(0);
    this.outQL.fill(0);
    this.outQR.fill(0);
    this.magBlur.fill(0);
    this.outPhase.fill(0);
    this.outIndex = this.hop; // compute a frame at the first sample
    this.stageIndex = 0;
    this.stageCount = 0;
    this.haveSpectrum = false;
    this.gainSmooth = 1;
    for (const s of this.sources) s.cachedPos = -1;
    this.headInit.fill(0);
    this.framesComputed = 0;
  }

  reset(): void {
    if (this.pending) {
      this.configure(this.pending.fftSize, this.pending.overlap);
      this.pending = null;
    } else this.resetState();
    this.fade = 1;
    this.running = false;
  }

  /** Latency of the engine in frames (window + one pipelined hop). */
  get latency(): number {
    return this.n + this.hop;
  }

  // ---- regions and scan heads -----------------------------------------------------

  private updateRegions(slots: readonly SlotRuntime[]): void {
    for (let s = 0; s < slots.length; s++) {
      const slot = slots[s];
      const len = slot.channels[0]?.length ?? 0;
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
      this.regionStart[s] = ls;
      this.regionEnd[s] = le;
      if (len && (!this.headInit[s] || this.heads[s] < ls || this.heads[s] >= le)) {
        this.heads[s] = ls;
        this.headInit[s] = 1;
      }
    }
  }

  // ---- audio ---------------------------------------------------------------------------

  process(block: EngineBlock, outL: Float32Array, outR: Float32Array, frames: number): void {
    const { slots } = block;
    this.running = true;
    this.updateRegions(slots);
    const fadeStep = 1 / (RECONFIG_FADE * this.sampleRate);
    let i = 0;
    while (i < frames) {
      if (this.outIndex >= this.hop) {
        // Finish whatever is left of the frame being prepared, then start the next.
        if (this.stageCount === 0) this.beginFrame(block);
        while (this.stageIndex < this.stageCount) this.runStage(slots);
        this.emitFrame();
        this.beginFrame(block);
      }
      const n = Math.min(frames - i, this.hop - this.outIndex);
      let f = this.fade;
      for (let k = 0; k < n; k++) {
        outL[i + k] += this.outQL[this.outIndex + k] * f;
        outR[i + k] += this.outQR[this.outIndex + k] * f;
        if (this.pending) f = Math.max(0, f - fadeStep);
        else if (f < 1) f = Math.min(1, f + fadeStep);
      }
      this.fade = f;
      this.outIndex += n;
      i += n;
      if (this.pending && this.fade === 0) {
        const p = this.pending;
        this.pending = null;
        this.configure(p.fftSize, p.overlap);
        // configure() resets state; fade back in from silence
      }
    }
    // Spread the next frame's work evenly over the blocks left in this hop.
    const left = this.stageCount - this.stageIndex;
    if (left > 0) {
      const blocksLeft = Math.max(1, Math.ceil((this.hop - this.outIndex) / Math.max(1, frames)));
      const todo = Math.ceil(left / blocksLeft);
      for (let k = 0; k < todo; k++) this.runStage(slots);
    }
  }

  private pushStage(kind: number, slot: number): void {
    this.stageKind[this.stageCount] = kind;
    this.stageSlot[this.stageCount] = slot;
    this.stageCount++;
  }

  /** Capture weights and scan positions and queue the work for one frame. */
  private beginFrame(block: EngineBlock): void {
    const { slots, fractionEnd } = block;
    let total = 0;
    for (let s = 0; s < MAX_SLOTS; s++) {
      const slot = slots[s];
      let w = 0;
      if (slot && slot.channels.length && this.regionEnd[s] > this.regionStart[s]) {
        const g = slot.settings.gain > 1e-9 ? slot.blockGainEnd / slot.settings.gain : 0;
        w = fractionEnd[s] * Math.min(1, Math.max(0, g));
      }
      this.frameWeights[s] = w;
      total += w;
    }
    if (total > 1e-9) for (let s = 0; s < MAX_SLOTS; s++) this.frameWeights[s] /= total;
    this.frameFrozen = block.frozen && this.haveSpectrum;
    this.stageCount = 0;
    this.stageIndex = 0;
    if (!this.frameFrozen) {
      for (let s = 0; s < MAX_SLOTS; s++) {
        if (this.frameWeights[s] < 1e-4) continue;
        this.framePos[s] = Math.floor(this.heads[s]);
        if (this.sources[s].cachedPos !== this.framePos[s]) this.pushStage(STAGE_ANALYSE0, s);
        this.pushStage(STAGE_ANALYSE1, s);
      }
      this.pushStage(STAGE_MIX, 0);
      // advance scan heads by one hop
      const adv = this.params.speed * this.hop;
      for (let s = 0; s < MAX_SLOTS; s++) {
        const a = this.regionStart[s];
        const span = this.regionEnd[s] - a;
        if (span <= 0) continue;
        let h = this.heads[s] + adv;
        if (h >= this.regionEnd[s] || h < a) h = a + ((((h - a) % span) + span) % span);
        this.heads[s] = h;
      }
    }
    this.pushStage(STAGE_SYNTH, 0);
    this.pushStage(STAGE_SYNTH_R, 0);
  }

  private runStage(slots: readonly SlotRuntime[]): void {
    const i = this.stageIndex++;
    const kind = this.stageKind[i];
    const s = this.stageSlot[i];
    if (kind === STAGE_ANALYSE0) this.analyse(slots[s], s, 0);
    else if (kind === STAGE_ANALYSE1) this.analyse(slots[s], s, 1);
    else if (kind === STAGE_MIX) this.mix();
    else if (kind === STAGE_SYNTH) this.synth();
    else this.synthRight();
  }

  // ---- analysis -------------------------------------------------------------------------

  /**
   * Read one windowed mono frame from a slot starting at virtual position
   * `pos`. The loop region is read as the seamless loop the crossfade
   * engine plays: period = loop length − xf, with an equal-power crossfade
   * from the loop tail into the loop head. A hard wrap here would put a
   * step into the frame, and phase locking would faithfully resynthesise it
   * as a click.
   */
  private readFrame(slot: SlotRuntime, s: number, pos: number): void {
    const ch = slot.channels;
    const L = ch[0];
    const R = ch[1] ?? ch[0];
    const stereo = R !== L;
    const last = L.length - 1;
    const a = this.regionStart[s];
    const span = this.regionEnd[s] - a;
    const xf = Math.min(Math.round(0.05 * this.sampleRate), Math.floor(span / 4));
    const period = span - xf;
    const fadeStart = a + span - xf;
    const rev = slot.settings.reverse;
    const gain = slot.settings.gain * (stereo ? 0.5 : 1);
    let t = (((pos - a) % period) + period) % period;
    const n = this.n;
    const re = this.re;
    const im = this.im;
    const half = Math.PI * 0.5;
    for (let i = 0; i < n; i++) {
      const q = a + xf + t;
      const iq = rev ? last - q : q;
      let v = stereo ? L[iq] + R[iq] : L[iq];
      if (q >= fadeStart && xf > 0) {
        const k = q - fadeStart;
        const x = (k + 0.5) / xf;
        const ih = rev ? last - (a + k) : a + k;
        const h = stereo ? L[ih] + R[ih] : L[ih];
        v = v * Math.cos(x * half) + h * Math.sin(x * half);
      }
      re[i] = v * gain * this.win[i];
      im[i] = 0;
      t++;
      if (t >= period) t = 0;
    }
  }

  private analyse(slot: SlotRuntime, s: number, which: 0 | 1): void {
    const src = this.sources[s];
    const pos = this.framePos[s];
    const b = this.bins;
    if (which === 0) {
      this.readFrame(slot, s, pos);
      this.fft.transform(this.re, this.im);
      for (let k = 0; k < b; k++) src.cachedPhase[k] = Math.atan2(this.im[k], this.re[k]);
      src.cachedPos = pos;
      return;
    }
    // Earlier frame's phase is in cachedPhase (analysed now or on the previous hop).
    src.prevPhase.set(src.cachedPhase);
    this.readFrame(slot, s, pos + this.hop);
    this.fft.transform(this.re, this.im);
    const hop = this.hop;
    const n = this.n;
    let power = 0;
    let maxM = 0;
    for (let k = 0; k < b; k++) {
      const m = Math.hypot(this.re[k], this.im[k]);
      const ph = Math.atan2(this.im[k], this.re[k]);
      src.mag[k] = m;
      src.phase[k] = ph;
      const expected = (TWO_PI * k * hop) / n;
      src.freq[k] = (expected + princarg(ph - src.prevPhase[k] - expected)) / hop;
      power += m * m;
      if (m > maxM) maxM = m;
    }
    src.power = power;
    // The later frame becomes the earlier frame of the next hop when the
    // scan head moves exactly one hop (speed 1): saves one FFT per hop.
    src.cachedPhase.set(src.phase);
    src.cachedPos = pos + hop;
    this.envelope(src, maxM);
  }

  /**
   * Split a magnitude spectrum into a log envelope and a log fine structure.
   * The envelope is the log magnitude averaged twice (a triangular kernel)
   * over a band of ±20 % of the frequency, at least ±120 Hz, which is wide
   * enough to step over individual partials and keep the resonant shape.
   */
  private envelope(src: SourceState, maxM: number): void {
    const b = this.bins;
    const floor = 1e-9 + maxM * 1e-5;
    const logM = src.logFine; // used as scratch for the log magnitude first
    const env = src.logEnv;
    for (let k = 0; k < b; k++) logM[k] = Math.log(src.mag[k] + floor);
    const minR = Math.max(2, Math.ceil((120 * this.n) / this.sampleRate));
    this.boxSmooth(logM, env, minR);
    this.boxSmooth(env, env, minR);
    for (let k = 0; k < b; k++) logM[k] -= env[k];
  }

  /** Variable-width moving average (prefix sums); `out` may alias `inp`. */
  private boxSmooth(inp: Float32Array, out: Float32Array, minR: number): void {
    const b = this.bins;
    const pre = this.prefix;
    pre[0] = 0;
    for (let k = 0; k < b; k++) pre[k + 1] = pre[k] + inp[k];
    for (let k = 0; k < b; k++) {
      const r = Math.max(minR, Math.round(k * 0.2));
      const lo = Math.max(0, k - r);
      const hi = Math.min(b, k + r + 1);
      out[k] = (pre[hi] - pre[lo]) / (hi - lo);
    }
  }

  // ---- interpolation ---------------------------------------------------------------------

  private mix(): void {
    const p = this.params;
    const b = this.bins;
    const w = this.frameWeights;
    // Timbre lead: for the two strongest sources, reshape the later one's
    // weight differently for envelope and fine structure.
    let a = -1;
    let c = -1;
    for (let s = 0; s < MAX_SLOTS; s++) {
      if (w[s] < 1e-4) continue;
      if (a < 0 || w[s] > w[a]) {
        c = a;
        a = s;
      } else if (c < 0 || w[s] > w[c]) c = s;
    }
    const lo = c >= 0 ? Math.min(a, c) : a;
    const hiS = c >= 0 ? Math.max(a, c) : -1;
    const pairSum = hiS >= 0 ? w[lo] + w[hiS] : 1;
    const t = hiS >= 0 ? w[hiS] / pairSum : 0;
    const lead = Math.max(-1, Math.min(1, p.envelopeLead));
    const shapeT = (x: number, l: number) => (l >= 0 ? 1 - Math.pow(1 - x, 1 + 3 * l) : Math.pow(x, 1 - 3 * l));
    const tEnv = shapeT(t, lead);
    const tFine = shapeT(t, -lead);
    const interp = Math.max(0, Math.min(1, p.interp));

    let targetPower = 0;
    for (let s = 0; s < MAX_SLOTS; s++) if (w[s] >= 1e-4) targetPower += w[s] * this.sources[s].power;

    const out = this.magOut;
    let actual = 0;
    for (let k = 0; k < b; k++) {
      let lin = 0;
      let logE = 0;
      let logF = 0;
      let num = 0;
      let den = 0;
      let domW = -1;
      let dom = a < 0 ? 0 : a;
      for (let s = 0; s < MAX_SLOTS; s++) {
        const ws = w[s];
        if (ws < 1e-4) continue;
        const src = this.sources[s];
        const m = src.mag[k];
        lin += ws * m;
        let we = ws;
        let wf = ws;
        if (hiS >= 0 && (s === lo || s === hiS)) {
          we = pairSum * (s === hiS ? tEnv : 1 - tEnv);
          wf = pairSum * (s === hiS ? tFine : 1 - tFine);
        }
        logE += we * src.logEnv[k];
        logF += wf * src.logFine[k];
        const wm = ws * m;
        num += wm * src.freq[k];
        den += wm;
        if (wm > domW) {
          domW = wm;
          dom = s;
        }
      }
      const hyb = Math.exp(logE + logF);
      const m = interp <= 0 ? lin : interp >= 1 ? hyb : lin * (1 - interp) + hyb * interp;
      out[k] = m;
      actual += m * m;
      this.freqOut[k] = den > 1e-12 ? num / den : (TWO_PI * k) / this.n;
      this.dominant[k] = dom;
    }

    // smoothing across frequency (energy is restored below)
    if (p.smoothing > 0.001) {
      const r = Math.max(1, Math.round(p.smoothing * 24));
      const pre = this.prefix;
      pre[0] = 0;
      for (let k = 0; k < b; k++) pre[k + 1] = pre[k] + out[k];
      actual = 0;
      for (let k = 0; k < b; k++) {
        const l = Math.max(0, k - r);
        const h = Math.min(b, k + r + 1);
        out[k] = (pre[h] - pre[l]) / (h - l);
        actual += out[k] * out[k];
      }
    }

    // energy normalisation, smoothed over frames, limited to ±12 dB
    let g = actual > 1e-20 ? Math.sqrt(targetPower / actual) : 1;
    g = Math.max(0.25, Math.min(4, g));
    this.gainSmooth += (g - this.gainSmooth) * 0.5;
    const blur = Math.max(0, Math.min(1, p.blur)) * 0.97;
    const mb = this.magBlur;
    const gs = this.gainSmooth;
    for (let k = 0; k < b; k++) mb[k] = mb[k] * blur + out[k] * gs * (1 - blur);
    this.haveSpectrum = true;
    this.framesComputed++;
  }

  // ---- resynthesis --------------------------------------------------------------------------

  private synth(): void {
    const p = this.params;
    const b = this.bins;
    const hop = this.hop;
    const mag = this.magBlur;
    const ph = this.outPhase;
    const freq = this.freqOut;

    // identity phase locking around spectral peaks
    let maxM = 0;
    for (let k = 0; k < b; k++) if (mag[k] > maxM) maxM = mag[k];
    const thr = maxM * 1e-6;
    let np = 0;
    for (let k = 1; k < b - 1; k++) if (mag[k] > thr && mag[k] > mag[k - 1] && mag[k] >= mag[k + 1]) this.peaks[np++] = k;
    if (np === 0) {
      for (let k = 0; k < b; k++) ph[k] = princarg(ph[k] + hop * freq[k]);
    } else {
      for (let j = 0; j < np; j++) {
        const pk = this.peaks[j];
        ph[pk] = princarg(ph[pk] + hop * freq[pk]);
      }
      for (let j = 0; j < np; j++) {
        const pk = this.peaks[j];
        const start = j === 0 ? 0 : ((this.peaks[j - 1] + pk) >> 1) + 1;
        const end = j === np - 1 ? b - 1 : (pk + this.peaks[j + 1]) >> 1;
        const src = this.sources[this.dominant[pk]];
        const ref = src.phase[pk];
        const base = ph[pk];
        for (let k = start; k <= end; k++) if (k !== pk) ph[k] = princarg(base + src.phase[k] - ref);
      }
    }
    const pr = Math.max(0, Math.min(1, p.phaseRandom));
    if (pr > 0.001) {
      const amt = pr * pr * Math.PI;
      for (let k = 1; k < b; k++) ph[k] = princarg(ph[k] + amt * this.rng.bipolar());
    }

    this.inverse(mag, ph, -this.widthAngle(), this.frameL, this.olaNorm());
  }

  /** Right channel: same spectrum, opposite decorrelation offset. */
  private synthRight(): void {
    const width = this.widthAngle();
    if (width > 0.001) this.inverse(this.magBlur, this.outPhase, width, this.frameR, this.olaNorm());
    else this.frameR.set(this.frameL);
  }

  private widthAngle(): number {
    return Math.max(0, Math.min(1, this.params.width)) * Math.PI * 0.5;
  }

  /** 1 / Σ w² of the Hann window at this overlap. */
  private olaNorm(): number {
    return 1 / ((this.n / this.hop) * 0.375);
  }

  private inverse(mag: Float32Array, ph: Float64Array, offset: number, out: Float32Array, norm: number): void {
    const n = this.n;
    const b = this.bins;
    const re = this.re;
    const im = this.im;
    for (let k = 0; k < b; k++) {
      const phi = ph[k] + offset * this.decor[k];
      re[k] = mag[k] * Math.cos(phi);
      im[k] = mag[k] * Math.sin(phi);
    }
    im[0] = 0;
    im[b - 1] = 0;
    for (let k = 1; k < b - 1; k++) {
      re[n - k] = re[k];
      im[n - k] = -im[k];
    }
    this.fft.transform(re, im, true);
    const win = this.win;
    for (let i = 0; i < n; i++) out[i] = re[i] * win[i] * norm;
  }

  /** Overlap-add the prepared frame and move one hop of output to the queue. */
  private emitFrame(): void {
    const n = this.n;
    const hop = this.hop;
    const L = this.olaL;
    const R = this.olaR;
    for (let i = 0; i < n; i++) {
      L[i] += this.frameL[i];
      R[i] += this.frameR[i];
    }
    this.outQL.set(L.subarray(0, hop));
    this.outQR.set(R.subarray(0, hop));
    L.copyWithin(0, hop);
    R.copyWithin(0, hop);
    L.fill(0, n - hop);
    R.fill(0, n - hop);
    this.outIndex = 0;
  }
}
