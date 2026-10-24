/**
 * The morph timeline: a canvas showing each sample's contribution over the
 * whole morph (stacked areas), the sample nodes, the transition windows
 * between them and the playhead.
 *
 * Interaction:
 *  - drag a node handle (top lane) to move a sample's node
 *  - drag a transition window edge (bottom lane) to change where a
 *    transition starts or ends; click a window to select it for editing
 *  - click or drag anywhere else to scrub the morph position
 *
 * The static layers are cached in an offscreen canvas and only redrawn when
 * the timeline changes; each animation frame only composites the cache and
 * draws the playhead.
 */

import type { App } from '../app';
import { computeFractions, segmentProgress } from '../dsp/timeline';
import { formatTime } from '../dsp/clock';
import { activeSlots, slotLetter, timelineModel } from '../state/project';
import { fitCanvas, h, slotColor } from './dom';

const PAD = 18;
const RULER_H = 20;
const NODE_LANE = 26;
const LANE_H = 26;

type Drag =
  | { kind: 'node'; index: number }
  | { kind: 'window'; segment: number; edge: 'start' | 'end' }
  | { kind: 'scrub' }
  | null;

export class TimelineView {
  readonly root: HTMLElement;
  private readonly canvas: HTMLCanvasElement;
  private readonly cache = document.createElement('canvas');
  private dirty = true;
  private drag: Drag = null;
  private readonly legend: HTMLElement;
  selectedSegment = 0;
  onSelectSegment: (index: number) => void = () => {};
  private readonly frac = new Float32Array(8);

  constructor(private readonly app: App) {
    this.canvas = h('canvas', { class: 'timeline-canvas', 'aria-label': 'Morph timeline', role: 'slider', tabindex: '0' });
    this.legend = h('div', { class: 'legend' });
    this.root = h('div', { class: 'timeline' }, this.canvas, this.legend);
    this.canvas.addEventListener('pointerdown', (e) => this.pointerDown(e));
    this.canvas.addEventListener('pointermove', (e) => this.pointerMove(e));
    this.canvas.addEventListener('pointerup', (e) => this.pointerUp(e));
    this.canvas.addEventListener('pointercancel', (e) => this.pointerUp(e));
    this.canvas.addEventListener('keydown', (e) => this.keyDown(e));
    new ResizeObserver(() => (this.dirty = true)).observe(this.canvas);
    app.on((k) => {
      if (k !== 'transport') this.dirty = true;
    });
  }

  invalidate(): void {
    this.dirty = true;
  }

  private x(p: number, w: number): number {
    return PAD + p * (w - 2 * PAD);
  }

  private p(x: number, w: number): number {
    return Math.max(0, Math.min(1, (x - PAD) / (w - 2 * PAD)));
  }

  private geometry() {
    const r = this.canvas.getBoundingClientRect();
    const w = r.width;
    const hgt = r.height;
    const areaTop = RULER_H + NODE_LANE;
    const laneTop = hgt - LANE_H - 4;
    const areaBottom = laneTop - 8;
    return { w, h: hgt, areaTop, areaBottom, laneTop };
  }

  /** Redraw the cached static layers. */
  private drawStatic(): void {
    const { ctx, w, h: hgt } = fitCanvas(this.canvas);
    void ctx;
    const dpr = window.devicePixelRatio || 1;
    this.cache.width = Math.round(w * dpr);
    this.cache.height = Math.round(hgt * dpr);
    const c = this.cache.getContext('2d')!;
    c.setTransform(dpr, 0, 0, dpr, 0, 0);
    const { areaTop, areaBottom, laneTop } = this.geometry();
    const state = this.app.state;
    const model = timelineModel(state);
    const act = activeSlots(state);

    c.fillStyle = '#121519';
    c.fillRect(0, 0, w, hgt);

    // ruler
    c.fillStyle = '#8a93a0';
    c.strokeStyle = '#2a3038';
    c.font = '11px ui-monospace, SFMono-Regular, Menlo, monospace';
    c.textBaseline = 'middle';
    const dur = state.durationSec;
    const steps = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600];
    const minPx = 70;
    const pxPerSec = (w - 2 * PAD) / dur;
    const step = steps.find((s) => s * pxPerSec >= minPx) ?? 3600;
    for (let t = 0; t <= dur + 1e-6; t += step) {
      const x = this.x(t / dur, w);
      c.beginPath();
      c.moveTo(x + 0.5, RULER_H - 6);
      c.lineTo(x + 0.5, areaBottom);
      c.stroke();
      c.textAlign = t === 0 ? 'left' : 'center';
      if (x < w - 30) c.fillText(formatTime(t), x, RULER_H / 2);
    }
    c.textAlign = 'right';
    c.fillText(formatTime(dur), w - 4, RULER_H / 2);

