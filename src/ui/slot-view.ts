/**
 * Sample slot cards: loading (button or drag-and-drop), waveform with trim
 * and loop markers, gain, normalise, reverse, mute and solo.
 */

import type { App } from '../app';
import { MAX_SLOTS, MIN_SLOTS, slotLetter, type SlotState } from '../state/project';
import { fitCanvas, h, slider, slotColor } from './dom';

const ACCEPT = '.wav,.wave,.aif,.aiff,.aifc,.mp3,.m4a,.aac,.flac,.ogg,.oga,.opus,audio/*';

type Marker = 'trimStart' | 'trimEnd' | 'loopStart' | 'loopEnd';

class SlotCard {
  readonly root: HTMLElement;
  private readonly canvas: HTMLCanvasElement;
  private readonly title: HTMLElement;
  private readonly info: HTMLElement;
  private readonly message: HTMLElement;
  private readonly gain;
  private readonly btnNorm: HTMLButtonElement;
  private readonly btnRev: HTMLButtonElement;
  private readonly btnMute: HTMLButtonElement;
  private readonly btnSolo: HTMLButtonElement;
  private readonly meter: HTMLElement;
  private readonly fileInput: HTMLInputElement;
  private peaks: { width: number; audio: unknown; min: Float32Array; max: Float32Array } | null = null;
  private drag: Marker | null = null;

  constructor(private readonly app: App, private readonly index: number) {
    const color = slotColor(index);
    this.fileInput = h('input', { type: 'file', accept: ACCEPT, hidden: true });
    this.fileInput.addEventListener('change', () => {
      const f = this.fileInput.files?.[0];
      if (f) void app.loadFile(index, f);
      this.fileInput.value = '';
    });
    this.title = h('span', { class: 'slot-name' });
    this.info = h('span', { class: 'slot-info' });
    this.message = h('div', { class: 'slot-msg', role: 'status' });
    this.canvas = h('canvas', { class: 'wave', title: 'Drag the markers: grey = trim, coloured = loop. Click to load when empty.' });
    this.gain = slider({
      label: 'Gain',
      min: -36,
      max: 12,
      step: 0.5,
      value: 0,
      format: (v) => `${v > 0 ? '+' : ''}${v.toFixed(1)} dB`,
      onInput: (v) => app.updateSlot(index, { gainDb: v }),
    });
    const toggle = (label: string, title: string, fn: () => void) => h('button', { type: 'button', class: 'toggle', title, onclick: fn }, label);
    const s = () => app.state.slots[index];
    this.btnNorm = toggle('Norm', 'Normalise the sample peak to −1 dBFS', () => app.updateSlot(index, { normalize: !s().normalize }));
    this.btnRev = toggle('Rev', 'Play this sample in reverse', () => app.updateSlot(index, { reverse: !s().reverse }));
    this.btnMute = toggle('M', 'Mute', () => app.updateSlot(index, { mute: !s().mute }));
    this.btnSolo = toggle('S', 'Solo', () => app.updateSlot(index, { solo: !s().solo }));
    this.meter = h('i');
    const removable = app.state.slots.length > MIN_SLOTS;
    this.root = h(
      'div',
      { class: 'slot', style: `--c:${color}`, 'data-slot': String(index) },
      h(
        'div',
        { class: 'slot-head' },
        h('span', { class: 'badge' }, slotLetter(index)),
        this.title,
        h('button', { type: 'button', class: 'icon', title: 'Load or replace sample', onclick: () => this.fileInput.click() }, 'Load'),
        h('button', { type: 'button', class: 'icon', title: 'Clear sample', onclick: () => app.clearSlot(index) }, 'Clear'),
        removable && h('button', { type: 'button', class: 'icon', title: 'Remove this slot', onclick: () => app.removeSlot(index) }, '✕'),
      ),
      this.canvas,
      h('div', { class: 'slot-row' }, this.info, h('span', { class: 'contrib' }, this.meter)),
      h('div', { class: 'slot-row controls' }, this.gain.root, this.btnNorm, this.btnRev, this.btnMute, this.btnSolo),
      this.message,
      this.fileInput,
    );
    this.root.addEventListener('dragover', (e) => {
      e.preventDefault();
      this.root.classList.add('drop');
    });
    this.root.addEventListener('dragleave', () => this.root.classList.remove('drop'));
    this.root.addEventListener('drop', (e) => {
      e.preventDefault();
      this.root.classList.remove('drop');
      const f = e.dataTransfer?.files?.[0];
      if (f) void app.loadFile(index, f);
    });
    this.canvas.addEventListener('pointerdown', (e) => this.pointerDown(e));
    this.canvas.addEventListener('pointermove', (e) => this.pointerMove(e));
    this.canvas.addEventListener('pointerup', (e) => this.pointerUp(e));
    this.canvas.addEventListener('pointercancel', (e) => this.pointerUp(e));
    new ResizeObserver(() => this.drawWave()).observe(this.canvas);
    this.update();
  }

