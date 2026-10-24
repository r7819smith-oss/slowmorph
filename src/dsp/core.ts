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
import type { EngineBlock, MorphEngine } from './engines/types';

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

  private engine: MorphEngine = new CrossfadeEngine();
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
    for (let i = 0; i < MAX_SLOTS; i++) this.slots.push(new SlotRuntime());
    this.block = {
      sampleRate,
      slots: this.slots,
      fractionStart: this.fractionStart,
      fractionEnd: this.fractions,
      gainLaw: this.gainLaw,
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

    // Mix fractions: compute targets, smooth in the linear (fraction)
    // domain so the gain law is preserved during glides.
    computeFractions(this.timeline, this.clock.position, this.targetFractions);
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

    this.engine.process(this.block, outL, outR, frames);

    for (let s = 0; s < MAX_SLOTS; s++) {
      const slot = this.slots[s];
      if (slot.hasPending && slot.declick === 0) slot.applyPending(this.loopCrossfadeFrames);
    }

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

  private startGlide(tauSeconds: number): void {
    this.fractionTau = Math.max(FRACTION_TAU, tauSeconds);
    this.glideFrames = Math.ceil(this.fractionTau * 5 * this.sampleRate);
  }

  private snapFractions(): void {
    computeFractions(this.timeline, this.clock.position, this.targetFractions);
    this.fractions.set(this.targetFractions);
  }

  private resetPlayback(): void {
    this.stopPending = false;
    this.clock.seek(this.clock.direction === 1 ? 0 : 1);
    for (const slot of this.slots) slot.player.rewind();
    this.engine.reset();
    this.snapFractions();
  }
}
