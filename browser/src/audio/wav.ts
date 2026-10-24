/**
 * WAV encoding for exports and tests: 16- or 24-bit PCM or 32-bit float.
 * `wavHeader` + `encodePcm` support streaming long recordings in chunks.
 */

export type WavFormat = 'pcm16' | 'pcm24' | 'float32';

export function bytesPerSample(format: WavFormat): number {
  return format === 'pcm16' ? 2 : format === 'pcm24' ? 3 : 4;
}

/** 44-byte RIFF/WAVE header for `frames` frames of interleaved audio. */
export function wavHeader(frames: number, channels: number, sampleRate: number, format: WavFormat): Uint8Array {
  const bytes = bytesPerSample(format);
  const dataSize = frames * channels * bytes;
  const h = new Uint8Array(44);
  const v = new DataView(h.buffer);
  const w4 = (o: number, s: string) => { for (let i = 0; i < 4; i++) h[o + i] = s.charCodeAt(i); };
  w4(0, 'RIFF'); v.setUint32(4, Math.min(0xffffffff, 36 + dataSize), true); w4(8, 'WAVE');
  w4(12, 'fmt '); v.setUint32(16, 16, true);
  v.setUint16(20, format === 'float32' ? 3 : 1, true);
  v.setUint16(22, channels, true);
  v.setUint32(24, sampleRate, true);
  v.setUint32(28, sampleRate * channels * bytes, true);
  v.setUint16(32, channels * bytes, true);
  v.setUint16(34, bytes * 8, true);
  w4(36, 'data'); v.setUint32(40, Math.min(0xffffffff, dataSize), true);
  return h;
}

/** Interleave and encode a stereo chunk (no header). */
export function encodePcm(L: Float32Array, R: Float32Array, format: WavFormat): Uint8Array {
  const n = L.length;
  const bytes = bytesPerSample(format);
  const out = new Uint8Array(n * 2 * bytes);
  const v = new DataView(out.buffer);
  let p = 0;
  for (let i = 0; i < n; i++) {
    for (let c = 0; c < 2; c++) {
      const s = c === 0 ? L[i] : R[i];
      if (format === 'float32') v.setFloat32(p, s, true);
      else {
        const x = Math.max(-1, Math.min(1, s));
        if (format === 'pcm16') v.setInt16(p, Math.round(x < 0 ? x * 32768 : x * 32767), true);
        else {
          const q = Math.round(x < 0 ? x * 8388608 : x * 8388607);
          out[p] = q & 0xff;
          out[p + 1] = (q >> 8) & 0xff;
          out[p + 2] = (q >> 16) & 0xff;
        }
      }
      p += bytes;
    }
  }
  return out;
}

/** Parse the essentials of a WAV header (used by tests). */
export function readWavInfo(buf: ArrayBuffer): { format: number; channels: number; sampleRate: number; bits: number; frames: number } {
  const v = new DataView(buf);
  const channels = v.getUint16(22, true);
  const bits = v.getUint16(34, true);
  return { format: v.getUint16(20, true), channels, sampleRate: v.getUint32(24, true), bits, frames: v.getUint32(40, true) / (channels * (bits / 8)) };
}

export function encodeWav(channels: Float32Array[], sampleRate: number, format: 'pcm16' | 'float32' = 'pcm16'): ArrayBuffer {
  const nch = channels.length;
  const frames = channels[0]?.length ?? 0;
  const bytes = format === 'pcm16' ? 2 : 4;
  const dataSize = frames * nch * bytes;
  const buf = new ArrayBuffer(44 + dataSize);
  const v = new DataView(buf);
  const w4 = (o: number, s: string) => { for (let i = 0; i < 4; i++) v.setUint8(o + i, s.charCodeAt(i)); };
  w4(0, 'RIFF'); v.setUint32(4, 36 + dataSize, true); w4(8, 'WAVE');
  w4(12, 'fmt '); v.setUint32(16, 16, true);
  v.setUint16(20, format === 'pcm16' ? 1 : 3, true);
  v.setUint16(22, nch, true);
  v.setUint32(24, sampleRate, true);
  v.setUint32(28, sampleRate * nch * bytes, true);
  v.setUint16(32, nch * bytes, true);
  v.setUint16(34, bytes * 8, true);
  w4(36, 'data'); v.setUint32(40, dataSize, true);
  let p = 44;
  for (let i = 0; i < frames; i++) {
    for (let c = 0; c < nch; c++) {
      const s = channels[c][i];
      if (format === 'pcm16') {
        const x = Math.max(-1, Math.min(1, s));
        v.setInt16(p, Math.round(x < 0 ? x * 32768 : x * 32767), true);
      } else v.setFloat32(p, s, true);
      p += bytes;
    }
  }
  return buf;
}
