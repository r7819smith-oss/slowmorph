/**
 * File import: decode WAV/MP3/AAC/FLAC/OGG with the browser, AIFF with our
 * own parser, then bring everything to the audio context's sample rate.
 * decodeAudioData already resamples to the context rate; AIFF data is
 * resampled by rendering it through an OfflineAudioContext, which uses the
 * browser's high-quality resampler.
 */

import { AudioFileError, decodeAiff, isAiff, type PcmData } from './aiff';

export const MAX_SAMPLE_SECONDS = 60;
export const RECOMMENDED_MAX_SECONDS = 15;
const MAX_FILE_BYTES = 300 * 1024 * 1024;

export interface DecodedSample {
  name: string;
  channels: Float32Array[]; // 1 or 2 channels at the target rate
  sampleRate: number;
  originalSampleRate: number | null;
  frames: number;
  peak: number;
  warnings: string[];
}

export async function decodeFile(file: File, targetRate: number): Promise<DecodedSample> {
  if (file.size === 0) throw new AudioFileError(`"${file.name}" is empty.`);
  if (file.size > MAX_FILE_BYTES) throw new AudioFileError(`"${file.name}" is too large to load as a sample.`);
  const data = await file.arrayBuffer();
  return decodeArrayBuffer(data, file.name, targetRate);
}

export async function decodeArrayBuffer(data: ArrayBuffer, name: string, targetRate: number): Promise<DecodedSample> {
  const warnings: string[] = [];
  let channels: Float32Array[];
  let originalRate: number | null = null;

  if (isAiff(data)) {
    let pcm: PcmData;
    try {
      pcm = decodeAiff(data);
    } catch (e) {
      throw new AudioFileError(`"${name}": ${(e as Error).message}`);
    }
    originalRate = pcm.sampleRate;
    channels = pcm.sampleRate === targetRate ? pcm.channels : await resample(pcm, targetRate);
  } else {
    const ctx = new OfflineAudioContext(1, 1, targetRate);
    let buffer: AudioBuffer;
    try {
      buffer = await ctx.decodeAudioData(data.slice(0));
    } catch {
      throw new AudioFileError(
        `"${name}" could not be decoded. It may be corrupted, or in a format this browser cannot read (WAV, AIFF, MP3, AAC, FLAC and OGG are supported in Chrome and Edge).`,
      );
    }
    channels = [];
    for (let c = 0; c < buffer.numberOfChannels; c++) channels.push(buffer.getChannelData(c).slice());
  }

  if (channels.length === 0 || channels[0].length === 0) throw new AudioFileError(`"${name}" contains no audio.`);
  if (channels.length > 2) {
    warnings.push(`${channels.length} channels found; using the first two.`);
    channels = channels.slice(0, 2);
  }
  const maxFrames = Math.floor(MAX_SAMPLE_SECONDS * targetRate);
  if (channels[0].length > maxFrames) {
    warnings.push(`Longer than ${MAX_SAMPLE_SECONDS} s; only the first ${MAX_SAMPLE_SECONDS} s were loaded.`);
    channels = channels.map((c) => c.slice(0, maxFrames));
  } else if (channels[0].length > RECOMMENDED_MAX_SECONDS * targetRate) {
    warnings.push(`Longer than the recommended ${RECOMMENDED_MAX_SECONDS} s; fine, but uses more memory.`);
  }
  let peak = 0;
  let nonFinite = false;
  for (const ch of channels) {
    for (let i = 0; i < ch.length; i++) {
      const x = ch[i];
      if (!Number.isFinite(x)) { ch[i] = 0; nonFinite = true; continue; }
      const a = Math.abs(x);
      if (a > peak) peak = a;
    }
  }
  if (nonFinite) warnings.push('The file contained invalid sample values; they were replaced with silence.');
  if (peak === 0) warnings.push('The file is completely silent.');
  return { name, channels, sampleRate: targetRate, originalSampleRate: originalRate, frames: channels[0].length, peak, warnings };
}

export async function resample(pcm: PcmData, targetRate: number): Promise<Float32Array[]> {
  const nch = pcm.channels.length;
  const frames = pcm.channels[0].length;
  const outFrames = Math.max(1, Math.ceil((frames * targetRate) / pcm.sampleRate));
  const ctx = new OfflineAudioContext(nch, outFrames, targetRate);
  const src = ctx.createBuffer(nch, frames, pcm.sampleRate);
  pcm.channels.forEach((c, i) => src.copyToChannel(c as Float32Array<ArrayBuffer>, i));
  const node = ctx.createBufferSource();
  node.buffer = src;
  node.connect(ctx.destination);
  node.start();
  const out = await ctx.startRendering();
  const res: Float32Array[] = [];
  for (let c = 0; c < nch; c++) res.push(out.getChannelData(c).slice());
  return res;
}
