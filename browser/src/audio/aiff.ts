/**
 * Minimal AIFF / AIFF-C decoder (PCM 8/16/24/32-bit, little-endian 'sowt',
 * and 32/64-bit float). Chrome's decodeAudioData does not read AIFF, so
 * SlowMorph parses it here and lets the browser resample the result.
 */

export interface PcmData {
  sampleRate: number;
  channels: Float32Array[];
}

export class AudioFileError extends Error {}

function readExtended(view: DataView, off: number): number {
  // IEEE 754 80-bit extended precision, big-endian.
  const expon = view.getUint16(off) & 0x7fff;
  const sign = view.getUint16(off) & 0x8000 ? -1 : 1;
  const hi = view.getUint32(off + 2);
  const lo = view.getUint32(off + 6);
  if (expon === 0 && hi === 0 && lo === 0) return 0;
  const mant = hi * 2 ** -31 + lo * 2 ** -63;
  return sign * mant * 2 ** (expon - 16383);
}

function fourcc(view: DataView, off: number): string {
  return String.fromCharCode(view.getUint8(off), view.getUint8(off + 1), view.getUint8(off + 2), view.getUint8(off + 3));
}

export function isAiff(buf: ArrayBuffer): boolean {
  if (buf.byteLength < 12) return false;
  const v = new DataView(buf);
  const form = fourcc(v, 8);
  return fourcc(v, 0) === 'FORM' && (form === 'AIFF' || form === 'AIFC');
}

export function decodeAiff(buf: ArrayBuffer): PcmData {
  if (!isAiff(buf)) throw new AudioFileError('Not an AIFF file.');
  const v = new DataView(buf);
  const isAifc = fourcc(v, 8) === 'AIFC';
  let off = 12;
  let numChannels = 0;
  let numFrames = 0;
  let bits = 0;
  let rate = 0;
  let compression = 'NONE';
  let dataOff = -1;
  let dataLen = 0;
  while (off + 8 <= buf.byteLength) {
    const id = fourcc(v, off);
    const size = v.getUint32(off + 4);
    const body = off + 8;
    if (id === 'COMM') {
      numChannels = v.getInt16(body);
      numFrames = v.getUint32(body + 2);
      bits = v.getInt16(body + 6);
      rate = readExtended(v, body + 8);
      if (isAifc && size >= 22) compression = fourcc(v, body + 18);
    } else if (id === 'SSND') {
      const ssndOffset = v.getUint32(body);
      dataOff = body + 8 + ssndOffset;
      dataLen = Math.min(size - 8 - ssndOffset, buf.byteLength - dataOff);
    }
    off = body + size + (size & 1);
  }
  if (!numChannels || !rate || dataOff < 0) throw new AudioFileError('AIFF file is missing its COMM or SSND chunk.');
  const ctype = compression.toLowerCase();
  let little = false;
  let float = false;
  if (ctype === 'sowt') little = true;
  else if (ctype === 'fl32') { float = true; bits = 32; }
  else if (ctype === 'fl64') { float = true; bits = 64; }
  else if (ctype !== 'none' && ctype !== 'twos') throw new AudioFileError(`AIFF-C compression "${compression}" is not supported.`);
  const bytes = Math.ceil(bits / 8);
  if (![1, 2, 3, 4, 8].includes(bytes)) throw new AudioFileError(`Unsupported AIFF bit depth ${bits}.`);
  const frameBytes = bytes * numChannels;
  const frames = Math.min(numFrames, Math.floor(dataLen / frameBytes));
  const channels: Float32Array[] = [];
  for (let c = 0; c < numChannels; c++) channels.push(new Float32Array(frames));
  for (let i = 0; i < frames; i++) {
    for (let c = 0; c < numChannels; c++) {
      const p = dataOff + i * frameBytes + c * bytes;
      let x: number;
      if (float) x = bytes === 4 ? v.getFloat32(p, little) : v.getFloat64(p, little);
      else if (bytes === 1) x = v.getInt8(p) / 128;
      else if (bytes === 2) x = v.getInt16(p, little) / 32768;
      else if (bytes === 3) {
        const b0 = v.getUint8(p), b1 = v.getUint8(p + 1), b2 = v.getUint8(p + 2);
        let n = little ? b0 | (b1 << 8) | (b2 << 16) : (b0 << 16) | (b1 << 8) | b2;
        if (n & 0x800000) n -= 0x1000000;
        x = n / 8388608;
      } else x = v.getInt32(p, little) / 2147483648;
      channels[c][i] = x;
    }
  }
  return { sampleRate: rate, channels };
}

/** Encode PCM as a big-endian 16-bit AIFF (used by tests). */
export function encodeAiff16(data: PcmData): ArrayBuffer {
  const nch = data.channels.length;
  const frames = data.channels[0].length;
  const ssndSize = 8 + frames * nch * 2;
  const total = 12 + 26 + 8 + ssndSize;
  const buf = new ArrayBuffer(total);
  const v = new DataView(buf);
  const w4 = (o: number, s: string) => { for (let i = 0; i < 4; i++) v.setUint8(o + i, s.charCodeAt(i)); };
  w4(0, 'FORM'); v.setUint32(4, total - 8); w4(8, 'AIFF');
  w4(12, 'COMM'); v.setUint32(16, 18);
  v.setInt16(20, nch); v.setUint32(22, frames); v.setInt16(26, 16);
  // 80-bit extended sample rate
  const r = data.sampleRate;
  const e = Math.floor(Math.log2(r));
  v.setUint16(28, 16383 + e);
  const mant = r / 2 ** e; // 1..2
  const hi = Math.floor(mant * 2 ** 31);
  v.setUint32(30, hi);
  v.setUint32(34, 0);
  w4(38, 'SSND'); v.setUint32(42, ssndSize); v.setUint32(46, 0); v.setUint32(50, 0);
  let p = 54;
  for (let i = 0; i < frames; i++) for (let c = 0; c < nch; c++) {
    const s = Math.max(-1, Math.min(1, data.channels[c][i]));
    v.setInt16(p, Math.round(s * 32767)); p += 2;
  }
  return buf;
}
