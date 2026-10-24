/**
 * MorphCore: everything that runs on the audio thread, independent of the
 * Web Audio API. The AudioWorklet wraps it for real-time playback; tests
 * and the offline renderer drive it directly.
 *
 * Signal flow per block:
 *   clock → timeline fractions → fraction smoothing → engine → master gain
 *   × transport fade → safety limiter → output
 */

import type { GainLaw } from './curves';
import { MorphClock, type PlayMode } from './clock';
import { computeFractions, type TimelineModel } from './timeline';
import { SlotRuntime, type SlotSettings } from './slot';
import { SafetyLimiter } from './limiter';
import { CrossfadeEngine } from './engines/crossfade';
import { defaultGranularParams, GranularEngine, type GranularParams } from './engines/granular';
import { defaultSpectralParams, SpectralEngine, type SpectralParams } from './engines/spectral';
import { granularWithCharacter, spectralWithCharacter, timelineSmear } from './character';
import { Effects, type EffectsParams } from './effects';
import { SmoothRandom } from './smooth-random';
import type { EngineBlock, MorphEngine } from './engines/types';

export type EngineId = 'crossfade' | 'granular' | 'spectral';

export const MAX_SLOTS = 8;

export type TransportState = 'stopped' | 'playing' | 'paused';

/** Smoothing time for normal morph-position changes (s). */
const FRACTION_TAU = 0.03;
/** Smoothing time after a manual seek (s). */
const SEEK_TAU = 0.08;
/** Smoothing time for slot gain, mute and solo (s). */
const GAIN_TAU = 0.02;
/** Transport fade in/out time (s). */
const TRANSPORT_FADE = 0.03;
/** Declick fade time for buffer and region swaps (s). */
const DECLICK_TIME = 0.008;
/** Crossfade time when switching engines (s). */
const ENGINE_XFADE = 0.15;
/** Smoothing of the Character control (s). */
const CHARACTER_TAU = 0.08;

export interface DriftParams {
  enabled: boolean;
  /** Maximum excursion either side of the morph position (0..0.5). */
  amount: number;
  /** New random targets per minute (0.5..60). */
  speed: number;
  /** 0 = linear wandering, 1 = very smooth. */
  smoothness: number;
}

export function defaultDriftParams(): DriftParams {
  return { enabled: false, amount: 0.05, speed: 4, smoothness: 0.8 };
}

export class MorphCore {
  readonly sampleRate: number;
  readonly clock = new MorphClock();
  readonly slots: SlotRuntime[] = [];
  timeline: TimelineModel = { order: [], positions: [], segments: [] };
  gainLaw: GainLaw = 'equalPower';
  /** Seconds of glide applied to the mix when a looping morph wraps around. */
  wrapGlide = 3;
  loopCrossfadeSeconds = 0.05;
  transport: TransportState = 'stopped';

  readonly crossfade = new CrossfadeEngine();
  readonly granular: GranularEngine;
  readonly spectral: SpectralEngine;
  private engine: MorphEngine;
  /** Engine being faded out after a switch, or null. */
  private prevEngine: MorphEngine | null = null;
  private engineMix = 1;
  private scratchL = new Float32Array(128);
  private scratchR = new Float32Array(128);
  readonly effects: Effects;
  private granularBase: GranularParams = defaultGranularParams();
  private spectralBase: SpectralParams = defaultSpectralParams();
  private characterTarget = 0;
  /** Smoothed Original Character ↔ Strange Hybrid value, 0..1. */
  character = 0;
  drift: DriftParams = defaultDriftParams();
  private readonly driftLfo = new SmoothRandom(4242);
  private driftValue = 0;
  /** Morph position actually heard (clock position + drift), 0..1. */
  effectivePosition = 0;
  private readonly smearTmp = new Float32Array(MAX_SLOTS);
  private readonly limiter: SafetyLimiter;
  private masterTarget = 1;
  private masterGain = 1;
  private transportGain = 0;
  private readonly targetFractions = new Float32Array(MAX_SLOTS);
  readonly fractions = new Float32Array(MAX_SLOTS);
  private readonly fractionStart = new Float32Array(MAX_SLOTS);
  private fractionTau = FRACTION_TAU;
  private glideFrames = 0;
  private stopPending = false;
  private readonly block: EngineBlock;
  /** Last block's peak level per channel, after the limiter. */
  peakL = 0;
  peakR = 0;
  /** Lowest limiter gain in the last block (1 = not limiting). */
  limiterGain = 1;

