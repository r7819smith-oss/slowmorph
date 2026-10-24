/** WAV encoding (16-bit PCM or 32-bit float) for exports and tests. */

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
