/**
 * Global sound shaping, kept separate from the morph engines:
 * high-pass → low-pass (with resonance) → stereo width → delay and reverb
 * sends. The whole section can be bypassed (with a 20 ms crossfade). Runs
 * in the worklet so recordings and offline renders include it.
 */

export interface EffectsParams {
  enabled: boolean;
  /** Low-pass cutoff, Hz (40–20000). */
  lowpassHz: number;
  /** High-pass cutoff, Hz (20–8000). */
  highpassHz: number;
  /** Filter resonance 0..1. */
  resonance: number;
  /** Stereo width: 0 mono, 1 unchanged, 2 extra wide. */
  width: number;
  reverbSend: number;
  /** Reverb size/decay 0..1. */
  reverbSize: number;
  delaySend: number;
  /** Delay time, seconds (0.02–2). */
  delayTime: number;
  delayFeedback: number;
}

export function defaultEffectsParams(): EffectsParams {
  return {
    enabled: true,
    lowpassHz: 20000,
    highpassHz: 20,
    resonance: 0.1,
    width: 1,
    reverbSend: 0.15,
    reverbSize: 0.7,
    delaySend: 0,
    delayTime: 0.45,
    delayFeedback: 0.4,
  };
}

/** Topology-preserving-transform state-variable filter (Cytomic), one channel. */
class Svf {
  private ic1 = 0;
  private ic2 = 0;
  g = 0;
  k = 1.414;
  a1 = 0;
  a2 = 0;
  a3 = 0;

  set(cutoff: number, q: number, sr: number): void {
    this.g = Math.tan((Math.PI * Math.min(cutoff, sr * 0.49)) / sr);
    this.k = 1 / q;
    this.a1 = 1 / (1 + this.g * (this.g + this.k));
    this.a2 = this.g * this.a1;
    this.a3 = this.g * this.a2;
  }

  /** Returns [low, high] via the out-params of the caller. */
  low(v0: number): number {
    const v3 = v0 - this.ic2;
    const v1 = this.a1 * this.ic1 + this.a2 * v3;
    const v2 = this.ic2 + this.a2 * this.ic1 + this.a3 * v3;
    this.ic1 = 2 * v1 - this.ic1;
    this.ic2 = 2 * v2 - this.ic2;
    return v2;
  }

  high(v0: number): number {
    const v3 = v0 - this.ic2;
    const v1 = this.a1 * this.ic1 + this.a2 * v3;
    const v2 = this.ic2 + this.a2 * this.ic1 + this.a3 * v3;
    this.ic1 = 2 * v1 - this.ic1;
    this.ic2 = 2 * v2 - this.ic2;
    return v0 - this.k * v1 - v2;
  }

  reset(): void {
    this.ic1 = this.ic2 = 0;
  }
}

class Comb {
  private buf: Float32Array;
  private i = 0;
  private store = 0;
  constructor(len: number) {
    this.buf = new Float32Array(len);
  }
  process(x: number, feedback: number, damp: number): number {
    const y = this.buf[this.i];
    this.store = y * (1 - damp) + this.store * damp;
    this.buf[this.i] = x + this.store * feedback;
    if (++this.i >= this.buf.length) this.i = 0;
    return y;
  }
  reset(): void {
    this.buf.fill(0);
    this.store = 0;
  }
}

class Allpass {
  private buf: Float32Array;
  private i = 0;
  constructor(len: number) {
    this.buf = new Float32Array(len);
  }
  process(x: number): number {
    const b = this.buf[this.i];
    const y = -x + b;
    this.buf[this.i] = x + b * 0.5;
    if (++this.i >= this.buf.length) this.i = 0;
    return y;
  }
  reset(): void {
    this.buf.fill(0);
  }
}

/** Freeverb-style stereo reverb (8 combs + 4 all-passes per channel). */
class Reverb {
  private readonly combsL: Comb[] = [];
  private readonly combsR: Comb[] = [];
  private readonly apL: Allpass[] = [];
  private readonly apR: Allpass[] = [];

