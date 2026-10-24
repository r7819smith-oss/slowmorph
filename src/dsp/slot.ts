/**
 * Per-slot runtime state on the audio thread: the loop player plus the
 * slot's gain, mute/solo state and a declicker used whenever the audio or
 * loop region is swapped while it may be audible.
 */

import { LoopPlayer, type LoopRegion } from './loop-player';

export interface SlotSettings {
  /** Linear gain including normalisation. */
  gain: number;
  mute: boolean;
  solo: boolean;
  reverse: boolean;
  /** Region in frames of the slot's buffer. */
  region: LoopRegion;
}

export class SlotRuntime {
  readonly player = new LoopPlayer();
  settings: SlotSettings = {
    gain: 1,
    mute: false,
    solo: false,
    reverse: false,
    region: { trimStart: 0, loopStart: 0, loopEnd: 0 },
  };
  /** Raw channels for engines that read the audio directly (granular, spectral). */
  channels: Float32Array[] = [];
  /** Smoothed user gain (gain × mute/solo). */
  smoothGain = 0;
  /** Declick gain: 0..1, falls to zero before a pending swap is applied. */
  declick = 1;
  /** Gain actually applied at the start/end of the current block (set by the core). */
  blockGainStart = 0;
  blockGainEnd = 0;
  private pendingBuffer: Float32Array[] | null | undefined = undefined;
  private pendingSettings: SlotSettings | null = null;

  get loaded(): boolean {
    return this.player.loaded;
  }

  get hasPending(): boolean {
    return this.pendingBuffer !== undefined || this.pendingSettings !== null;
  }

  /** Queue new audio (or null to clear) to be swapped in at zero gain. */
  queueBuffer(channels: Float32Array[] | null, settings: SlotSettings): void {
    this.pendingBuffer = channels;
    this.pendingSettings = settings;
  }

  /** Queue new settings; region/reverse changes are applied at zero gain. */
  queueSettings(settings: SlotSettings, crossfadeFrames: number): void {
    const cur = this.settings;
    const structural =
      settings.reverse !== cur.reverse ||
      settings.region.trimStart !== cur.region.trimStart ||
      settings.region.loopStart !== cur.region.loopStart ||
      settings.region.loopEnd !== cur.region.loopEnd;
    if (structural && this.loaded) {
      this.pendingSettings = settings;
    } else {
      this.settings = settings;
      if (structural) this.player.setRegion(settings.region, settings.reverse, crossfadeFrames);
    }
  }

  /** Apply whatever is pending. Call only when the slot is silent. */
  applyPending(crossfadeFrames: number): void {
    if (this.pendingBuffer !== undefined) {
      if (this.pendingBuffer === null) {
        this.player.clear();
        this.channels = [];
      } else {
        this.channels = this.pendingBuffer;
        this.player.setBuffer(this.pendingBuffer);
      }
      this.pendingBuffer = undefined;
    }
    if (this.pendingSettings) {
      this.settings = this.pendingSettings;
      this.pendingSettings = null;
      if (this.loaded) {
        this.player.setRegion(this.settings.region, this.settings.reverse, crossfadeFrames);
        this.player.rewind();
      }
    }
  }
}
