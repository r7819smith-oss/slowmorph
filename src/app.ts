/**
 * App controller: owns the project state and the audio engine. UI code
 * calls these actions; each action updates the state, sends the matching
 * message to the audio thread and notifies listeners.
 */

import { AudioEngine, type ContextState } from './audio/audio-engine';
import { decodeFile } from './audio/decode';
import type { WorkletStatus } from './audio/messages';
import type { GainLaw } from './dsp/curves';
import type { GranularParams } from './dsp/engines/granular';
import type { SpectralParams } from './dsp/engines/spectral';
import type { PlayMode } from './dsp/clock';
import { moveNode as moveNodePos, type Segment } from './dsp/timeline';
import {
  activeSlots,
  defaultProject,
  emptySlot,
  getSegment,
  MAX_SLOTS,
  MIN_SLOTS,
  nodePositions,
  sanitizeMarkers,
  slotSettings,
  timelineModel,
  type EngineId,
  type ProjectState,
  type SlotState,
} from './state/project';
import { dbToGain } from './dsp/curves';

export type ChangeKind = 'slots' | 'timeline' | 'transport' | 'settings' | 'all';

export interface LiveStatus {
  position: number;
  direction: 1 | -1;
  transport: WorkletStatus['transport'];
  ended: boolean;
  fractions: number[];
  peakL: number;
  peakR: number;
  limiting: boolean;
  frozen: boolean;
  grains: number;
  contextState: ContextState;
  contextMessage: string | null;
}

export class App {
  state: ProjectState = defaultProject();
  readonly engine = new AudioEngine();
  live: LiveStatus = {
    position: 0,
    direction: 1,
    transport: 'stopped',
    ended: false,
    fractions: new Array(MAX_SLOTS).fill(0),
    peakL: 0,
    peakR: 0,
    limiting: false,
    frozen: false,
    grains: 0,
    contextState: 'suspended',
    contextMessage: null,
  };
  private listeners = new Set<(kind: ChangeKind) => void>();
  private statusListeners = new Set<() => void>();

  constructor() {
    this.engine.onStatus = (s) => {
      this.live.position = s.position;
      this.live.direction = s.direction;
      const transportChanged = this.live.transport !== s.transport || this.live.ended !== s.ended;
      this.live.transport = s.transport;
      this.live.ended = s.ended;
      this.live.fractions = s.fractions;
      this.live.peakL = s.peakL;
      this.live.peakR = s.peakR;
      this.live.limiting = s.limiterGain < 0.98;
      this.live.grains = s.grains;
      if (this.state.direction !== s.direction) this.state.direction = s.direction;
      if (transportChanged) this.emit('transport');
      for (const l of this.statusListeners) l();
    };
    this.engine.onStateChange = (st, detail) => {
      this.live.contextState = st;
      this.live.contextMessage = detail ?? null;
      this.emit('transport');
    };
    this.live.contextState = this.engine.ctx.state as ContextState;
    this.syncAll();
  }

  on(fn: (kind: ChangeKind) => void): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  onStatus(fn: () => void): () => void {
    this.statusListeners.add(fn);
    return () => this.statusListeners.delete(fn);
  }

  private emit(kind: ChangeKind): void {
    for (const l of this.listeners) l(kind);
  }

  /** Push the whole state to the audio thread. */
  private syncAll(): void {
    const s = this.state;
    const post = this.engine.post.bind(this.engine);
    s.slots.forEach((slot, i) => post({ type: 'sample', slot: i, channels: slot.audio?.channels ?? null, settings: slotSettings(slot) }));
    for (let i = s.slots.length; i < MAX_SLOTS; i++) post({ type: 'sample', slot: i, channels: null, settings: slotSettings(emptySlot()) });
    post({ type: 'duration', seconds: s.durationSec });
    post({ type: 'playMode', mode: s.playMode });
    post({ type: 'direction', direction: s.direction });
    post({ type: 'gainLaw', law: s.gainLaw });
    post({ type: 'master', gain: dbToGain(s.masterDb) });
    post({ type: 'loopCrossfade', seconds: s.loopCrossfadeMs / 1000 });
    post({ type: 'granular', params: s.granular });
    post({ type: 'spectral', params: s.spectral });
    post({ type: 'engine', engine: s.engine });
    this.syncTimeline();
  }

  private syncTimeline(): void {
    this.engine.post({ type: 'timeline', model: timelineModel(this.state) });
  }