  constructor(sr: number) {
    const scale = sr / 44100;
    const combs = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617];
    const aps = [556, 441, 341, 225];
    const spread = 23;
    for (const c of combs) {
      this.combsL.push(new Comb(Math.round(c * scale)));
      this.combsR.push(new Comb(Math.round((c + spread) * scale)));
    }
    for (const a of aps) {
      this.apL.push(new Allpass(Math.round(a * scale)));
      this.apR.push(new Allpass(Math.round((a + spread) * scale)));
    }
  }

  process(inL: number, inR: number, size: number, out: Float32Array): void {
    const fb = 0.7 + 0.28 * size;
    const damp = 0.2 + 0.2 * (1 - size);
    const x = (inL + inR) * 0.015;
    let l = 0;
    let r = 0;
    for (let i = 0; i < 8; i++) {
      l += this.combsL[i].process(x, fb, damp);
      r += this.combsR[i].process(x, fb, damp);
    }
    for (let i = 0; i < 4; i++) {
      l = this.apL[i].process(l);
      r = this.apR[i].process(r);
    }
    out[0] = l;
    out[1] = r;
  }

  reset(): void {
    for (let i = 0; i < 8; i++) {
      this.combsL[i].reset();
      this.combsR[i].reset();
    }
    for (let i = 0; i < 4; i++) {
      this.apL[i].reset();
      this.apR[i].reset();
    }
  }
}

export class Effects {
  /** Starts bypassed; the app enables it from the project settings. */
  params: EffectsParams = { ...defaultEffectsParams(), enabled: false };
  private readonly sr: number;
  private readonly lpL = new Svf();
  private readonly lpR = new Svf();
  private readonly hpL = new Svf();
  private readonly hpR = new Svf();
  private readonly reverb: Reverb;
  private readonly delayL: Float32Array;
  private readonly delayR: Float32Array;
  private dw = 0;
  private delaySamples = 0;
  private dampL = 0;
  private dampR = 0;
  private readonly rv = new Float32Array(2);
  // smoothed parameters
  private lp = 20000;
  private hp = 20;
  private res = 0.1;
  private width = 1;
  private rvSend = 0;
  private dlSend = 0;
  private wet = 1;

  constructor(sampleRate: number) {
    this.sr = sampleRate;
    this.reverb = new Reverb(sampleRate);
    const max = Math.ceil(2.1 * sampleRate);
    this.delayL = new Float32Array(max);
    this.delayR = new Float32Array(max);
    this.delaySamples = this.params.delayTime * sampleRate;
    this.syncSmoothed();
  }

  private syncSmoothed(): void {
    const p = this.params;
    this.lp = p.lowpassHz;
    this.hp = p.highpassHz;
    this.res = p.resonance;
    this.width = p.width;
    this.rvSend = p.reverbSend;
    this.dlSend = p.delaySend;
    this.wet = p.enabled ? 1 : 0;
  }

  setParams(p: Partial<EffectsParams>): void {
    this.params = { ...this.params, ...p };
  }

  reset(): void {
    this.lpL.reset(); this.lpR.reset(); this.hpL.reset(); this.hpR.reset();
    this.reverb.reset();
    this.delayL.fill(0);
    this.delayR.fill(0);
    this.dampL = this.dampR = 0;
  }