  constructor(sampleRate: number) {
    this.sampleRate = sampleRate;
    this.limiter = new SafetyLimiter(sampleRate);
    this.granular = new GranularEngine(sampleRate);
    this.spectral = new SpectralEngine(sampleRate);
    this.effects = new Effects(sampleRate);
    this.engine = this.crossfade;
    for (let i = 0; i < MAX_SLOTS; i++) this.slots.push(new SlotRuntime());
    this.block = {
      sampleRate,
      slots: this.slots,
      fractionStart: this.fractionStart,
      fractionEnd: this.fractions,
      gainLaw: this.gainLaw,
      frozen: false,
    };
  }

  get loopCrossfadeFrames(): number {
    return Math.round(this.loopCrossfadeSeconds * this.sampleRate);
  }

  // ---- control API (called between blocks) -------------------------------

  setSample(slot: number, channels: Float32Array[] | null, settings: SlotSettings): void {
    const s = this.slots[slot];
    if (!s) return;
    if (!s.loaded || this.transport === 'stopped') {
      s.queueBuffer(channels, settings);
      s.applyPending(this.loopCrossfadeFrames);
    } else {
      s.queueBuffer(channels, settings);
    }
  }

  setSlotSettings(slot: number, settings: SlotSettings): void {
    this.slots[slot]?.queueSettings(settings, this.loopCrossfadeFrames);
  }

  setTimeline(model: TimelineModel): void {
    this.timeline = model;
  }

  get engineId(): EngineId {
    return this.engine.id as EngineId;
  }

  /** Switch engine with a short equal-power crossfade. */
  setEngine(id: EngineId): void {
    const next = this.engineById(id);
    if (next === this.engine) return;
    if (this.transportGain === 0) {
      this.engine = next;
      this.prevEngine = null;
      this.engineMix = 1;
      return;
    }
    if (this.prevEngine === next) {
      // switching back mid-fade: reverse the fade
      this.prevEngine = this.engine;
      this.engine = next;
      this.engineMix = 1 - this.engineMix;
      return;
    }
    this.prevEngine?.reset();
    next.reset();
    this.prevEngine = this.engine;
    this.engine = next;
    this.engineMix = 0;
  }

  setGranularParams(p: Partial<GranularParams>): void {
    this.granularBase = { ...this.granularBase, ...p };
    this.granular.setParams(p);
    this.applyCharacter();
  }

  setCharacter(value: number): void {
    this.characterTarget = Math.max(0, Math.min(1, value));
  }

  setDrift(p: Partial<DriftParams>): void {
    this.drift = { ...this.drift, ...p };
  }

  setEffects(p: Partial<EffectsParams>): void {
    this.effects.setParams(p);
  }

  /** Write the character-adjusted settings into the engines (allocation-free). */
  private applyCharacter(): void {
    const c = this.character;
    this.crossfade.character = c;
    granularWithCharacter(this.granularBase, c, this.granular.params);
    spectralWithCharacter(this.spectralBase, c, this.spectral.params);
  }

  setSpectralParams(p: Partial<SpectralParams>): void {
    this.spectralBase = { ...this.spectralBase, ...p };
    this.spectral.setParams(p);
    this.applyCharacter();
  }

  private engineById(id: EngineId): MorphEngine {
    return id === 'granular' ? this.granular : id === 'spectral' ? this.spectral : this.crossfade;
  }

  setGainLaw(law: GainLaw): void {
    this.gainLaw = law;
    this.block.gainLaw = law;
  }

  setMasterGain(gain: number): void {
    this.masterTarget = Math.max(0, gain);
  }

  setDuration(seconds: number): void {
    this.clock.setDuration(seconds);
  }

  setPlayMode(mode: PlayMode): void {
    this.clock.mode = mode;
    if (mode !== 'once') this.clock.ended = false;
  }

  setDirection(direction: 1 | -1): void {
    this.clock.direction = direction;
    this.clock.ended = false;
  }

  setHold(held: boolean): void {
    this.clock.held = held;
  }

  seek(position: number): void {
    this.clock.seek(position);
    this.effectivePosition = Math.min(1, Math.max(0, this.clock.position + this.driftValue));
    this.startGlide(SEEK_TAU);
  }