  private syncSlot(i: number): void {
    this.engine.post({ type: 'slotSettings', slot: i, settings: slotSettings(this.state.slots[i]) });
  }

  // ---- samples -----------------------------------------------------------------

  async loadFile(index: number, file: File): Promise<void> {
    const slot = this.state.slots[index];
    if (!slot) return;
    void this.engine.resume();
    slot.loading = true;
    slot.error = null;
    this.emit('slots');
    try {
      const d = await decodeFile(file, this.engine.sampleRate);
      const wasActive = !!slot.audio;
      slot.audio = { channels: d.channels, sampleRate: d.sampleRate, frames: d.frames, peak: d.peak, originalSampleRate: d.originalSampleRate };
      slot.name = d.name.replace(/\.[^.]+$/, '');
      slot.warnings = d.warnings;
      const len = d.frames / d.sampleRate;
      slot.trimStart = 0;
      slot.trimEnd = len;
      slot.loopStart = 0;
      slot.loopEnd = len;
      this.engine.post({ type: 'sample', slot: index, channels: d.channels, settings: slotSettings(slot) });
      if (!wasActive) this.placeNewNode(slot);
      this.syncTimeline();
    } catch (e) {
      slot.error = (e as Error).message;
    } finally {
      slot.loading = false;
      this.emit('all');
    }
  }

  /**
   * Give a newly loaded slot a node when spacing is manual: half-way
   * between its neighbours (0 and 1 stand in for missing neighbours). If
   * there is no room, fall back to equal spacing.
   */
  private placeNewNode(slot: SlotState): void {
    const tl = this.state.timeline;
    if (tl.autoSpace) return;
    const act = activeSlots(this.state);
    const i = act.findIndex((a) => a.slot === slot);
    const prev = i > 0 ? tl.positions[act[i - 1].slot.id] ?? 0 : 0;
    const next = i < act.length - 1 ? tl.positions[act[i + 1].slot.id] ?? 1 : 1;
    if (next - prev < 0.01) tl.autoSpace = true;
    else tl.positions[slot.id] = i === 0 ? 0 : i === act.length - 1 && next === 1 && prev < 1 ? (prev + 1) / 2 : (prev + next) / 2;
  }

  clearSlot(index: number): void {
    const slot = this.state.slots[index];
    if (!slot) return;
    Object.assign(slot, { ...emptySlot(), id: slot.id });
    this.engine.post({ type: 'sample', slot: index, channels: null, settings: slotSettings(slot) });
    this.syncTimeline();
    this.emit('all');
  }

  addSlot(): void {
    if (this.state.slots.length >= MAX_SLOTS) return;
    this.state.slots.push(emptySlot());
    this.emit('all');
  }

  removeSlot(index: number): void {
    if (this.state.slots.length <= MIN_SLOTS) return;
    this.state.slots.splice(index, 1);
    // Slot indices shift, so resend every slot from the removed one onward.
    for (let i = index; i < MAX_SLOTS; i++) {
      const slot = this.state.slots[i];
      this.engine.post({ type: 'sample', slot: i, channels: slot?.audio?.channels ?? null, settings: slotSettings(slot ?? emptySlot()) });
    }
    this.syncTimeline();
    this.emit('all');
  }

  updateSlot(index: number, patch: Partial<Pick<SlotState, 'gainDb' | 'normalize' | 'reverse' | 'mute' | 'solo'>>): void {
    const slot = this.state.slots[index];
    if (!slot) return;
    Object.assign(slot, patch);
    this.syncSlot(index);
    this.emit('slots');
  }

  setMarkers(index: number, patch: Partial<Pick<SlotState, 'trimStart' | 'trimEnd' | 'loopStart' | 'loopEnd'>>, commit = true): void {
    const slot = this.state.slots[index];
    if (!slot?.audio) return;
    Object.assign(slot, patch);
    sanitizeMarkers(slot);
    if (commit) this.syncSlot(index);
    this.emit('slots');
  }

  // ---- timeline ----------------------------------------------------------------

  /** Move the node of the n-th active slot. */
  moveNode(activeIndex: number, pos: number): void {
    const act = activeSlots(this.state);
    const positions = moveNodePos(nodePositions(this.state), activeIndex, pos);
    const tl = this.state.timeline;
    tl.autoSpace = false;
    act.forEach((a, i) => (tl.positions[a.slot.id] = positions[i]));
    this.syncTimeline();
    this.emit('timeline');
  }

  equalSpacing(): void {
    this.state.timeline.autoSpace = true;
    this.syncTimeline();
    this.emit('timeline');
  }