  /** Process in place. */
  process(L: Float32Array, R: Float32Array, frames: number): void {
    const p = this.params;
    const sr = this.sr;
    const a = 1 - Math.exp(-frames / (0.03 * sr));
    const wetTarget = p.enabled ? 1 : 0;
    if (this.wet === 0 && wetTarget === 0) return; // fully bypassed
    if (this.wet === 0) this.reset(); // coming back from bypass: no stale tails
    // Filter settings are smoothed every 16 samples (cutoffs in the log
    // domain) so sweeps are smooth; the rest are ramped per block.
    const lpTarget = Math.log(clamp(p.lowpassHz, 40, 20000));
    const hpTarget = Math.log(clamp(p.highpassHz, 20, 8000));
    const resTarget = clamp(p.resonance, 0, 1);
    const a16 = 1 - Math.exp(-16 / (0.03 * sr));
    const w0 = this.width;
    this.width += (clamp(p.width, 0, 2) - this.width) * a;
    const rs0 = this.rvSend;
    this.rvSend += (clamp(p.reverbSend, 0, 1) - this.rvSend) * a;
    const ds0 = this.dlSend;
    this.dlSend += (clamp(p.delaySend, 0, 1) - this.dlSend) * a;
    // Filters, delay and reverb always run while the section is enabled, so
    // switching any of them in or out never meets stale state.
    const targetDelay = clamp(p.delayTime, 0.02, 2) * sr;
    const fb = clamp(p.delayFeedback, 0, 0.92);
    const size = clamp(p.reverbSize, 0, 1);
    const n = this.delayL.length;
    const wetStep = 1 / (0.02 * sr);
    let wet = this.wet;
    for (let i = 0; i < frames; i++) {
      if ((i & 15) === 0) {
        this.lp = Math.exp(Math.log(this.lp) + (lpTarget - Math.log(this.lp)) * a16);
        this.hp = Math.exp(Math.log(this.hp) + (hpTarget - Math.log(this.hp)) * a16);
        this.res += (resTarget - this.res) * a16;
        const q = 0.707 + this.res * this.res * 11;
        this.lpL.set(this.lp, q, sr);
        this.lpR.set(this.lp, q, sr);
        this.hpL.set(this.hp, 0.707 + this.res * 2, sr);
        this.hpR.set(this.hp, 0.707 + this.res * 2, sr);
      }
      const t = i / frames;
      const dryL = L[i];
      const dryR = R[i];
      let l = dryL;
      let r = dryR;
      l = this.lpL.low(this.hpL.high(l));
      r = this.lpR.low(this.hpR.high(r));
      // width (mid/side)
      const wd = w0 + (this.width - w0) * t;
      const m = (l + r) * 0.5;
      const s = (l - r) * 0.5 * wd;
      l = m + s;
      r = m - s;
      // delay: glide the delay time (tape-like) so changes never click
      {
        this.delaySamples += (targetDelay - this.delaySamples) * 0.0005;
        let rp = this.dw - this.delaySamples;
        if (rp < 0) rp += n;
        const ri = Math.floor(rp);
        const rf = rp - ri;
        const ri2 = ri + 1 >= n ? 0 : ri + 1;
        const yl = this.delayL[ri] + (this.delayL[ri2] - this.delayL[ri]) * rf;
        const yr = this.delayR[ri] + (this.delayR[ri2] - this.delayR[ri]) * rf;
        this.dampL += (yl - this.dampL) * 0.35;
        this.dampR += (yr - this.dampR) * 0.35;
        const send = ds0 + (this.dlSend - ds0) * t;
        // ping-pong: each side feeds the other
        this.delayL[this.dw] = l * send + this.dampR * fb;
        this.delayR[this.dw] = r * send + this.dampL * fb;
        if (++this.dw >= n) this.dw = 0;
        l += yl;
        r += yr;
      }
      {
        const send = rs0 + (this.rvSend - rs0) * t;
        this.reverb.process(l * send, r * send, size, this.rv);
        l += this.rv[0];
        r += this.rv[1];
      }
      if (wet !== wetTarget) wet = wetTarget > wet ? Math.min(1, wet + wetStep) : Math.max(0, wet - wetStep);
      L[i] = dryL + (l - dryL) * wet;
      R[i] = dryR + (r - dryR) * wet;
    }
    this.wet = wet;
  }
}

function clamp(x: number, lo: number, hi: number): number {
  return x < lo ? lo : x > hi ? hi : x;
}