    // stacked contributions
    const areaH = areaBottom - areaTop;
    if (act.length === 0) {
      c.fillStyle = '#5c6470';
      c.textAlign = 'center';
      c.font = '14px system-ui, sans-serif';
      c.fillText('Load samples into the slots below to build a morph.', w / 2, (areaTop + areaBottom) / 2);
    } else {
      const cols = Math.max(2, Math.floor((w - 2 * PAD) / 2));
      const stacks: Float32Array[] = act.map(() => new Float32Array(cols + 1));
      for (let i = 0; i <= cols; i++) {
        computeFractions(model, i / cols, this.frac);
        let acc = 0;
        act.forEach((a, k) => {
          acc += this.frac[a.index];
          stacks[k][i] = acc;
        });
      }
      act.forEach((a, k) => {
        c.beginPath();
        for (let i = 0; i <= cols; i++) {
          const x = this.x(i / cols, w);
          c.lineTo(x, areaBottom - stacks[k][i] * areaH);
        }
        for (let i = cols; i >= 0; i--) {
          const x = this.x(i / cols, w);
          c.lineTo(x, areaBottom - (k > 0 ? stacks[k - 1][i] : 0) * areaH);
        }
        c.closePath();
        c.fillStyle = slotColor(a.index) + '55';
        c.fill();
        c.strokeStyle = slotColor(a.index) + 'cc';
        c.lineWidth = 1;
        c.stroke();
      });
    }

    // nodes
    model.positions.forEach((p, i) => {
      const idx = model.order[i];
      const x = this.x(p, w);
      c.strokeStyle = slotColor(idx);
      c.setLineDash([3, 3]);
      c.beginPath();
      c.moveTo(x + 0.5, areaTop);
      c.lineTo(x + 0.5, laneTop + LANE_H);
      c.stroke();
      c.setLineDash([]);
      c.fillStyle = slotColor(idx);
      c.beginPath();
      c.arc(x, RULER_H + NODE_LANE / 2, 10, 0, Math.PI * 2);
      c.fill();
      c.fillStyle = '#0d0f12';
      c.font = 'bold 12px system-ui, sans-serif';
      c.textAlign = 'center';
      c.fillText(slotLetter(idx), x, RULER_H + NODE_LANE / 2 + 0.5);
    });