  updateSegment(activeIndex: number, patch: Partial<Segment>): void {
    const act = activeSlots(this.state);
    const a = act[activeIndex]?.slot;
    const b = act[activeIndex + 1]?.slot;
    if (!a || !b) return;
    const seg = { ...getSegment(this.state, a, b), ...patch };
    seg.tStart = Math.max(0, Math.min(1, seg.tStart));
    seg.tEnd = Math.max(seg.tStart, Math.min(1, seg.tEnd));
    this.state.timeline.segments[`${a.id}>${b.id}`] = seg;
    this.syncTimeline();
    this.emit('timeline');
  }

  /** Apply a segment's curve settings to every segment. */
  applySegmentToAll(activeIndex: number): void {
    const act = activeSlots(this.state);
    const a = act[activeIndex]?.slot;
    const b = act[activeIndex + 1]?.slot;
    if (!a || !b) return;
    const src = getSegment(this.state, a, b);
    for (let i = 0; i + 1 < act.length; i++) {
      this.state.timeline.segments[`${act[i].slot.id}>${act[i + 1].slot.id}`] = { ...src };
    }
    this.state.timeline.defaultCurve = src.curve;
    this.syncTimeline();
    this.emit('timeline');
  }

  // ---- global settings -----------------------------------------------------------

  setDuration(seconds: number): void {
    this.state.durationSec = Math.min(7200, Math.max(5, seconds));
    this.engine.post({ type: 'duration', seconds: this.state.durationSec });
    this.emit('settings');
  }

  setPlayMode(mode: PlayMode): void {
    this.state.playMode = mode;
    this.engine.post({ type: 'playMode', mode });
    this.emit('settings');
  }

  setDirection(direction: 1 | -1): void {
    this.state.direction = direction;
    this.live.direction = direction;
    this.engine.post({ type: 'direction', direction });
    this.emit('transport');
  }

  setEngine(engine: EngineId): void {
    this.state.engine = engine;
    this.engine.post({ type: 'engine', engine });
    this.emit('settings');
  }

  setGranular(patch: Partial<GranularParams>): void {
    Object.assign(this.state.granular, patch);
    this.engine.post({ type: 'granular', params: patch });
    this.emit('settings');
  }

  setSpectral(patch: Partial<SpectralParams>): void {
    Object.assign(this.state.spectral, patch);
    this.engine.post({ type: 'spectral', params: patch });
    this.emit('settings');
  }

  setGainLaw(law: GainLaw): void {
    this.state.gainLaw = law;
    this.engine.post({ type: 'gainLaw', law });
    this.emit('settings');
  }

  setMasterDb(db: number): void {
    this.state.masterDb = db;
    this.engine.post({ type: 'master', gain: dbToGain(db) });
    this.emit('settings');
  }

  setLoopCrossfadeMs(ms: number): void {
    this.state.loopCrossfadeMs = ms;
    this.engine.post({ type: 'loopCrossfade', seconds: ms / 1000 });
    // Loop crossfade length is applied when slot regions are next set.
    this.state.slots.forEach((_, i) => this.state.slots[i].audio && this.syncSlot(i));
    this.emit('settings');
  }

  // ---- transport -------------------------------------------------------------------

  play(): void {
    void this.engine.resume();
    this.engine.post({ type: 'transport', command: 'play' });
    this.live.transport = 'playing';
    this.emit('transport');
  }

  pause(): void {
    this.engine.post({ type: 'transport', command: 'pause' });
    this.live.transport = 'paused';
    this.emit('transport');
  }

  togglePlay(): void {
    if (this.live.transport === 'playing') this.pause();
    else this.play();
  }

  stop(): void {
    this.engine.post({ type: 'transport', command: 'stop' });
    this.live.transport = 'stopped';
    this.live.frozen = false;
    this.engine.post({ type: 'hold', held: false });
    this.emit('transport');
  }

  restart(): void {
    void this.engine.resume();
    this.engine.post({ type: 'transport', command: 'restart' });
    this.live.transport = 'playing';
    this.emit('transport');
  }

  seek(position: number): void {
    this.live.position = Math.max(0, Math.min(1, position));
    this.engine.post({ type: 'seek', position: this.live.position });
  }

  toggleFreeze(): void {
    this.live.frozen = !this.live.frozen;
    this.engine.post({ type: 'hold', held: this.live.frozen });
    this.emit('transport');
  }

  get hasAudio(): boolean {
    return activeSlots(this.state).length > 0;
  }
}
