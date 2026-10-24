/**
 * Contract between the morph core and an engine.
 *
 * The core owns the clock, the timeline, the slots and their gains; an
 * engine only decides how the per-slot mix fractions turn into sound. Each
 * engine can therefore be developed and tested on its own.
 */

import type { GainLaw } from '../curves';
import type { SlotRuntime } from '../slot';

export interface EngineBlock {
  sampleRate: number;
  slots: readonly SlotRuntime[];
  /** Smoothed mix fraction of each slot at the start of the block. */
  fractionStart: Float32Array;
  /** Smoothed mix fraction of each slot at the end of the block. */
  fractionEnd: Float32Array;
  gainLaw: GainLaw;
}

export interface MorphEngine {
  readonly id: string;
  /** Add one block of stereo audio into outL/outR (which start at zero). */
  process(block: EngineBlock, outL: Float32Array, outR: Float32Array, frames: number): void;
  /** Called when playback is stopped and every voice should reset. */
  reset(): void;
}
