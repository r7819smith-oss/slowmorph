/**
 * Portable project files (.slowmorph).
 *
 * Layout (little-endian):
 *   "SLOWMORPH\0"            10-byte magic
 *   u32 format version       (1)
 *   u32 JSON byte length
 *   JSON (UTF-8)             all settings + per-slot audio descriptors
 *   zero padding to a multiple of 4
 *   audio data               32-bit float PCM, one block per channel per slot
 *
 * Audio is stored losslessly at its working sample rate; on load it is
 * resampled if the audio device runs at a different rate. Unknown or missing
 * fields fall back to defaults so older files keep loading.
 */

import { defaultProject, emptySlot, MAX_SLOTS, MIN_SLOTS, type ProjectState, type SlotState } from './project';

const MAGIC = 'SLOWMORPH\0';
export const PROJECT_VERSION = 1;
export const PROJECT_EXTENSION = '.slowmorph';

export class ProjectFileError extends Error {}

interface AudioDescriptor {
  sampleRate: number;
  frames: number;
  channels: number;
  peak: number;
  originalSampleRate: number | null;
  /** Byte offset of this slot's first channel within the audio data. */
  offset: number;
}

type SlotJson = Omit<SlotState, 'audio' | 'loading' | 'error' | 'warnings'> & { audio: AudioDescriptor | null };

interface ProjectJson {
  format: 'slowmorph-project';
  version: number;
  savedAt: string;
  app: string;
  settings: Omit<ProjectState, 'slots'>;
  slots: SlotJson[];
}

export function serializeProject(state: ProjectState, now = new Date()): ArrayBuffer {
  let offset = 0;
  const slots: SlotJson[] = state.slots.map((s) => {
    const { audio, loading: _l, error: _e, warnings: _w, ...rest } = s;
    void _l; void _e; void _w;
    if (!audio) return { ...rest, audio: null };
    const d: AudioDescriptor = {
      sampleRate: audio.sampleRate,
      frames: audio.frames,
      channels: audio.channels.length,
      peak: audio.peak,
      originalSampleRate: audio.originalSampleRate,
      offset,
    };
    offset += audio.frames * audio.channels.length * 4;
    return { ...rest, audio: d };
  });
  const { slots: _s, ...settings } = state;
  void _s;
  const json: ProjectJson = { format: 'slowmorph-project', version: PROJECT_VERSION, savedAt: now.toISOString(), app: 'SlowMorph', settings, slots };
  const jsonBytes = new TextEncoder().encode(JSON.stringify(json));
  const headLen = MAGIC.length + 8 + jsonBytes.length;
  const pad = (4 - (headLen % 4)) % 4;
  const total = headLen + pad + offset;
  const buf = new ArrayBuffer(total);
  const u8 = new Uint8Array(buf);
  const v = new DataView(buf);
  for (let i = 0; i < MAGIC.length; i++) u8[i] = MAGIC.charCodeAt(i);
  v.setUint32(MAGIC.length, PROJECT_VERSION, true);
  v.setUint32(MAGIC.length + 4, jsonBytes.length, true);
  u8.set(jsonBytes, MAGIC.length + 8);
  const dataStart = headLen + pad;
  state.slots.forEach((s, i) => {
    const d = slots[i].audio;
    if (!s.audio || !d) return;
    s.audio.channels.forEach((ch, c) => {
      new Float32Array(buf, dataStart + d.offset + c * d.frames * 4, d.frames).set(ch.subarray(0, d.frames));
    });
  });
  return buf;
}

/** Parse a project file. Audio comes back at the rate it was saved at. */
export function deserializeProject(buf: ArrayBuffer): ProjectState {
  const u8 = new Uint8Array(buf);
  if (buf.byteLength < MAGIC.length + 8) throw new ProjectFileError('This file is too short to be a SlowMorph project.');
  for (let i = 0; i < MAGIC.length; i++) {
    if (u8[i] !== MAGIC.charCodeAt(i)) throw new ProjectFileError('This is not a SlowMorph project file.');
  }
  const v = new DataView(buf);
  const version = v.getUint32(MAGIC.length, true);
  if (version > PROJECT_VERSION) throw new ProjectFileError(`This project was saved by a newer SlowMorph (format ${version}).`);
  const jsonLen = v.getUint32(MAGIC.length + 4, true);
  const headLen = MAGIC.length + 8 + jsonLen;
  if (headLen > buf.byteLength) throw new ProjectFileError('The project file is truncated.');
  let json: ProjectJson;
  try {
    json = JSON.parse(new TextDecoder().decode(u8.subarray(MAGIC.length + 8, headLen)));
  } catch {
    throw new ProjectFileError('The project settings could not be read (corrupted file).');
  }
  if (json.format !== 'slowmorph-project' || !Array.isArray(json.slots)) throw new ProjectFileError('The project file is not in the expected format.');
  const dataStart = headLen + ((4 - (headLen % 4)) % 4);

  const base = defaultProject();
  const st = json.settings ?? ({} as ProjectJson['settings']);
  const state: ProjectState = {
    ...base,
    ...pick(st, base),
    granular: { ...base.granular, ...(st.granular ?? {}) },
    spectral: { ...base.spectral, ...(st.spectral ?? {}) },
    drift: { ...base.drift, ...(st.drift ?? {}) },
    effects: { ...base.effects, ...(st.effects ?? {}) },
    timeline: { ...base.timeline, ...(st.timeline ?? {}) },
    midi: Array.isArray(st.midi) ? st.midi : [],
    slots: [],
  };
  for (const sj of json.slots.slice(0, MAX_SLOTS)) {
    const slot: SlotState = { ...emptySlot(), ...pick(sj, emptySlot()), audio: null, loading: false, error: null, warnings: [] };
    if (typeof sj.id === 'string') slot.id = sj.id;
    const d = sj.audio;
    if (d) {
      const bytes = d.frames * d.channels * 4;
      if (d.frames <= 0 || d.channels < 1 || d.channels > 2 || dataStart + d.offset + bytes > buf.byteLength) {
        slot.error = `The audio for "${sj.name}" is missing from the project file.`;
      } else {
        const channels: Float32Array[] = [];
        for (let c = 0; c < d.channels; c++) {
          // copy so the project's buffer can be released
          channels.push(new Float32Array(buf.slice(dataStart + d.offset + c * d.frames * 4, dataStart + d.offset + (c + 1) * d.frames * 4)));
        }
        slot.audio = { channels, sampleRate: d.sampleRate, frames: d.frames, peak: d.peak, originalSampleRate: d.originalSampleRate };
      }
    }
    state.slots.push(slot);
  }
  while (state.slots.length < MIN_SLOTS) state.slots.push(emptySlot());
  return state;
}

/** Copy only keys that exist in `shape` and have the same primitive type. */
function pick<T extends object>(src: unknown, shape: T): Partial<T> {
  const out: Partial<T> = {};
  if (!src || typeof src !== 'object') return out;
  for (const k of Object.keys(shape) as (keyof T)[]) {
    const v = (src as Record<string, unknown>)[k as string];
    const ref = shape[k];
    if (v === undefined || v === null) continue;
    if (typeof ref === 'object' && ref !== null) continue; // nested objects are merged explicitly
    if (typeof v === typeof ref) out[k] = v as T[keyof T];
  }
  return out;
}
