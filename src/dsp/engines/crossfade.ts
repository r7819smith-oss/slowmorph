/**
 * Mode A: crossfade engine.
 *
 * Every slot loops independently (see LoopPlayer) and the timeline only
 * changes each slot's amplitude. Fractions are converted to gains with the
 * selected gain law (equal-power or linear), multiplied by the slot's own
 * gain, and ramped linearly across each block so changes never step.
 * Silent slots keep advancing so every loop stays independent in time.
 */

import { fractionToGain } from '../curves';
import type { EngineBlock, MorphEngine } from './types';

const SILENT = 1e-6;

export class CrossfadeEngine implements MorphEngine {
  readonly id = 'crossfade';

  process(block: EngineBlock, outL: Float32Array, outR: Float32Array, frames: number): void {
    const { slots, fractionStart, fractionEnd, gainLaw } = block;
    for (let s = 0; s < slots.length; s++) {
      const slot = slots[s];
      if (!slot.loaded) continue;
      const g0 = fractionToGain(fractionStart[s], gainLaw) * slot.blockGainStart;
      const g1 = fractionToGain(fractionEnd[s], gainLaw) * slot.blockGainEnd;
      if (g0 < SILENT && g1 < SILENT) slot.player.skip(frames);
      else slot.player.process(outL, outR, 0, frames, g0, g1);
    }
  }

  reset(): void {}
}