  play(): void {
    if (this.transport !== 'playing' && this.clock.ended) {
      this.clock.seek(this.clock.direction === 1 ? 0 : 1);
      this.startGlide(SEEK_TAU);
    }
    this.transport = 'playing';
    this.stopPending = false;
    if (this.transportGain === 0) this.snapFractions();
  }

  pause(): void {
    if (this.transport === 'playing') this.transport = 'paused';
  }

  stop(): void {
    if (this.transport === 'stopped' && this.transportGain === 0) {
      this.resetPlayback();
      return;
    }
    this.transport = 'stopped';
    this.stopPending = true;
  }

  restart(): void {
    this.clock.seek(this.clock.direction === 1 ? 0 : 1);
    this.startGlide(SEEK_TAU);
    if (this.transport !== 'playing') this.play();
  }

  // ---- audio -----------------------------------------------------------------

  /** Render `frames` of stereo audio into outL/outR (overwrites). */
  process(outL: Float32Array, outR: Float32Array, frames: number): void {
    outL.fill(0, 0, frames);
    outR.fill(0, 0, frames);
    const sr = this.sampleRate;
    const fadeStep = frames / (TRANSPORT_FADE * sr);
    const wantOn = this.transport === 'playing';
    const tg0 = this.transportGain;

    // Apply pending swaps on slots that are already silent, and always
    // while the transport is silent.
    for (let s = 0; s < MAX_SLOTS; s++) {
      const slot = this.slots[s];
      if (slot.hasPending && (tg0 === 0 || (this.fractions[s] < 1e-5 && this.targetFractions[s] < 1e-5))) {
        slot.applyPending(this.loopCrossfadeFrames);
        slot.declick = 1;
      }
    }

    if (!wantOn && tg0 === 0) {
      if (this.stopPending) this.resetPlayback();
      this.peakL = this.peakR = 0;
      this.limiterGain = 1;
      return;
    }
    const tg1 = wantOn ? Math.min(1, tg0 + fadeStep) : Math.max(0, tg0 - fadeStep);
    this.transportGain = tg1;

    const ev = this.clock.advance(frames, sr);
    if (ev.wrapped) this.startGlide(this.wrapGlide / 3);

    // Character (smoothed) → engine settings.
    const ca = 1 - Math.exp(-frames / (CHARACTER_TAU * sr));
    if (Math.abs(this.characterTarget - this.character) > 1e-5) {
      this.character += (this.characterTarget - this.character) * ca;
      this.applyCharacter();
    } else if (this.character !== this.characterTarget) {
      this.character = this.characterTarget;
      this.applyCharacter();
    }

    // Drift: a slow, continuous random offset around the morph position.
    const d = this.drift;
    const lfo = this.driftLfo.step(frames / sr, Math.max(0.5, d.speed) / 60, d.smoothness);
    const driftTarget = d.enabled ? lfo * Math.max(0, Math.min(0.5, d.amount)) : 0;
    this.driftValue += (driftTarget - this.driftValue) * (1 - Math.exp(-frames / (0.5 * sr)));
    this.effectivePosition = Math.min(1, Math.max(0, this.clock.position + this.driftValue));

    // Mix fractions: compute targets, smooth in the linear (fraction)
    // domain so the gain law is preserved during glides.
    this.computeTargets(this.effectivePosition);
    const a = 1 - Math.exp(-frames / (this.fractionTau * sr));
    if (this.glideFrames > 0) {
      this.glideFrames -= frames;
      if (this.glideFrames <= 0) this.fractionTau = FRACTION_TAU;
    }
    this.fractionStart.set(this.fractions);
    for (let s = 0; s < MAX_SLOTS; s++) {
      this.fractions[s] += (this.targetFractions[s] - this.fractions[s]) * a;
    }

    // Slot gains: user gain × mute/solo, smoothed, × declick.
    let anySolo = false;
    for (let s = 0; s < MAX_SLOTS; s++) if (this.slots[s].loaded && this.slots[s].settings.solo) anySolo = true;
    const ga = 1 - Math.exp(-frames / (GAIN_TAU * sr));
    const dStep = frames / (DECLICK_TIME * sr);
    for (let s = 0; s < MAX_SLOTS; s++) {
      const slot = this.slots[s];
      const st = slot.settings;
      const audible = !st.mute && (!anySolo || st.solo);
      const target = audible ? st.gain : 0;
      const g0 = slot.smoothGain * slot.declick;
      slot.smoothGain += (target - slot.smoothGain) * ga;
      if (slot.hasPending) {
        slot.declick = Math.max(0, slot.declick - dStep);
      } else {
        slot.declick = Math.min(1, slot.declick + dStep);
      }
      slot.blockGainStart = g0;
      slot.blockGainEnd = slot.smoothGain * slot.declick;
    }

    this.block.frozen = this.clock.held;
    if (this.prevEngine) this.renderEngineCrossfade(outL, outR, frames);
    else this.engine.process(this.block, outL, outR, frames);

    for (let s = 0; s < MAX_SLOTS; s++) {
      const slot = this.slots[s];
      if (slot.hasPending && slot.declick === 0) slot.applyPending(this.loopCrossfadeFrames);
    }

    this.effects.process(outL, outR, frames);

    // Master gain × transport fade, ramped across the block.
    const m0 = this.masterGain * tg0;
    this.masterGain += (this.masterTarget - this.masterGain) * ga;
    const m1 = this.masterGain * tg1;
    const dm = (m1 - m0) / frames;
    let m = m0;
    for (let i = 0; i < frames; i++) {
      outL[i] *= m;
      outR[i] *= m;
      m += dm;
    }

    this.limiter.process(outL, outR, frames);
    this.limiterGain = this.limiter.minGain;
    let pl = 0;
    let pr = 0;
    for (let i = 0; i < frames; i++) {
      const l = Math.abs(outL[i]);
      const r = Math.abs(outR[i]);
      if (l > pl) pl = l;
      if (r > pr) pr = r;
    }
    this.peakL = pl;
    this.peakR = pr;
  }