    // transition windows
    for (let i = 0; i + 1 < model.positions.length; i++) {
      const seg = model.segments[i];
      const x0 = this.x(model.positions[i], w);
      const x1 = this.x(model.positions[i + 1], w);
      const ws = x0 + (x1 - x0) * seg.tStart;
      const we = x0 + (x1 - x0) * seg.tEnd;
      c.fillStyle = '#1b2026';
      c.fillRect(x0, laneTop, x1 - x0, LANE_H);
      // curve preview inside the window
      const ca = slotColor(model.order[i]);
      const cb = slotColor(model.order[i + 1]);
      const grad = c.createLinearGradient(ws, 0, Math.max(ws + 1, we), 0);
      grad.addColorStop(0, ca);
      grad.addColorStop(1, cb);
      c.fillStyle = grad;
      c.globalAlpha = i === this.selectedSegment ? 0.9 : 0.55;
      c.fillRect(ws, laneTop + 3, Math.max(2, we - ws), LANE_H - 6);
      c.globalAlpha = 1;
      c.strokeStyle = '#0d0f12';
      c.lineWidth = 1.5;
      c.beginPath();
      const n = 40;
      for (let k = 0; k <= n; k++) {
        const local = seg.tStart + ((seg.tEnd - seg.tStart) * k) / n;
        const t = segmentProgress(seg, local);
        const x = x0 + (x1 - x0) * local;
        const y = laneTop + LANE_H - 4 - t * (LANE_H - 8);
        if (k === 0) c.moveTo(x, y);
        else c.lineTo(x, y);
      }
      c.stroke();
      if (i === this.selectedSegment) {
        c.strokeStyle = '#e6e9ee';
        c.lineWidth = 1;
        c.strokeRect(x0 + 0.5, laneTop + 0.5, x1 - x0 - 1, LANE_H - 1);
      }
      c.fillStyle = '#e6e9ee';
      c.fillRect(ws - 2, laneTop, 4, LANE_H);
      c.fillRect(we - 2, laneTop, 4, LANE_H);
    }
    this.dirty = false;
  }

  /** Draw one frame: cached layers + playhead. Called every animation frame. */
  frame(): void {
    if (this.dirty) this.drawStatic();
    const { ctx, w, h: hgt } = fitCanvas(this.canvas);
    ctx.clearRect(0, 0, w, hgt);
    ctx.drawImage(this.cache, 0, 0, w, hgt);
    const x = this.x(this.app.live.position, w);
    const frozen = this.app.live.frozen;
    if (this.app.state.drift.enabled) {
      // where the drift has actually taken the sound
      const xd = this.x(this.app.live.effectivePosition, w);
      ctx.strokeStyle = 'rgba(255,255,255,0.55)';
      ctx.setLineDash([4, 4]);
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(xd, 10);
      ctx.lineTo(xd, hgt);
      ctx.stroke();
      ctx.setLineDash([]);
    }
    ctx.strokeStyle = frozen ? '#7dd3fc' : '#ffffff';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, hgt);
    ctx.stroke();
    ctx.fillStyle = ctx.strokeStyle;
    ctx.beginPath();
    ctx.moveTo(x - 6, 0);
    ctx.lineTo(x + 6, 0);
    ctx.lineTo(x, 8);
    ctx.fill();
    this.updateLegend();
  }

  private legendKey = '';
  private updateLegend(): void {
    const act = activeSlots(this.app.state);
    const fr = this.app.live.fractions;
    const key = act.map((a) => `${a.index}:${a.slot.name}:${Math.round((fr[a.index] ?? 0) * 100)}`).join('|');
    if (key === this.legendKey) return;
    this.legendKey = key;
    this.legend.replaceChildren(
      ...act.map((a) => {
        const pct = Math.round((fr[a.index] ?? 0) * 100);
        return h(
          'span',
          { class: 'chip', style: `--c:${slotColor(a.index)}` },
          h('b', {}, slotLetter(a.index)),
          ` ${a.slot.name || 'untitled'} `,
          h('span', { class: 'pct' }, `${pct}%`),
          h('i', { style: `width:${pct}%` }),
        );
      }),
    );
  }

  // ---- interaction -------------------------------------------------------------

  private hit(e: PointerEvent): Drag {
    const r = this.canvas.getBoundingClientRect();
    const x = e.clientX - r.left;
    const y = e.clientY - r.top;
    const { w, areaTop, laneTop } = this.geometry();
    const model = timelineModel(this.app.state);
    if (y < areaTop + 4 && y > RULER_H - 4) {
      let best = -1;
      let bestD = 14;
      model.positions.forEach((p, i) => {
        const d = Math.abs(this.x(p, w) - x);
        if (d < bestD) {
          best = i;
          bestD = d;
        }
      });
      if (best >= 0) return { kind: 'node', index: best };
    }
    if (y >= laneTop - 2) {
      for (let i = 0; i + 1 < model.positions.length; i++) {
        const seg = model.segments[i];
        const x0 = this.x(model.positions[i], w);
        const x1 = this.x(model.positions[i + 1], w);
        if (x < x0 - 6 || x > x1 + 6) continue;
        const ws = x0 + (x1 - x0) * seg.tStart;
        const we = x0 + (x1 - x0) * seg.tEnd;
        if (Math.abs(x - ws) < 7 && Math.abs(x - ws) <= Math.abs(x - we)) return { kind: 'window', segment: i, edge: 'start' };
        if (Math.abs(x - we) < 7) return { kind: 'window', segment: i, edge: 'end' };
        if (x >= x0 && x <= x1) {
          this.select(i);
          return null;
        }
      }
      return null;
    }
    return { kind: 'scrub' };
  }

  private select(i: number): void {
    this.selectedSegment = i;
    this.dirty = true;
    this.onSelectSegment(i);
  }

  private pointerDown(e: PointerEvent): void {
    this.drag = this.hit(e);
    if (!this.drag) return;
    this.canvas.setPointerCapture(e.pointerId);
    if (this.drag.kind === 'window') this.select(this.drag.segment);
    this.pointerMove(e);
  }

  private pointerMove(e: PointerEvent): void {
    const r = this.canvas.getBoundingClientRect();
    const x = e.clientX - r.left;
    const { w } = this.geometry();
    if (!this.drag) {
      const hv = this.peek(e);
      this.canvas.style.cursor = hv;
      return;
    }
    const p = this.p(x, w);
    if (this.drag.kind === 'scrub') this.app.seek(p);
    else if (this.drag.kind === 'node') this.app.moveNode(this.drag.index, p);
    else {
      const model = timelineModel(this.app.state);
      const i = this.drag.segment;
      const a = model.positions[i];
      const b = model.positions[i + 1];
      const local = b > a ? (p - a) / (b - a) : 0;
      const seg = model.segments[i];
      if (this.drag.edge === 'start') this.app.updateSegment(i, { tStart: Math.min(local, seg.tEnd) });
      else this.app.updateSegment(i, { tEnd: Math.max(local, seg.tStart) });
    }
  }

  private peek(e: PointerEvent): string {
    const r = this.canvas.getBoundingClientRect();
    const y = e.clientY - r.top;
    const { areaTop, laneTop } = this.geometry();
    if (y < areaTop + 4 && y > RULER_H - 4) return 'ew-resize';
    if (y >= laneTop - 2) return 'pointer';
    return 'crosshair';
  }

  private pointerUp(e: PointerEvent): void {
    if (this.drag && this.canvas.hasPointerCapture(e.pointerId)) this.canvas.releasePointerCapture(e.pointerId);
    this.drag = null;
  }

  private keyDown(e: KeyboardEvent): void {
    const step = e.shiftKey ? 0.05 : 0.005;
    if (e.key === 'ArrowLeft') this.app.seek(this.app.live.position - step);
    else if (e.key === 'ArrowRight') this.app.seek(this.app.live.position + step);
    else return;
    e.preventDefault();
  }
}
