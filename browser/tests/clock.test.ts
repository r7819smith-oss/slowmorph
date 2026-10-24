import { describe, expect, it } from 'vitest';
import { formatTime, MorphClock, parseDuration } from '../src/dsp/clock';

describe('morph clock', () => {
  it('advances by duration and stops at the end in once mode', () => {
    const c = new MorphClock();
    c.setDuration(10);
    c.advance(48000 * 5, 48000);
    expect(c.position).toBeCloseTo(0.5);
    const ev = c.advance(48000 * 6, 48000);
    expect(ev.ended).toBe(true);
    expect(c.position).toBe(1);
    c.advance(48000, 48000);
    expect(c.position).toBe(1);
  });

  it('loops and reports a wrap', () => {
    const c = new MorphClock();
    c.setDuration(10);
    c.mode = 'loop';
    c.seek(0.95);
    const ev = c.advance(48000, 48000);
    expect(ev.wrapped).toBe(true);
    expect(c.position).toBeCloseTo(0.05);
  });

  it('ping-pongs and flips direction', () => {
    const c = new MorphClock();
    c.setDuration(10);
    c.mode = 'pingpong';
    c.seek(0.95);
    c.advance(48000, 48000);
    expect(c.direction).toBe(-1);
    expect(c.position).toBeCloseTo(0.95);
  });

  it('runs in reverse', () => {
    const c = new MorphClock();
    c.setDuration(10);
    c.seek(0.5);
    c.direction = -1;
    c.advance(48000, 48000);
    expect(c.position).toBeCloseTo(0.4);
  });

  it('holds position when held', () => {
    const c = new MorphClock();
    c.seek(0.3);
    c.held = true;
    c.advance(48000, 48000);
    expect(c.position).toBe(0.3);
  });

  it('clamps duration to 5 s .. 120 min and stays precise over long morphs', () => {
    const c = new MorphClock();
    c.setDuration(1);
    expect(c.duration).toBe(5);
    c.setDuration(1e9);
    expect(c.duration).toBe(7200);
    // 60 minutes of 128-frame blocks lands exactly at the half-way point
    c.setDuration(7200);
    const blocks = (3600 * 48000) / 128;
    for (let i = 0; i < blocks; i++) c.advance(128, 48000);
    expect(c.position).toBeCloseTo(0.5, 9);
  });

  it('parses and formats durations', () => {
    expect(parseDuration('90')).toBe(90);
    expect(parseDuration('1:30')).toBe(90);
    expect(parseDuration('1:02:03')).toBe(3723);
    expect(parseDuration('10m')).toBe(600);
    expect(parseDuration('1.5h')).toBe(5400);
    expect(parseDuration('45 s')).toBe(45);
    expect(parseDuration('abc')).toBeNull();
    expect(formatTime(3723)).toBe('1:02:03');
    expect(formatTime(65)).toBe('1:05');
  });
});
