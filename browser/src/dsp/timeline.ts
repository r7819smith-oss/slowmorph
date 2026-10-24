/**
 * Morph timeline.
 *
 * The timeline is a normalised axis 0..1 (0 = start of the morph,
 * 1 = end). Each loaded sample owns one node on it. Nodes are kept in slot
 * order and may not cross each other. Between two adjacent nodes lies a
 * *segment*; inside each segment a transition window [tStart, tEnd]
 * (fractions of the segment) says where the change actually happens. Before
 * the window the earlier sample plays alone, after it the later one does.
 *
 * `computeFractions` returns the mix fraction of every slot for a morph
 * position. The fractions always sum to 1 (or 0 with no nodes), and are
 * continuous in the position, which is what keeps transitions click-free.
 */

import { applyCurve, clamp01, type CurveType } from './curves';

export interface Segment {
  curve: CurveType;
  shape: number;
  /** Start of the transition window, as a fraction of the segment (0..1). */
  tStart: number;
  /** End of the transition window, as a fraction of the segment (0..1). */
  tEnd: number;
}

export interface TimelineModel {
  /** Slot index of each node, in timeline order. */
  order: number[];
  /** Normalised position of each node, non-decreasing. */
  positions: number[];
  /** One segment per adjacent node pair (order.length - 1). */
  segments: Segment[];
}

export const MIN_NODE_GAP = 0.002;

export function defaultSegment(): Segment {
  return { curve: 'linear', shape: 0.5, tStart: 0, tEnd: 1 };
}

export function equalSpacing(count: number): number[] {
  if (count <= 0) return [];
  if (count === 1) return [0];
  const out: number[] = [];
  for (let i = 0; i < count; i++) out.push(i / (count - 1));
  return out;
}

/**
 * Move node `index` to `pos`, clamped so it stays between its neighbours.
 * Returns a new positions array.
 */
export function moveNode(positions: readonly number[], index: number, pos: number): number[] {
  const out = positions.slice();
  const lo = index > 0 ? out[index - 1] + MIN_NODE_GAP : 0;
  const hi = index < out.length - 1 ? out[index + 1] - MIN_NODE_GAP : 1;
  out[index] = Math.min(Math.max(pos, lo), Math.max(lo, hi));
  return out;
}

/** Local transition progress (0..1, already curved) within a segment. */
export function segmentProgress(seg: Segment, local: number): number {
  const a = clamp01(Math.min(seg.tStart, seg.tEnd));
  const b = clamp01(Math.max(seg.tStart, seg.tEnd));
  let t: number;
  if (b - a < 1e-6) t = local < a ? 0 : 1;
  else t = clamp01((local - a) / (b - a));
  return applyCurve(t, seg.curve, seg.shape);
}

/**
 * Fill `out` (indexed by slot) with the mix fraction of each slot at morph
 * position `p`. Slots not on the timeline get 0. Allocation-free.
 */
export function computeFractions(model: TimelineModel, p: number, out: Float32Array | number[]): void {
  for (let i = 0; i < out.length; i++) out[i] = 0;
  const n = model.order.length;
  if (n === 0) return;
  const pos = model.positions;
  if (n === 1 || p <= pos[0]) {
    out[model.order[0]] = 1;
    return;
  }
  if (p >= pos[n - 1]) {
    out[model.order[n - 1]] = 1;
    return;
  }
  let i = 0;
  while (i < n - 2 && p >= pos[i + 1]) i++;
  const span = pos[i + 1] - pos[i];
  const local = span > 0 ? (p - pos[i]) / span : 1;
  const seg = model.segments[i] ?? defaultSegment();
  const t = segmentProgress(seg, local);
  out[model.order[i]] += 1 - t;
  out[model.order[i + 1]] += t;
}
