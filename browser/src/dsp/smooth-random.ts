/**
 * Smooth random motion: a continuous random curve in −1..1. New random
 * targets are drawn `rate` times a second and joined with linear (rough) to
 * cosine (smooth) interpolation, then softened by a one-pole filter. Used
 * for morph drift and for the subtle movement of Character in crossfade
 * mode. Allocation-free and seedable.
 */
import { Random } from './random';

export class SmoothRandom {
  private readonly rng: Random;
  private a = 0;
  private b = 0;
  private phase = 0;
  private y = 0;
  value = 0;

  constructor(seed: number) {
    this.rng = new Random(seed);
    this.b = this.rng.bipolar();
  }

  /** Advance by `seconds`; returns the new value. */
  step(seconds: number, rate: number, smoothness: number): number {
    this.phase += seconds * Math.max(0.001, rate);
    while (this.phase >= 1) {
      this.phase -= 1;
      this.a = this.b;
      this.b = this.rng.bipolar();
    }
    const lin = this.phase;
    const cos = 0.5 - 0.5 * Math.cos(Math.PI * lin);
    const t = lin + (cos - lin) * smoothness;
    const target = this.a + (this.b - this.a) * t;
    const tau = (0.05 + smoothness * 0.45) / Math.max(0.001, rate);
    this.y += (target - this.y) * (1 - Math.exp(-seconds / tau));
    this.value = this.y;
    return this.y;
  }

  reset(): void {
    this.a = this.b = this.y = this.value = 0;
    this.phase = 0;
  }
}
