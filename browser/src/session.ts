/**
 * Projects, presets, recording and offline rendering: everything that
 * reads or writes files. Kept apart from App so the instrument itself has
 * no file-handling code in its hot paths.
 */

import type { App } from './app';
import { resample } from './audio/decode';
import { downloadBlob, openWavSink, type WavSink } from './audio/wav-sink';
import { encodePcm, type WavFormat } from './audio/wav';
import { deserializeProject, PROJECT_EXTENSION, serializeProject } from './state/serialize';
import { FACTORY_PRESETS, loadUserPresets, saveUserPresets, settingsOf, type Preset } from './state/presets';
import { activeSlots, getSegment } from './state/project';
import { stateMessages } from './state/sync';
import { formatTime } from './dsp/clock';
import { defaultSegment } from './dsp/timeline';

export interface RecordingStatus {
  active: boolean;
  mode: 'live' | 'fullMorph';
  seconds: number;
  maxSeconds: number | null;
  destination: string;
  message: string | null;
}

export interface RenderStatus {
  active: boolean;
  progress: number;
  message: string | null;
}

interface FilePickerWindow extends Window {
  showSaveFilePicker?: (o: unknown) => Promise<{ name: string; createWritable(): Promise<{ write(d: unknown): Promise<void>; close(): Promise<void> }> }>;
  showOpenFilePicker?: (o: unknown) => Promise<{ getFile(): Promise<File> }[]>;
}

export class Session {
  projectName = 'Untitled';
  format: WavFormat = 'pcm24';
  recording: RecordingStatus = { active: false, mode: 'live', seconds: 0, maxSeconds: null, destination: '', message: null };
  render: RenderStatus = { active: false, progress: 0, message: null };
  userPresets: Preset[] = loadUserPresets();
  message: string | null = null;
  onChange: () => void = () => {};
  private sink: WavSink | null = null;
  private recFrames = 0;
  private worker: Worker | null = null;
  private endWatch = 0;

  constructor(private readonly app: App) {
    app.onStatus(() => this.tick());
  }

  private changed(): void {
    this.onChange();
  }

  private say(msg: string | null): void {
    this.message = msg;
    this.changed();
  }

  // ---- projects --------------------------------------------------------------------------

  async saveProject(): Promise<void> {
    const buf = serializeProject(this.app.state);
    const name = `${this.projectName || 'Untitled'}${PROJECT_EXTENSION}`;
    const w = window as FilePickerWindow;
    try {
      if (w.showSaveFilePicker) {
        const h = await w.showSaveFilePicker({ suggestedName: name, types: [{ description: 'SlowMorph project', accept: { 'application/octet-stream': [PROJECT_EXTENSION] } }] });
        const writable = await h.createWritable();
        await writable.write(buf);
        await writable.close();
        this.projectName = h.name.replace(/\.slowmorph$/i, '');
      } else downloadBlob(new Blob([buf]), name);
      this.say(`Saved “${this.projectName}” (${(buf.byteLength / 1048576).toFixed(1)} MB, samples included).`);
    } catch (e) {
      if ((e as Error).name !== 'AbortError') this.say(`Could not save the project: ${(e as Error).message}`);
    }
  }

  async openProjectFile(file: File): Promise<void> {
    try {
      const state = deserializeProject(await file.arrayBuffer());
      const rate = this.app.engine.sampleRate;
      for (const slot of state.slots) {
        if (slot.audio && slot.audio.sampleRate !== rate) {
          const channels = await resample({ sampleRate: slot.audio.sampleRate, channels: slot.audio.channels }, rate);
          slot.audio = { ...slot.audio, channels, sampleRate: rate, frames: channels[0].length };
        }
      }
      this.app.loadState(state);
      this.projectName = file.name.replace(/\.slowmorph$/i, '');
      const missing = state.slots.filter((s) => s.error).length;
      this.say(missing ? `Opened “${this.projectName}”, but ${missing} sample(s) could not be restored.` : `Opened “${this.projectName}”.`);
    } catch (e) {
      this.say(`Could not open “${file.name}”: ${(e as Error).message}`);
    }
  }

  // ---- presets ----------------------------------------------------------------------------

  get presets(): Preset[] {
    return [...FACTORY_PRESETS, ...this.userPresets];
  }

  applyPreset(p: Preset): void {
    this.app.applyPresetSettings(p.settings);
    this.say(`Preset “${p.name}” applied to the loaded samples.`);
  }

  saveUserPreset(name: string): void {
    const act = activeSlots(this.app.state);
    const seg = act.length > 1 ? getSegment(this.app.state, act[0].slot, act[1].slot) : defaultSegment();
    const preset: Preset = { name, description: 'User preset', factory: false, settings: settingsOf(this.app.state, seg) };
    this.userPresets = [...this.userPresets.filter((p) => p.name !== name), preset];
    if (!saveUserPresets(this.userPresets)) this.say('This browser would not store the preset (storage blocked or full).');
    else this.say(`Saved preset “${name}”.`);
  }

