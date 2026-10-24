/**
 * MIDI control: a registry of performable parameters, MIDI-learn, and the
 * mapping from incoming CC / note messages to app actions. Uses Web MIDI
 * (Chrome and Edge); everything else keeps working without it.
 */

import type { App } from '../app';

export type ParamId =
  | 'morphPosition'
  | 'character'
  | 'grainSize'
  | 'grainDensity'
  | 'spectralBlur'
  | 'filterCutoff'
  | 'freeze'
  | 'playback'
  | 'driftAmount'
  | 'reverbSend'
  | 'masterLevel';

export interface MidiMapping {
  param: ParamId;
  /** MIDI channel 0–15. */
  channel: number;
  type: 'cc' | 'note';
  /** Controller or note number. */
  number: number;
}

export interface ParamDef {
  id: ParamId;
  label: string;
  kind: 'continuous' | 'toggle';
  /** Apply a normalised value (0..1; toggles: ≥ 0.5 = on). */
  apply(app: App, v: number): void;
}

export const PARAMS: ParamDef[] = [
  { id: 'morphPosition', label: 'Morph position', kind: 'continuous', apply: (a, v) => a.seek(v) },
  { id: 'character', label: 'Character ↔ Hybrid', kind: 'continuous', apply: (a, v) => a.setCharacter(v) },
  { id: 'grainSize', label: 'Grain size', kind: 'continuous', apply: (a, v) => a.setGranular({ sizeMs: Math.round(10 * Math.pow(50, v)) }) },
  { id: 'grainDensity', label: 'Grain density', kind: 'continuous', apply: (a, v) => a.setGranular({ density: 0.02 + 0.98 * v }) },
  { id: 'spectralBlur', label: 'Spectral blur', kind: 'continuous', apply: (a, v) => a.setSpectral({ blur: v }) },
  { id: 'filterCutoff', label: 'Low-pass cutoff', kind: 'continuous', apply: (a, v) => a.setEffects({ lowpassHz: Math.round(40 * Math.pow(500, v)) }) },
  { id: 'driftAmount', label: 'Drift amount', kind: 'continuous', apply: (a, v) => a.setDrift({ amount: 0.5 * v, enabled: v > 0.005 }) },
  { id: 'reverbSend', label: 'Reverb send', kind: 'continuous', apply: (a, v) => a.setEffects({ reverbSend: v }) },
  { id: 'masterLevel', label: 'Output level', kind: 'continuous', apply: (a, v) => a.setMasterDb(-48 + 54 * v) },
  { id: 'freeze', label: 'Freeze', kind: 'toggle', apply: (a, v) => a.setFreeze(v >= 0.5) },
  { id: 'playback', label: 'Play / pause', kind: 'toggle', apply: (a, v) => (v >= 0.5 ? a.play() : a.pause()) },
];

export function paramDef(id: ParamId): ParamDef | undefined {
  return PARAMS.find((p) => p.id === id);
}

export interface ParsedMidi {
  type: 'cc' | 'note';
  channel: number;
  number: number;
  /** 0..1 */
  value: number;
  /** Note-on (true) or note-off (false); always true for CC. */
  on: boolean;
}

/** Parse a raw MIDI message; returns null for messages SlowMorph ignores. */
export function parseMidi(data: ArrayLike<number>): ParsedMidi | null {
  if (data.length < 3) return null;
  const status = data[0] & 0xf0;
  const channel = data[0] & 0x0f;
  if (status === 0xb0) return { type: 'cc', channel, number: data[1], value: data[2] / 127, on: true };
  if (status === 0x90 && data[2] > 0) return { type: 'note', channel, number: data[1], value: 1, on: true };
  if (status === 0x80 || (status === 0x90 && data[2] === 0)) return { type: 'note', channel, number: data[1], value: 0, on: false };
  return null;
}

/**
 * Route a parsed message: while learning, bind it to the learn target;
 * otherwise apply every mapping that matches. Notes toggle toggle-params
 * on note-on and are ignored for continuous params.
 * Returns the mappings list (possibly changed).
 */
export function routeMidi(
  msg: ParsedMidi,
  mappings: MidiMapping[],
  learning: ParamId | null,
  apply: (def: ParamDef, value: number) => void,
  currentToggle: (id: ParamId) => boolean,
): { mappings: MidiMapping[]; learned: boolean } {
  if (learning) {
    if (msg.type === 'note' && !msg.on) return { mappings, learned: false };
    const next = mappings.filter((m) => m.param !== learning && !(m.type === msg.type && m.channel === msg.channel && m.number === msg.number));
    next.push({ param: learning, channel: msg.channel, type: msg.type, number: msg.number });
    return { mappings: next, learned: true };
  }
  for (const m of mappings) {
    if (m.type !== msg.type || m.channel !== msg.channel || m.number !== msg.number) continue;
    const def = paramDef(m.param);
    if (!def) continue;
    if (msg.type === 'note') {
      if (def.kind === 'toggle' && msg.on) apply(def, currentToggle(def.id) ? 0 : 1);
    } else apply(def, msg.value);
  }
  return { mappings, learned: false };
}

export type MidiStatus = 'off' | 'unsupported' | 'denied' | 'on';

export class MidiController {
  status: MidiStatus = 'off';
  learning: ParamId | null = null;
  inputs: string[] = [];
  onChange: () => void = () => {};

  constructor(private readonly app: App) {}

  async enable(): Promise<void> {
    if (!('requestMIDIAccess' in navigator)) {
      this.status = 'unsupported';
      this.onChange();
      return;
    }
    try {
      const access = await navigator.requestMIDIAccess({ sysex: false });
      const bind = () => {
        this.inputs = [];
        access.inputs.forEach((input) => {
          this.inputs.push(input.name ?? 'MIDI input');
          input.onmidimessage = (e) => e.data && this.handle(e.data);
        });
        this.onChange();
      };
      access.onstatechange = bind;
      bind();
      this.status = 'on';
    } catch {
      this.status = 'denied';
    }
    this.onChange();
  }

  learn(param: ParamId | null): void {
    this.learning = param;
    this.onChange();
  }

  handle(data: ArrayLike<number>): void {
    const msg = parseMidi(data);
    if (!msg) return;
    const res = routeMidi(
      msg,
      this.app.state.midi,
      this.learning,
      (def, v) => def.apply(this.app, v),
      (id) => (id === 'freeze' ? this.app.live.frozen : this.app.live.transport === 'playing'),
    );
    if (res.learned) {
      this.app.setMidiMappings(res.mappings);
      this.learning = null;
      this.onChange();
    }
  }
}
