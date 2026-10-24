/**
 * Project state: the single source of truth on the main thread. Everything
 * the UI shows and everything sent to the audio thread derives from it.
 * Marker positions are stored in seconds so a project survives a change of
 * audio-device sample rate.
 */

import type { CurveType, GainLaw } from '../dsp/curves';
import type { PlayMode } from '../dsp/clock';
import { defaultSegment, equalSpacing, type Segment, type TimelineModel } from '../dsp/timeline';
import type { SlotSettings } from '../dsp/slot';
import { dbToGain } from '../dsp/curves';
import { defaultGranularParams, type GranularParams } from '../dsp/engines/granular';
import { defaultSpectralParams, type SpectralParams } from '../dsp/engines/spectral';

export const MIN_SLOTS = 2;
export const MAX_SLOTS = 8;
export const SLOT_LETTERS = 'ABCDEFGH';

export type EngineId = 'crossfade' | 'granular' | 'spectral';

export interface SampleAudio {
  channels: Float32Array[];
  sampleRate: number;
  frames: number;
  peak: number;
  originalSampleRate: number | null;
}

export interface SlotState {
  id: string;
  name: string;
  audio: SampleAudio | null;
  gainDb: number;
  normalize: boolean;
  reverse: boolean;
  mute: boolean;
  solo: boolean;
  /** Seconds within the sample. */
  trimStart: number;
  trimEnd: number;
  loopStart: number;
  loopEnd: number;
  error: string | null;
  warnings: string[];
  loading: boolean;
}

export interface TimelineState {
  /** When true, nodes are spread evenly whenever samples are added or removed. */
  autoSpace: boolean;
  /** Node position by slot id. */
  positions: Record<string, number>;
  /** Segment settings by "idA>idB" pair key. */
  segments: Record<string, Segment>;
  /** Curve used for new segments. */
  defaultCurve: CurveType;
}

export interface ProjectState {
  slots: SlotState[];
  durationSec: number;
  playMode: PlayMode;
  direction: 1 | -1;
  gainLaw: GainLaw;
  masterDb: number;
  loopCrossfadeMs: number;
  engine: EngineId;
  granular: GranularParams;
  spectral: SpectralParams;
  timeline: TimelineState;
}

let idCounter = 0;
export function newSlotId(): string {
  idCounter += 1;
  return `s${Date.now().toString(36)}${idCounter}`;
}

export function emptySlot(): SlotState {
  return {
    id: newSlotId(),
    name: '',
    audio: null,
    gainDb: 0,
    normalize: false,
    reverse: false,
    mute: false,
    solo: false,
    trimStart: 0,
    trimEnd: 0,
    loopStart: 0,
    loopEnd: 0,
    error: null,
    warnings: [],
    loading: false,
  };
}

export function defaultProject(): ProjectState {
  return {
    slots: [emptySlot(), emptySlot(), emptySlot()],
    durationSec: 600,
    playMode: 'once',
    direction: 1,
    gainLaw: 'equalPower',
    masterDb: -3,
    loopCrossfadeMs: 60,
    engine: 'crossfade',
    granular: defaultGranularParams(),
    spectral: defaultSpectralParams(),
    timeline: { autoSpace: true, positions: {}, segments: {}, defaultCurve: 'linear' },
  };
}

export function slotLetter(index: number): string {
  return SLOT_LETTERS[index] ?? '?';
}

/** Loaded slots in timeline order, with their slot indices. */
export function activeSlots(state: ProjectState): { slot: SlotState; index: number }[] {
  return state.slots.map((slot, index) => ({ slot, index })).filter((x) => x.slot.audio);
}

export function segmentKey(a: SlotState, b: SlotState): string {
  return `${a.id}>${b.id}`;
}

export function getSegment(state: ProjectState, a: SlotState, b: SlotState): Segment {
  return state.timeline.segments[segmentKey(a, b)] ?? { ...defaultSegment(), curve: state.timeline.defaultCurve };
}

/** Node positions of the active slots, in order. */
export function nodePositions(state: ProjectState): number[] {
  const act = activeSlots(state);
  const even = equalSpacing(act.length);
  if (state.timeline.autoSpace) return even;
  const out: number[] = [];
  let prev = -Infinity;
  act.forEach(({ slot }, i) => {
    let p = state.timeline.positions[slot.id];
    if (p === undefined || p < prev) p = Math.max(even[i], prev);
    out.push(p);
    prev = p;
  });
  return out;
}

export function timelineModel(state: ProjectState): TimelineModel {
  const act = activeSlots(state);
  const segments: Segment[] = [];
  for (let i = 0; i + 1 < act.length; i++) segments.push(getSegment(state, act[i].slot, act[i + 1].slot));
  return { order: act.map((a) => a.index), positions: nodePositions(state), segments };
}

export function slotGain(slot: SlotState): number {
  const norm = slot.normalize && slot.audio && slot.audio.peak > 0 ? 0.891 / slot.audio.peak : 1;
  return dbToGain(slot.gainDb) * norm;
}

export function slotSettings(slot: SlotState): SlotSettings {
  const sr = slot.audio?.sampleRate ?? 48000;
  const frames = slot.audio?.frames ?? 0;
  const f = (s: number) => Math.max(0, Math.min(frames, Math.round(s * sr)));
  return {
    gain: slotGain(slot),
    mute: slot.mute,
    solo: slot.solo,
    reverse: slot.reverse,
    region: { trimStart: f(slot.trimStart), loopStart: f(slot.loopStart), loopEnd: f(slot.loopEnd) },
  };
}

/**
 * Keep markers ordered: trimStart ≤ loopStart < loopEnd ≤ trimEnd, all
 * within the sample, with a minimum loop length of 20 ms.
 */
export function sanitizeMarkers(slot: SlotState): void {
  if (!slot.audio) return;
  const len = slot.audio.frames / slot.audio.sampleRate;
  const minLoop = Math.min(0.02, len);
  const c = (x: number) => Math.max(0, Math.min(len, x));
  slot.trimStart = c(slot.trimStart);
  slot.trimEnd = c(Math.max(slot.trimEnd, slot.trimStart + minLoop));
  slot.loopStart = Math.max(slot.trimStart, Math.min(c(slot.loopStart), slot.trimEnd - minLoop));
  slot.loopEnd = Math.min(slot.trimEnd, Math.max(c(slot.loopEnd), slot.loopStart + minLoop));
}