  deleteUserPreset(name: string): void {
    this.userPresets = this.userPresets.filter((p) => p.name !== name);
    saveUserPresets(this.userPresets);
    this.changed();
  }

  // ---- recording -------------------------------------------------------------------------

  private stamp(): string {
    const d = new Date();
    const p = (n: number) => String(n).padStart(2, '0');
    return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}.${p(d.getMinutes())}.${p(d.getSeconds())}`;
  }

  /** Start recording the output. Must be called from a click (file dialog). */
  async startRecording(opts: { maxSeconds: number | null; fullMorph: boolean }): Promise<void> {
    if (this.recording.active || this.render.active) return;
    const rec = this.app.engine.recorder;
    if (!rec) return this.say('The audio engine is still starting.');
    const sink = await openWavSink(`${this.projectName} ${this.stamp()}.wav`);
    if (!sink) return;
    this.sink = sink;
    this.recFrames = 0;
    const format = this.format;
    const max = opts.fullMorph ? null : opts.maxSeconds;
    this.recording = { active: true, mode: opts.fullMorph ? 'fullMorph' : 'live', seconds: 0, maxSeconds: max, destination: sink.kind === 'file' ? sink.name : 'memory', message: null };
    rec.start((l, r) => {
      if (!this.recording.active || !this.sink) return;
      void this.sink.write(encodePcm(l, r, format));
      this.recFrames += l.length;
    });
    if (opts.fullMorph) {
      this.app.setPlayMode('once');
      this.app.setDirection(1);
      this.app.setFreeze(false);
      this.app.restart();
      this.endWatch = 0;
    } else if (this.app.live.transport !== 'playing') this.app.play();
    this.changed();
  }

  async stopRecording(): Promise<void> {
    if (!this.recording.active || !this.sink) return;
    const rec = this.app.engine.recorder!;
    this.recording.active = false;
    await rec.stop();
    const sink = this.sink;
    this.sink = null;
    const seconds = this.recFrames / this.app.engine.sampleRate;
    const blob = await sink.finish(this.recFrames, this.app.engine.sampleRate, this.format);
    if (blob) downloadBlob(blob, sink.name);
    this.recording.message = `Recorded ${formatTime(seconds)} → ${sink.kind === 'file' ? sink.name : `${sink.name} (downloaded)`}`;
    this.changed();
  }

  private tick(): void {
    if (!this.recording.active) return;
    const sr = this.app.engine.sampleRate;
    this.recording.seconds = this.recFrames / sr;
    const max = this.recording.maxSeconds;
    if (max && this.recording.seconds >= max) void this.stopRecording();
    if (this.recording.mode === 'fullMorph' && this.app.live.ended) {
      // keep 4 s after the end for the final sample and effect tails
      if (!this.endWatch) this.endWatch = this.recording.seconds;
      else if (this.recording.seconds - this.endWatch > 4) void this.stopRecording();
    }
    this.changed();
  }

  // ---- offline render ---------------------------------------------------------------------

  /** Render the whole morph faster than real time to a WAV file. */
  async renderOffline(): Promise<void> {
    if (this.render.active || this.recording.active) return;
    if (!activeSlots(this.app.state).length) return this.say('Load at least one sample first.');
    const sink = await openWavSink(`${this.projectName} full morph ${formatTime(this.app.state.durationSec).replace(/:/g, '.')}.wav`);
    if (!sink) return;
    const sr = this.app.engine.sampleRate;
    const format = this.format;
    this.render = { active: true, progress: 0, message: null };
    this.changed();
    const worker = new Worker(new URL('./audio/offline-worker.ts', import.meta.url), { type: 'module' });
    this.worker = worker;
    const t0 = performance.now();
    let frames = 0;
    worker.onmessage = async (e: MessageEvent) => {
      const d = e.data;
      if (d.type === 'chunk') {
        frames += d.frames;
        await sink.write(d.bytes);
        this.render.progress = d.done / d.total;
        this.changed();
      } else if (d.type === 'done') {
        const blob = await sink.finish(frames, sr, format);
        if (blob) downloadBlob(blob, sink.name);
        const secs = (performance.now() - t0) / 1000;
        const audio = frames / sr;
        this.render = { active: false, progress: 1, message: `Rendered ${formatTime(audio)} in ${formatTime(secs)} (${(audio / Math.max(0.001, secs)).toFixed(1)}× real time) → ${sink.name}` };
        worker.terminate();
        this.worker = null;
        this.changed();
      } else if (d.type === 'cancelled' || d.type === 'error') {
        await sink.abort();
        this.render = { active: false, progress: 0, message: d.type === 'error' ? `Render failed: ${d.message}` : 'Render cancelled.' };
        worker.terminate();
        this.worker = null;
        this.changed();
      }
    };
    // Copy the sample data for the worker (the live instrument keeps its own).
    worker.postMessage({ type: 'start', messages: stateMessages(this.app.state), sampleRate: sr, tailSeconds: 4, format });
  }

  cancelRender(): void {
    this.worker?.postMessage({ type: 'cancel' });
  }
}