  private get slot(): SlotState {
    return this.app.state.slots[this.index];
  }

  update(): void {
    const s = this.slot;
    if (!s) return;
    this.root.classList.toggle('empty', !s.audio);
    this.root.classList.toggle('loading', s.loading);
    this.title.textContent = s.loading ? 'Loading…' : s.audio ? s.name || 'untitled' : 'Empty: drop a file or click Load';
    if (s.audio) {
      const len = s.audio.frames / s.audio.sampleRate;
      const ch = s.audio.channels.length === 1 ? 'mono' : 'stereo';
      const conv = s.audio.originalSampleRate && s.audio.originalSampleRate !== s.audio.sampleRate ? ` · resampled from ${(s.audio.originalSampleRate / 1000).toFixed(1)} kHz` : '';
      this.info.textContent = `${len.toFixed(2)} s · ${ch} · loop ${(s.loopEnd - s.loopStart).toFixed(2)} s${conv}`;
    } else this.info.textContent = '';
    this.gain.set(s.gainDb);
    this.btnNorm.classList.toggle('on', s.normalize);
    this.btnRev.classList.toggle('on', s.reverse);
    this.btnMute.classList.toggle('on', s.mute);
    this.btnSolo.classList.toggle('on', s.solo);
    for (const b of [this.btnNorm, this.btnRev, this.btnMute, this.btnSolo]) b.disabled = !s.audio;
    this.gain.input.disabled = !s.audio;
    this.message.textContent = s.error ?? s.warnings.join(' ');
    this.message.classList.toggle('error', !!s.error);
    this.drawWave();
  }

  updateLive(): void {
    const f = this.app.live.fractions[this.index] ?? 0;
    this.meter.style.width = `${Math.round(f * 100)}%`;
  }

  private xOf(sec: number, w: number): number {
    const a = this.slot.audio!;
    return (sec / (a.frames / a.sampleRate)) * w;
  }

  private drawWave(): void {
    const s = this.slot;
    if (!s) return;
    const { ctx, w, h: hgt } = fitCanvas(this.canvas);
    ctx.fillStyle = '#0f1215';
    ctx.fillRect(0, 0, w, hgt);
    const color = slotColor(this.index);
    if (!s.audio) {
      ctx.fillStyle = '#4a515b';
      ctx.font = '13px system-ui, sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('WAV · AIFF · MP3 · FLAC · OGG', w / 2, hgt / 2);
      return;
    }
    if (!this.peaks || this.peaks.width !== w || this.peaks.audio !== s.audio) this.peaks = computePeaks(s.audio.channels, w, s.audio);
    const mid = hgt / 2;
    const { min, max } = this.peaks;
    // loop region background
    const ls = this.xOf(s.loopStart, w);
    const le = this.xOf(s.loopEnd, w);
    ctx.fillStyle = color + '1f';
    ctx.fillRect(ls, 0, le - ls, hgt);
    ctx.fillStyle = color;
    for (let x = 0; x < w; x++) {
      const i = s.reverse ? w - 1 - x : x;
      const y0 = mid - max[i] * mid * 0.95;
      const y1 = mid - min[i] * mid * 0.95;
      ctx.fillRect(x, y0, 1, Math.max(1, y1 - y0));
    }
    // trimmed-away areas
    const ts = this.xOf(s.trimStart, w);
    const te = this.xOf(s.trimEnd, w);
    ctx.fillStyle = 'rgba(8,10,12,0.72)';
    ctx.fillRect(0, 0, ts, hgt);
    ctx.fillRect(te, 0, w - te, hgt);
    // markers
    const mark = (x: number, c: string, top: boolean) => {
      ctx.fillStyle = c;
      ctx.fillRect(x - 1, 0, 2, hgt);
      ctx.beginPath();
      if (top) {
        ctx.moveTo(x - 6, 0);
        ctx.lineTo(x + 6, 0);
        ctx.lineTo(x, 9);
      } else {
        ctx.moveTo(x - 6, hgt);
        ctx.lineTo(x + 6, hgt);
        ctx.lineTo(x, hgt - 9);
      }
      ctx.fill();
    };
    mark(ts, '#9aa3ad', false);
    mark(te, '#9aa3ad', false);
    mark(ls, '#ffffff', true);
    mark(le, '#ffffff', true);
    if (s.reverse) {
      ctx.fillStyle = '#e6e9ee';
      ctx.font = '11px system-ui, sans-serif';
      ctx.textAlign = 'right';
      ctx.textBaseline = 'top';
      ctx.fillText('◀ reversed', w - 6, 12);
    }
  }