  // ---- internals -------------------------------------------------------------

  /**
   * Target fractions at position p. With Character in granular/spectral
   * mode the timeline is smeared (averaged at p − d, p, p + d), so samples
   * further along the timeline are interleaved earlier.
   */
  private computeTargets(p: number): void {
    computeFractions(this.timeline, p, this.targetFractions);
    const smear = timelineSmear(this.engine.id, this.character);
    if (smear < 1e-4) return;
    const t = this.targetFractions;
    const tmp = this.smearTmp;
    computeFractions(this.timeline, Math.max(0, p - smear), tmp);
    for (let s = 0; s < MAX_SLOTS; s++) t[s] += tmp[s];
    computeFractions(this.timeline, Math.min(1, p + smear), tmp);
    for (let s = 0; s < MAX_SLOTS; s++) t[s] = (t[s] + tmp[s]) / 3;
  }

  private renderEngineCrossfade(outL: Float32Array, outR: Float32Array, frames: number): void {
    if (this.scratchL.length < frames) {
      this.scratchL = new Float32Array(frames);
      this.scratchR = new Float32Array(frames);
    }
    const sl = this.scratchL;
    const sr = this.scratchR;
    sl.fill(0, 0, frames);
    sr.fill(0, 0, frames);
    this.prevEngine!.process(this.block, sl, sr, frames);
    this.engine.process(this.block, outL, outR, frames);
    const step = 1 / (ENGINE_XFADE * this.sampleRate);
    let m = this.engineMix;
    for (let i = 0; i < frames; i++) {
      const a = Math.sin(m * Math.PI * 0.5);
      const b = Math.cos(m * Math.PI * 0.5);
      outL[i] = outL[i] * a + sl[i] * b;
      outR[i] = outR[i] * a + sr[i] * b;
      m = Math.min(1, m + step);
    }
    this.engineMix = m;
    if (m >= 1) {
      this.prevEngine!.reset();
      this.prevEngine = null;
    }
  }

  private startGlide(tauSeconds: number): void {
    this.fractionTau = Math.max(FRACTION_TAU, tauSeconds);
    this.glideFrames = Math.ceil(this.fractionTau * 5 * this.sampleRate);
  }

  private snapFractions(): void {
    this.computeTargets(Math.min(1, Math.max(0, this.clock.position + this.driftValue)));
    this.fractions.set(this.targetFractions);
  }

  private resetPlayback(): void {
    this.stopPending = false;
    this.clock.seek(this.clock.direction === 1 ? 0 : 1);
    for (const slot of this.slots) slot.player.rewind();
    this.crossfade.reset();
    this.granular.reset();
    this.spectral.reset();
    this.effects.reset();
    this.prevEngine = null;
    this.engineMix = 1;
    this.snapFractions();
  }
}
