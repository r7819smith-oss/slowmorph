/**
 * Recorder tap: passes stereo audio through unchanged and, while armed,
 * posts copies of each block to the main thread in ~0.1 s batches.
 */

/// <reference path="./worklet-env.d.ts" />

class RecorderProcessor extends AudioWorkletProcessor {
  private recording = false;
  private bufL = new Float32Array(4096);
  private bufR = new Float32Array(4096);
  private fill = 0;

  constructor() {
    super();
    this.port.onmessage = (e: MessageEvent<{ type: 'start' | 'stop' }>) => {
      if (e.data.type === 'start') {
        this.recording = true;
        this.fill = 0;
      } else {
        this.flush();
        this.recording = false;
        this.port.postMessage({ type: 'stopped' });
      }
    };
  }

  private flush(): void {
    if (this.fill === 0) return;
    const l = this.bufL.slice(0, this.fill);
    const r = this.bufR.slice(0, this.fill);
    this.port.postMessage({ type: 'chunk', l, r }, [l.buffer, r.buffer]);
    this.fill = 0;
  }

  process(inputs: Float32Array[][], outputs: Float32Array[][]): boolean {
    const inp = inputs[0];
    const out = outputs[0];
    const l = inp[0];
    const r = inp[1] ?? inp[0];
    const frames = out[0].length;
    if (l) {
      out[0].set(l);
      out[1]?.set(r);
    }
    if (this.recording) {
      if (this.fill + frames > this.bufL.length) this.flush();
      if (l) {
        this.bufL.set(l, this.fill);
        this.bufR.set(r, this.fill);
      }
      this.fill += frames;
    }
    return true;
  }
}

registerProcessor('slowmorph-recorder', RecorderProcessor);
