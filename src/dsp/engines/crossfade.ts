/**
 * Mode A: crossfade engine.
 *
 * Every slot loops independently (see LoopPlayer) and the timeline only
 * changes each slot's amplitude. Fractions are converted to gains with the
 * selected gain law (equal-power or linear), multiplied by the slot's own
 * gain, and ramped linearly across each block so changes never step.
 * Silent slots keep advancing so every loop stays independent in time.
 *
 * Character > 0 adds, per slot, a gentle two-pole low-pass whose cutoff
 * wanders slowly (20 kHz → as low as 1.5 kHz at 100 %) and a slow amplitude
 * shimmer (up to ±2.5 dB). The engine stays amplitude-based: the timeline
 * gains are never changed.
 */

import { fractionToGain } from '../curves';
import { SmoothRandom } from '../smooth-random';
import type { EngineBlock, MorphEngine } from './types';

const SILENT = 1e-6;
const MAX_SLOTS = 8;

class SlotColour {
  readonly cutoffLfo: SmoothRandom;
  readonly ampLfo: SmoothRandom;
  l1 = 0; l2 = 0; r1 = 0; r2 = 0;
  coef = 0;
  active = false;
  constructor(seed: number) {
    this.cutoffLfo = new SmoothRandom(seed * 7919 + 1);
    this.ampLfo = new SmoothRandom(seed * 104729 + 3);
  }
}

export class CrossfadeEngine implements MorphEngine {
  readonly id = 'crossfade';
  /** Original Character ↔ Strange Hybrid, 0..1 (set by the core each block). */
  character = 0;
  private readonly colour: SlotColour[] = [];
  private scratchL = new Float32Array(128);
  private scratchR = new Float32Array(128);

  constructor() {
    for (let i = 0; i < MAX_SLOTS; i++) this.colour.push(new SlotColour(i + 1));
  }

  process(block: EngineBlock, outL: Float32Array, outR: Float32Array, frames: number): void {
    const { slots, fractionStart, fractionEnd, gainLaw, sampleRate } = block;
    const c = this.character;
    if (this.scratchL.length < frames) {
      this.scratchL = new Float32Array(frames);
      this.scratchR = new Float32Array(frames);
    }
    for (let s = 0; s < slots.length; s++) {
      const slot = slots[s];
      if (!slot.loaded) continue;
      let g0 = fractionToGain(fractionStart[s], gainLaw) * slot.blockGainStart;
      let g1 = fractionToGain(fractionEnd[s], gainLaw) * slot.blockGainEnd;
      const col = this.colour[s];
      if (c < 1e-3) {
        col.active = false;
        if (g0 < SILENT && g1 < SILENT) slot.player.skip(frames);
        else slot.player.process(outL, outR, 0, frames, g0, g1);
        continue;
      }
      // coloured path
      const dt = frames / sampleRate;
      const cut = col.cutoffLfo.step(dt, 0.07, 1); // −1..1, ~14 s per move
      const amp = col.ampLfo.step(dt, 0.11, 1);
      const shimmer = Math.pow(10, (2.5 * c * amp) / 20);
      g0 *= shimmer;
      g1 *= shimmer;
      if (g0 < SILENT && g1 < SILENT) {
        slot.player.skip(frames);
        continue;
      }
      const sl = this.scratchL;
      const sr = this.scratchR;
      sl.fill(0, 0, frames);
      sr.fill(0, 0, frames);
      slot.player.process(sl, sr, 0, frames, g0, g1);
      const cutoff = 20000 * Math.pow(1500 / 20000, c * (0.5 + 0.5 * cut));
      const target = 1 - Math.exp((-2 * Math.PI * Math.min(cutoff, sampleRate * 0.45)) / sampleRate);
      if (!col.active) {
        col.l1 = col.l2 = sl[0];
        col.r1 = col.r2 = sr[0];
        col.coef = target;
        col.active = true;
      }
      // Blend in the coloured signal over the first 10 % of the control so
      // the switch from the plain path is continuous.
      const wet = Math.min(1, c * 10);
      const dry = 1 - wet;
      const dc = (target - col.coef) / frames;
      let k = col.coef;
      let l1 = col.l1, l2 = col.l2, r1 = col.r1, r2 = col.r2;
      for (let i = 0; i < frames; i++) {
        k += dc;
        l1 += (sl[i] - l1) * k;
        l2 += (l1 - l2) * k;
        r1 += (sr[i] - r1) * k;
        r2 += (r1 - r2) * k;
        outL[i] += l2 * wet + sl[i] * dry;
        outR[i] += r2 * wet + sr[i] * dry;
      }
      col.coef = k;
      col.l1 = l1; col.l2 = l2; col.r1 = r1; col.r2 = r2;
    }
  }

  reset(): void {
    for (const col of this.colour) col.active = false;
  }
}
