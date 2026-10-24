/**
 * Seamless looping sample player.
 *
 * Plays from the trim start, then loops [loopStart, loopEnd) forever. The
 * loop seam is hidden with an equal-power crossfade: over the last `xf`
 * frames before loopEnd the tail fades out while the first `xf` frames
 * after loopStart fade in, and the read head then continues from
 * loopStart + xf. No pre-roll before loopStart is needed, so loops may
 * start at frame 0.
 *
 * Reverse playback reads the buffer through a mirrored index, so no
 * reversed copy of the audio is kept.
 *
 * All processing is allocation-free.
 */

export interface LoopRegion {
  trimStart: number;
  loopStart: number;
  loopEnd: number;
}

export class LoopPlayer {
  private left: Float32Array = new Float32Array(0);
  private right: Float32Array = new Float32Array(0);
  private length = 0;
  private reverse = false;
  /** Virtual (direction-adjusted) region in frames. */
  private start = 0;
  private loopStart = 0;
  private loopEnd = 0;
  private xf = 0;
  private fadeOut: Float32Array = new Float32Array(0);
  private fadeIn: Float32Array = new Float32Array(0);
  private requestedXf = 0;
  /** Virtual read position in frames. */
  pos = 0;

  get loaded(): boolean {
    return this.length > 0;
  }

  get frameCount(): number {
    return this.length;
  }

  setBuffer(channels: Float32Array[]): void {
    this.left = channels[0] ?? new Float32Array(0);
    this.right = channels[1] ?? this.left;
    this.length = this.left.length;
    this.setRegion({ trimStart: 0, loopStart: 0, loopEnd: this.length }, this.reverse, this.requestedXf);
    this.pos = this.start;
  }

  clear(): void {
    this.left = this.right = new Float32Array(0);
    this.length = 0;
    this.pos = this.start = this.loopStart = this.loopEnd = 0;
  }

  /**
   * Set the region in physical frames, the reverse flag and the requested
   * loop crossfade length. Keeps the read head inside the new region.
   */
  setRegion(region: LoopRegion, reverse: boolean, crossfadeFrames: number): void {
    const len = this.length;
    let ls = clampInt(region.loopStart, 0, len);
    let le = clampInt(region.loopEnd, 0, len);
    if (le - ls < 2) {
      ls = 0;
      le = len;
    }
    let ts = clampInt(region.trimStart, 0, ls);
    this.reverse = reverse;
    if (reverse) {
      // Mirror: physical [ls, le) becomes virtual [len-le, len-ls); the
      // intro (trim start → loop start) becomes trimEnd → loop end, which
      // we approximate with the loop end itself.
      const vls = len - le;
      const vle = len - ls;
      ts = vls;
      ls = vls;
      le = vle;
    }
    this.start = ts;
    this.loopStart = ls;
    this.loopEnd = le;
    this.requestedXf = Math.max(0, Math.floor(crossfadeFrames));
    const xf = Math.min(this.requestedXf, Math.floor((le - ls) / 2));
    if (xf !== this.xf || this.fadeIn.length !== xf) {
      this.xf = xf;
      this.fadeIn = new Float32Array(xf);
      this.fadeOut = new Float32Array(xf);
      for (let i = 0; i < xf; i++) {
        const t = (i + 0.5) / xf;
        this.fadeIn[i] = Math.sin(t * Math.PI * 0.5);
        this.fadeOut[i] = Math.cos(t * Math.PI * 0.5);
      }
    }
    if (this.pos < this.start || this.pos >= this.loopEnd) this.pos = this.loopStart;
  }

  /** Return the read head to the start of the region. */
  rewind(): void {
    this.pos = this.start;
  }

  /** Advance the read head without producing audio. */
  skip(frames: number): void {
    if (!this.length) return;
    let p = this.pos + frames;
    const period = this.loopEnd - this.loopStart - this.xf;
    if (p >= this.loopEnd && period > 0) {
      p = this.loopStart + this.xf + ((p - this.loopEnd) % period);
    }
    this.pos = p;
  }

  /**
   * Add `frames` of audio into outL/outR from `offset`, with a gain ramp
   * from g0 to g1 across the block.
   */
  process(outL: Float32Array, outR: Float32Array, offset: number, frames: number, g0: number, g1: number): void {
    if (!this.length) return;
    const L = this.left;
    const R = this.right;
    const last = this.length - 1;
    const rev = this.reverse;
    const ls = this.loopStart;
    const le = this.loopEnd;
    const xf = this.xf;
    const fadeStart = le - xf;
    const fin = this.fadeIn;
    const fout = this.fadeOut;
    const dg = frames > 0 ? (g1 - g0) / frames : 0;
    let g = g0;
    let p = this.pos;
    for (let i = 0; i < frames; i++) {
      const ia = rev ? last - p : p;
      let l = L[ia];
      let r = R[ia];
      if (p >= fadeStart && xf > 0) {
        const k = p - fadeStart;
        const ib = rev ? last - (ls + k) : ls + k;
        const a = fout[k];
        const b = fin[k];
        l = l * a + L[ib] * b;
        r = r * a + R[ib] * b;
      }
      outL[offset + i] += l * g;
      outR[offset + i] += r * g;
      g += dg;
      p++;
      if (p >= le) p = ls + xf;
    }
    this.pos = p;
  }
}

function clampInt(x: number, lo: number, hi: number): number {
  const v = Math.round(x);
  return v < lo ? lo : v > hi ? hi : v;
}