  private pointerDown(e: PointerEvent): void {
    const s = this.slot;
    if (!s.audio) {
      this.fileInput.click();
      return;
    }
    const r = this.canvas.getBoundingClientRect();
    const x = e.clientX - r.left;
    const y = e.clientY - r.top;
    const w = r.width;
    const top = y < r.height / 2;
    // Prefer loop markers in the top half, trim markers in the bottom half.
    const order: Marker[] = top ? ['loopStart', 'loopEnd', 'trimStart', 'trimEnd'] : ['trimStart', 'trimEnd', 'loopStart', 'loopEnd'];
    let best: Marker | null = null;
    let bestD = 10;
    for (const m of order) {
      const d = Math.abs(this.xOf(s[m], w) - x);
      if (d < bestD - 0.01) {
        best = m;
        bestD = d;
      }
    }
    if (!best) return;
    this.drag = best;
    this.canvas.setPointerCapture(e.pointerId);
  }

  private pointerMove(e: PointerEvent): void {
    const s = this.slot;
    if (!s.audio) return;
    const r = this.canvas.getBoundingClientRect();
    const x = Math.max(0, Math.min(r.width, e.clientX - r.left));
    if (!this.drag) return;
    const sec = (x / r.width) * (s.audio.frames / s.audio.sampleRate);
    this.app.setMarkers(this.index, { [this.drag]: sec }, false);
  }

  private pointerUp(e: PointerEvent): void {
    if (!this.drag) return;
    if (this.canvas.hasPointerCapture(e.pointerId)) this.canvas.releasePointerCapture(e.pointerId);
    this.drag = null;
    // Send the region to the audio thread once the drag ends; it is swapped
    // in behind a short declick fade.
    this.app.setMarkers(this.index, {}, true);
  }
}

function computePeaks(channels: Float32Array[], width: number, audio: unknown) {
  const len = channels[0].length;
  const min = new Float32Array(width);
  const max = new Float32Array(width);
  for (let x = 0; x < width; x++) {
    const a = Math.floor((x * len) / width);
    const b = Math.max(a + 1, Math.floor(((x + 1) * len) / width));
    let lo = 0;
    let hi = 0;
    for (const ch of channels) {
      for (let i = a; i < b; i++) {
        const v = ch[i];
        if (v < lo) lo = v;
        if (v > hi) hi = v;
      }
    }
    min[x] = lo;
    max[x] = hi;
  }
  return { width, audio, min, max };
}

export class SlotsView {
  readonly root = h('section', { class: 'slots', 'aria-label': 'Sample slots' });
  private cards: SlotCard[] = [];

  constructor(private readonly app: App) {
    this.rebuild();
    app.on((k) => {
      if (k === 'all') this.rebuild();
      else if (k === 'slots') this.cards.forEach((c) => c.update());
    });
    app.onStatus(() => this.cards.forEach((c) => c.updateLive()));
  }

  private rebuild(): void {
    this.cards = this.app.state.slots.map((_, i) => new SlotCard(this.app, i));
    const add =
      this.app.state.slots.length < MAX_SLOTS
        ? h('button', { type: 'button', class: 'slot add', onclick: () => this.app.addSlot() }, h('span', {}, '+'), h('small', {}, `Add slot (${this.app.state.slots.length}/${MAX_SLOTS})`))
        : null;
    this.root.replaceChildren(...this.cards.map((c) => c.root), ...(add ? [add] : []));
  }
}
