/** Messages between the main thread and the SlowMorph AudioWorklet. */

import type { GainLaw } from '../dsp/curves';
import type { PlayMode } from '../dsp/clock';
import type { TimelineModel } from '../dsp/timeline';
import type { SlotSettings } from '../dsp/slot';
import type { DriftParams, EngineId, TransportState } from '../dsp/core';
import type { EffectsParams } from '../dsp/effects';
import type { GranularParams } from '../dsp/engines/granular';
import type { SpectralParams } from '../dsp/engines/spectral';

export type ToWorklet =
  | { type: 'sample'; slot: number; channels: Float32Array[] | null; settings: SlotSettings }
  | { type: 'slotSettings'; slot: number; settings: SlotSettings }
  | { type: 'timeline'; model: TimelineModel }
  | { type: 'gainLaw'; law: GainLaw }
  | { type: 'master'; gain: number }
  | { type: 'duration'; seconds: number }
  | { type: 'playMode'; mode: PlayMode }
  | { type: 'direction'; direction: 1 | -1 }
  | { type: 'hold'; held: boolean }
  | { type: 'seek'; position: number }
  | { type: 'loopCrossfade'; seconds: number }
  | { type: 'engine'; engine: EngineId }
  | { type: 'granular'; params: Partial<GranularParams> }
  | { type: 'spectral'; params: Partial<SpectralParams> }
  | { type: 'character'; value: number }
  | { type: 'drift'; params: Partial<DriftParams> }
  | { type: 'effects'; params: Partial<EffectsParams> }
  | { type: 'transport'; command: 'play' | 'pause' | 'stop' | 'restart' };

export interface WorkletStatus {
  type: 'status';
  position: number;
  direction: 1 | -1;
  transport: TransportState;
  ended: boolean;
  fractions: number[];
  peakL: number;
  peakR: number;
  limiterGain: number;
  /** Number of sounding grains (granular engine). */
  grains: number;
  /** Position actually heard, including drift. */
  effectivePosition: number;
}

export type FromWorklet = WorkletStatus;
