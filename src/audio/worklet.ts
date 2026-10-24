/**
 * AudioWorklet processor: a thin wrapper that feeds messages to MorphCore
 * and renders its output. Status (morph position, mix fractions, meters)
 * is posted back about 30 times a second.
 */

/// <reference path="./worklet-env.d.ts" />
import { MorphCore } from '../dsp/core';
import type { ToWorklet, WorkletStatus } from './messages';
import { applyMessage } from './apply-message';

class SlowMorphProcessor extends AudioWorkletProcessor {
  private readonly core = new MorphCore(sampleRate);
  private statusCountdown = 0;
  private readonly statusEvery = Math.round(sampleRate / 30);
  private peakL = 0;
  private peakR = 0;
  private limiterGain = 1;
  private scratchL = new Float32Array(128);
  private scratchR = new Float32Array(128);

  constructor() {
    super();
    this.port.onmessage = (e: MessageEvent<ToWorklet>) => this.handle(e.data);
  }

  private handle(msg: ToWorklet): void {
    applyMessage(this.core, msg);
    if (msg.type === 'transport') this.statusCountdown = 0;
  }

  process(_inputs: Float32Array[][], outputs: Float32Array[][]): boolean {
    const out = outputs[0];
    const frames = out[0]?.length ?? 128;
    if (this.scratchL.length !== frames) {
      this.scratchL = new Float32Array(frames);
      this.scratchR = new Float32Array(frames);
    }
    this.core.process(this.scratchL, this.scratchR, frames);
    out[0]?.set(this.scratchL);
    if (out[1]) out[1].set(this.scratchR);

    this.peakL = Math.max(this.peakL, this.core.peakL);
    this.peakR = Math.max(this.peakR, this.core.peakR);
    this.limiterGain = Math.min(this.limiterGain, this.core.limiterGain);
    this.statusCountdown -= frames;
    if (this.statusCountdown <= 0) {
      this.statusCountdown = this.statusEvery;
      const c = this.core;
      const status: WorkletStatus = {
        type: 'status',
        position: c.clock.position,
        direction: c.clock.direction,
        transport: c.transport,
        ended: c.clock.ended,
        fractions: Array.from(c.fractions),
        peakL: this.peakL,
        peakR: this.peakR,
        limiterGain: this.limiterGain,
        grains: c.granular.activeCount,
        effectivePosition: c.effectivePosition,
      };
      this.port.postMessage(status);
      this.peakL = this.peakR = 0;
      this.limiterGain = 1;
    }
    return true;
  }
}

registerProcessor('slowmorph', SlowMorphProcessor);
