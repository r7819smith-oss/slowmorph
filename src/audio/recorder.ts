/** Main-thread side of the recorder tap: collects chunks into a stereo take. */

export class Recorder {
  private chunksL: Float32Array[] = [];
  private chunksR: Float32Array[] = [];
  private stopResolve: (() => void) | null = null;
  recording = false;
  frames = 0;

  constructor(private readonly node: AudioWorkletNode, readonly sampleRate: number) {
    node.port.onmessage = (e: MessageEvent<{ type: string; l?: Float32Array; r?: Float32Array }>) => {
      if (e.data.type === 'chunk' && e.data.l && e.data.r) {
        this.chunksL.push(e.data.l);
        this.chunksR.push(e.data.r);
        this.frames += e.data.l.length;
      } else if (e.data.type === 'stopped') {
        this.stopResolve?.();
        this.stopResolve = null;
      }
    };
  }

  start(): void {
    this.chunksL = [];
    this.chunksR = [];
    this.frames = 0;
    this.recording = true;
    this.node.port.postMessage({ type: 'start' });
  }

  /** Stop and return the recorded stereo audio. */
  async stop(): Promise<[Float32Array, Float32Array]> {
    const done = new Promise<void>((r) => (this.stopResolve = r));
    this.node.port.postMessage({ type: 'stop' });
    await done;
    this.recording = false;
    return [concat(this.chunksL, this.frames), concat(this.chunksR, this.frames)];
  }
}

function concat(chunks: Float32Array[], frames: number): Float32Array {
  const out = new Float32Array(frames);
  let o = 0;
  for (const c of chunks) {
    out.set(c, o);
    o += c.length;
  }
  return out;
}
