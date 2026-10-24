/**
 * Output safety limiter.
 *
 * A transparent peak limiter that only engages above the ceiling
 * (-1 dBFS by default): instant attack, ~300 ms release, applied to both
 * channels together so the stereo image does not shift. It exists to stop
 * unexpected spikes reaching speakers; it is not a sound-shaping effect,
 * and the UI shows when it is reducing gain so it cannot hide problems.
 */

export class SafetyLimiter {
  ceiling = 0.891; // -1 dBFS
  private gain = 1;
  private release = 0;
  /** Lowest gain applied during the last process() call. */
  minGain = 1;

  constructor(sampleRate: number) {
    this.release = 1 - Math.exp(-1 / (0.3 * sampleRate));
  }

  process(L: Float32Array, R: Float32Array, frames: number): void {
    let g = this.gain;
    let minG = 1;
    const c = this.ceiling;
    const rel = this.release;
    for (let i = 0; i < frames; i++) {
      const peak = Math.max(Math.abs(L[i]), Math.abs(R[i]));
      const needed = peak > c ? c / peak : 1;
      g = Math.min(needed, g + (1 - g) * rel);
      L[i] *= g;
      R[i] *= g;
      if (g < minG) minG = g;
    }
    this.gain = g;
    this.minGain = minG;
  }
}
