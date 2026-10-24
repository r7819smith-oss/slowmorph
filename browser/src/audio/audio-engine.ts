/**
 * Main-thread side of the audio graph. Owns the AudioContext and the
 * SlowMorph AudioWorkletNode, forwards control messages, and reports
 * worklet status and context state changes (including suspension by the
 * browser or an audio-device change).
 */

import workletUrl from './worklet.ts?worker&url';
import recorderUrl from './recorder-worklet.ts?worker&url';
import { Recorder } from './recorder';
import type { FromWorklet, ToWorklet, WorkletStatus } from './messages';

export type ContextState = AudioContextState | 'interrupted' | 'uninitialised' | 'failed';

export class AudioEngine {
  readonly ctx: AudioContext;
  private node: AudioWorkletNode | null = null;
  private ready: Promise<void>;
  private queue: { msg: ToWorklet; transfer?: Transferable[] }[] = [];
  onStatus: (s: WorkletStatus) => void = () => {};
  onStateChange: (s: ContextState, detail?: string) => void = () => {};
  /** Post-engine bus; effects will be inserted here. */
  output: GainNode;
  /** Records whatever reaches the speakers. Available once ready. */
  recorder: Recorder | null = null;

  constructor() {
    this.ctx = new AudioContext({ latencyHint: 'playback' });
    this.output = this.ctx.createGain();
    this.ctx.onstatechange = () => this.onStateChange(this.ctx.state as ContextState);
    this.ready = this.init();
  }

  get sampleRate(): number {
    return this.ctx.sampleRate;
  }

  private async init(): Promise<void> {
    try {
      await this.ctx.audioWorklet.addModule(workletUrl);
      await this.ctx.audioWorklet.addModule(recorderUrl);
      const tap = new AudioWorkletNode(this.ctx, 'slowmorph-recorder', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
      tap.channelCount = 2;
      tap.channelCountMode = 'explicit';
      this.output.connect(tap);
      tap.connect(this.ctx.destination);
      this.recorder = new Recorder(tap, this.ctx.sampleRate);
      const node = new AudioWorkletNode(this.ctx, 'slowmorph', {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [2],
      });
      node.port.onmessage = (e: MessageEvent<FromWorklet>) => {
        if (e.data.type === 'status') this.onStatus(e.data);
      };
      node.onprocessorerror = () => this.onStateChange('failed', 'The audio processor stopped with an error. Reload the page to recover.');
      node.connect(this.output);
      this.node = node;
      for (const q of this.queue) node.port.postMessage(q.msg, q.transfer ?? []);
      this.queue = [];
    } catch (e) {
      this.onStateChange('failed', `Could not start the audio engine: ${(e as Error).message}`);
      throw e;
    }
  }

  whenReady(): Promise<void> {
    return this.ready;
  }

  post(msg: ToWorklet, transfer?: Transferable[]): void {
    if (this.node) this.node.port.postMessage(msg, transfer ?? []);
    else this.queue.push({ msg, transfer });
  }

  /** Resume a suspended context; must be called from a user gesture. */
  async resume(): Promise<void> {
    if (this.ctx.state !== 'running') {
      try {
        await this.ctx.resume();
      } catch {
        /* reported through onstatechange */
      }
    }
  }
}
