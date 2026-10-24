/// <reference lib="webworker" />
/** Web Worker that runs renderMorph off the main thread and streams encoded chunks back. */

import { renderMorph } from './offline-render';
import type { ToWorklet } from './messages';
import { encodePcm, type WavFormat } from './wav';

interface StartMsg {
  type: 'start';
  messages: ToWorklet[];
  sampleRate: number;
  tailSeconds: number;
  format: WavFormat;
}

let cancel = false;
const scope = self as unknown as DedicatedWorkerGlobalScope;

scope.onmessage = async (e: MessageEvent<StartMsg | { type: 'cancel' }>) => {
  if (e.data.type === 'cancel') {
    cancel = true;
    return;
  }
  const { messages, sampleRate, tailSeconds, format } = e.data;
  cancel = false;
  let peak = 0;
  try {
    const ok = await renderMorph(
      messages,
      { sampleRate, tailSeconds },
      async (L, R, p) => {
        for (let i = 0; i < L.length; i++) peak = Math.max(peak, Math.abs(L[i]), Math.abs(R[i]));
        const bytes = encodePcm(L, R, format);
        scope.postMessage({ type: 'chunk', bytes, frames: L.length, done: p.framesDone, total: p.framesTotal }, [bytes.buffer]);
        // yield so a cancel message can arrive
        await new Promise((r) => setTimeout(r, 0));
      },
      () => cancel,
    );
    scope.postMessage({ type: ok ? 'done' : 'cancelled', peak });
  } catch (err) {
    scope.postMessage({ type: 'error', message: (err as Error).message });
  }
};
