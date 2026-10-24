/**
 * Presets: sound settings without audio. Applying a preset keeps the loaded
 * samples and changes how they are morphed. Factory presets are below; user
 * presets are stored in this browser's localStorage.
 */

import type { ProjectState } from './project';
import type { Segment } from '../dsp/timeline';
import { defaultEffectsParams } from '../dsp/effects';
import { defaultGranularParams } from '../dsp/engines/granular';
import { defaultSpectralParams } from '../dsp/engines/spectral';
import { defaultDriftParams } from '../dsp/core';

export interface PresetSettings {
  engine: ProjectState['engine'];
  durationSec: number;
  playMode: ProjectState['playMode'];
  gainLaw: ProjectState['gainLaw'];
  loopCrossfadeMs: number;
  character: number;
  granular: ProjectState['granular'];
  spectral: ProjectState['spectral'];
  drift: ProjectState['drift'];
  effects: ProjectState['effects'];
  /** Applied to every transition. */
  segment: Segment;
}

export interface Preset {
  name: string;
  description: string;
  factory: boolean;
  settings: PresetSettings;
}

function preset(name: string, description: string, s: Partial<Omit<PresetSettings, 'granular' | 'spectral' | 'drift' | 'effects'>> & {
  granular?: Partial<PresetSettings['granular']>;
  spectral?: Partial<PresetSettings['spectral']>;
  drift?: Partial<PresetSettings['drift']>;
  effects?: Partial<PresetSettings['effects']>;
}): Preset {
  return {
    name,
    description,
    factory: true,
    settings: {
      engine: s.engine ?? 'crossfade',
      durationSec: s.durationSec ?? 600,
      playMode: s.playMode ?? 'once',
      gainLaw: s.gainLaw ?? 'equalPower',
      loopCrossfadeMs: s.loopCrossfadeMs ?? 60,
      character: s.character ?? 0.2,
      granular: { ...defaultGranularParams(), ...s.granular },
      spectral: { ...defaultSpectralParams(), ...s.spectral },
      drift: { ...defaultDriftParams(), ...s.drift },
      effects: { ...defaultEffectsParams(), ...s.effects },
      segment: s.segment ?? { curve: 'linear', shape: 0.5, tStart: 0, tEnd: 1 },
    },
  };
}

export const FACTORY_PRESETS: Preset[] = [
  preset('Slow Dissolve', 'Transparent equal-power crossfades on long S-curves; the recordings stay recognisable.', {
    engine: 'crossfade',
    durationSec: 1200,
    loopCrossfadeMs: 150,
    character: 0.1,
    segment: { curve: 'sCurve', shape: 0.6, tStart: 0, tEnd: 1 },
    effects: { reverbSend: 0.25, reverbSize: 0.85 },
  }),
  preset('Granular Cloud', 'A wide, shimmering cloud of overlapping grains passed from sample to sample.', {
    engine: 'granular',
    durationSec: 600,
    character: 0.3,
    granular: { sizeMs: 180, overlap: 8, positionRandom: 0.35, pitchRandomCents: 15, spread: 0.8, jitter: 0.7, window: 'hann' },
    effects: { reverbSend: 0.3, reverbSize: 0.8 },
  }),
  preset('Spectral Ghost', 'Blurred, slow-scanning spectral hybrids: the outlines of the sounds without their bodies.', {
    engine: 'spectral',
    durationSec: 900,
    character: 0.45,
    spectral: { fftSize: 4096, interp: 0.8, blur: 0.6, smoothing: 0.2, phaseRandom: 0.25, width: 0.7, speed: 0.25 },
    effects: { lowpassHz: 6000, reverbSend: 0.4, reverbSize: 0.9 },
  }),
  preset('Endless Drone', 'Long, almost static grains with frozen scan heads, ping-ponging for an hour.', {
    engine: 'granular',
    durationSec: 3600,
    playMode: 'pingpong',
    character: 0.15,
    granular: { sizeMs: 400, overlap: 12, positionRandom: 0.15, pitchRandomCents: 5, speed: 0.05, spread: 0.6, jitter: 0.5, window: 'gaussian' },
    drift: { enabled: true, amount: 0.08, speed: 2, smoothness: 1 },
    effects: { lowpassHz: 4000, reverbSend: 0.45, reverbSize: 0.95 },
  }),
  preset('Unstable Memory', 'Spectral hybrids that slip backwards in time, wander and smear; things half-remembered.', {
    engine: 'spectral',
    durationSec: 600,
    character: 0.7,
    segment: { curve: 'easeIn', shape: 0.5, tStart: 0, tEnd: 1 },
    spectral: { fftSize: 2048, interp: 1, envelopeLead: 0.6, blur: 0.3, phaseRandom: 0.15, warp: 0.25, speed: -0.5, width: 0.5 },
    drift: { enabled: true, amount: 0.15, speed: 8, smoothness: 0.7 },
    effects: { delaySend: 0.25, delayTime: 0.7, delayFeedback: 0.55, reverbSend: 0.25 },
  }),
  preset('Field Recording Transformation', 'Starts recognisable, then grains of each place blur into the next.', {
    engine: 'granular',
    durationSec: 1200,
    character: 0.5,
    segment: { curve: 'sCurve', shape: 0.5, tStart: 0.2, tEnd: 0.8 },
    granular: { sizeMs: 90, overlap: 6, positionRandom: 0.1, spread: 0.6, jitter: 0.4 },
    effects: { highpassHz: 80, reverbSend: 0.2, reverbSize: 0.7 },
  }),
];

export function settingsOf(state: ProjectState, segment: Segment): PresetSettings {
  const clone = <T>(x: T): T => JSON.parse(JSON.stringify(x));
  return {
    engine: state.engine,
    durationSec: state.durationSec,
    playMode: state.playMode,
    gainLaw: state.gainLaw,
    loopCrossfadeMs: state.loopCrossfadeMs,
    character: state.character,
    granular: clone(state.granular),
    spectral: clone(state.spectral),
    drift: clone(state.drift),
    effects: clone(state.effects),
    segment: { ...segment },
  };
}

const USER_KEY = 'slowmorph.userPresets.v1';

export function loadUserPresets(): Preset[] {
  try {
    const raw = localStorage.getItem(USER_KEY);
    const list = raw ? (JSON.parse(raw) as Preset[]) : [];
    return Array.isArray(list) ? list.map((p) => ({ ...p, factory: false })) : [];
  } catch {
    return [];
  }
}

export function saveUserPresets(list: Preset[]): boolean {
  try {
    localStorage.setItem(USER_KEY, JSON.stringify(list));
    return true;
  } catch {
    return false;
  }
}
