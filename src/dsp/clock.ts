/**
 * Morph clock: tracks the normalised morph position over a duration.
 *
 * Runs inside the audio thread so the morph advances sample-accurately and
 * keeps going while the UI is busy or the tab is in the background.
 */

export type PlayMode = 'once' | 'loop' | 'pingpong';

export const MIN_DURATION = 5;
export const MAX_DURATION = 120 * 60;

export interface ClockEvent {
  /** The position jumped (loop wrap); callers may glide gains. */
  wrapped: boolean;
  /** The morph reached its end in 'once' mode on this call. */
  ended: boolean;
}

export class MorphClock {
  position = 0;
  duration = 600;
  direction: 1 | -1 = 1;
  mode: PlayMode = 'once';
  /** True when the clock is held (freeze or pause). */
  held = false;
  ended = false;
  private readonly event: ClockEvent = { wrapped: false, ended: false };

  setDuration(seconds: number): void {
    this.duration = Math.min(MAX_DURATION, Math.max(MIN_DURATION, seconds));
  }

  seek(position: number): void {
    this.position = Math.min(1, Math.max(0, position));
    this.ended = false;
  }

  /** Advance by `frames` at `sampleRate`. Returns a reused event object. */
  advance(frames: number, sampleRate: number): ClockEvent {
    const ev = this.event;
    ev.wrapped = false;
    ev.ended = false;
    if (this.held || this.ended) return ev;
    let p = this.position + (this.direction * frames) / (this.duration * sampleRate);
    if (p > 1 || p < 0) {
      switch (this.mode) {
        case 'once':
          p = p > 1 ? 1 : 0;
          this.ended = true;
          ev.ended = true;
          break;
        case 'loop':
          p = p > 1 ? p - 1 : p + 1;
          ev.wrapped = true;
          break;
        case 'pingpong':
          p = p > 1 ? 2 - p : -p;
          this.direction = this.direction === 1 ? -1 : 1;
          break;
      }
    }
    this.position = Math.min(1, Math.max(0, p));
    return ev;
  }

  elapsedSeconds(): number {
    return this.position * this.duration;
  }
}

/** Format seconds as h:mm:ss or m:ss. */
export function formatTime(seconds: number): string {
  const s = Math.max(0, Math.round(seconds));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  const ss = sec.toString().padStart(2, '0');
  return h > 0 ? `${h}:${m.toString().padStart(2, '0')}:${ss}` : `${m}:${ss}`;
}

/**
 * Parse a duration typed by the user: "90", "1:30", "1:02:03", "10m",
 * "1.5h", "45s". Returns seconds or null when unreadable.
 */
export function parseDuration(text: string): number | null {
  const t = text.trim().toLowerCase();
  if (!t) return null;
  const unit = /^(\d+(?:\.\d+)?)\s*(h|hr|hours?|m|min|minutes?|s|sec|seconds?)$/.exec(t);
  if (unit) {
    const v = parseFloat(unit[1]);
    const u = unit[2][0];
    return u === 'h' ? v * 3600 : u === 'm' ? v * 60 : v;
  }
  if (/^\d+(?:\.\d+)?$/.test(t)) return parseFloat(t);
  const parts = t.split(':');
  if (parts.length < 2 || parts.length > 3 || parts.some((p) => !/^\d+(?:\.\d+)?$/.test(p))) return null;
  return parts.reduce((acc, p) => acc * 60 + parseFloat(p), 0);
}
