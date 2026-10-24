/**
 * AudioWorklet processor: a thin wrapper that feeds messages to MorphCore
 * and renders its output. Status (morph position, mix fractions, meters)
 * is posted back about 30 times a second.
 */

/// <reference path="./worklet-env.d.ts" />
import { MorphCore } from '../dsp/core';
import type { ToWorklet, WorkletStatus } from './messages';

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
    const c = this.core;
    switch (msg.type) {
      case 'sample': c.setSample(msg.slot, msg.channels, msg.settings); break;
      case 'slotSettings': c.setSlotSettings(msg.slot, msg.settings); break;
      case 'timeline': c.setTimeline(msg.model); break;
      case 'gainLaw': c.setGainLaw(msg.law); break;
      case 'master': c.setMasterGain(msg.gain); break;
      case 'duration': c.setDuration(msg.seconds); break;
      case 'playMode': c.setPlayMode(msg.mode); break;
      case 'direction': c.setDirection(msg.direction); break;
      case 'hold': c.setHold(msg.held); break;
      case 'seek': c.seek(msg.position); break;
      case 'engine': c.setEngine(msg.engine); break;
      case 'granular': c.setGranularParams(msg.params); break;
      case 'spectral': c.setSpectralParams(msg.params); break;
      case 'loopCrossfade': c.loopCrossfadeSeconds = msg.seconds; break;
      case 'transport':
        if (msg.command === 'play') c.play();
        else if (msg.command === 'pause') c.pause();
        else if (msg.command === 'stop') c.stop();
        else c.restart();
        this.statusCountdown = 0;
        break;
    }
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
      };
      this.port.postMessage(status);
      this.peakL = this.peakR = 0;
      this.limiterGain = 1;
    }
    return true;
  }
}

registerProcessor('slowmorph', SlowMorphProcessor);
