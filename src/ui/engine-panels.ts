/**
 * Engine-specific parameter panels. Only the selected engine's panel is
 * shown; all of them stay bound to the project state.
 */

import type { App } from '../app';
import { WINDOW_TYPES, type WindowType } from '../dsp/windows';
import { h, segmented, slider } from './dom';

export interface Panel {
  root: HTMLElement;
  refresh(): void;
}

export function crossfadePanel(app: App): Panel {
  const law = segmented(
    [
      { value: 'equalPower', label: 'Equal power', title: 'Constant loudness for unrelated sounds (recommended)' },
      { value: 'linear', label: 'Linear', title: 'Constant amplitude; suits very similar sounds' },
    ],
    app.state.gainLaw,
    (v) => app.setGainLaw(v),
    'Crossfade law',
  );
  const loopXf = slider({
    label: 'Loop crossfade',
    min: 0,
    max: 500,
    step: 5,
    value: app.state.loopCrossfadeMs,
    format: (v) => `${v} ms`,
    onInput: (v) => app.setLoopCrossfadeMs(v),
    title: 'Length of the crossfade that hides each loop seam',
  });
  return {
    root: h('div', { class: 'param-group' }, h('h3', {}, 'Crossfade engine'), h('small', {}, 'Crossfade law'), law.root, loopXf.root),
    refresh() {
      law.set(app.state.gainLaw);
      loopXf.set(app.state.loopCrossfadeMs);
    },
  };
}

export function granularPanel(app: App): Panel {
  const g = () => app.state.granular;
  const pct = (v: number) => `${Math.round(v * 100)}%`;
  const size = slider({ label: 'Grain size', min: 10, max: 500, step: 1, value: g().sizeMs, log: true, format: (v) => `${v} ms`, onInput: (v) => app.setGranular({ sizeMs: v }) });
  const overlap = slider({ label: 'Overlap', min: 1, max: 16, step: 0.5, value: g().overlap, format: (v) => `${v.toFixed(1)}×`, onInput: (v) => app.setGranular({ overlap: v }), title: 'Average number of grains sounding at once' });
  const density = slider({ label: 'Density', min: 0.02, max: 1, step: 0.01, value: g().density, format: pct, onInput: (v) => app.setGranular({ density: v }), title: 'Chance that each scheduled grain sounds: low = sparse, high = continuous' });
  const posRand = slider({ label: 'Position random', min: 0, max: 1, step: 0.01, value: g().positionRandom, format: pct, onInput: (v) => app.setGranular({ positionRandom: v }), title: 'How far from the scan head grains may start, as a share of the loop' });
  const pitchRand = slider({ label: 'Pitch variation', min: 0, max: 1200, step: 5, value: g().pitchRandomCents, format: (v) => `±${v} ct`, onInput: (v) => app.setGranular({ pitchRandomCents: v }) });
  const transpose = slider({ label: 'Transpose', min: -24, max: 24, step: 1, value: g().transpose, format: (v) => `${v > 0 ? '+' : ''}${v} st`, onInput: (v) => app.setGranular({ transpose: v }) });
  const speed = slider({ label: 'Playback speed', min: -2, max: 2, step: 0.01, value: g().speed, format: (v) => `${v.toFixed(2)}×`, onInput: (v) => app.setGranular({ speed: v }), title: 'Speed of the scan head through each sample (0 = stand still, negative = backwards)' });
  const spread = slider({ label: 'Stereo spread', min: 0, max: 1, step: 0.01, value: g().spread, format: pct, onInput: (v) => app.setGranular({ spread: v }) });
  const jitter = slider({ label: 'Onset jitter', min: 0, max: 1, step: 0.01, value: g().jitter, format: pct, onInput: (v) => app.setGranular({ jitter: v }), title: '0 = regular grain pulse, 100% = fully asynchronous cloud' });
  const win = segmented(
    WINDOW_TYPES.map((w) => ({ value: w, label: { hann: 'Hann', tukey: 'Tukey', gaussian: 'Gauss', trapezoid: 'Trapezoid' }[w] })),
    g().window,
    (v: WindowType) => app.setGranular({ window: v }),
    'Grain envelope',
  );
  const seed = h('input', { type: 'number', class: 'seed', min: '1', max: '99999', value: String(g().seed), 'aria-label': 'Random seed' });
  seed.addEventListener('change', () => {
    const v = Math.max(1, Math.min(99999, Math.round(Number(seed.value)) || 1));
    app.setGranular({ seed: v });
  });
  const dice = h('button', { type: 'button', class: 'small', title: 'New random seed', onclick: () => app.setGranular({ seed: 1 + Math.floor(Math.random() * 99999) }) }, 'New seed');
  const grains = h('span', { class: 'grain-count' });
  app.onStatus(() => (grains.textContent = `${app.live.grains} grains sounding`));
  return {
    root: h(
      'div',
      { class: 'param-group granular-panel' },
      h('h3', {}, 'Granular engine ', grains),
      h('div', { class: 'param-grid' }, size.root, overlap.root, density.root, posRand.root, pitchRand.root, transpose.root, speed.root, spread.root, jitter.root),
      h('small', {}, 'Grain envelope'),
      win.root,
      h('div', { class: 'seed-row' }, h('small', {}, 'Seed'), seed, dice),
    ),
    refresh() {
      const p = g();
      size.set(p.sizeMs);
      overlap.set(p.overlap);
      density.set(p.density);
      posRand.set(p.positionRandom);
      pitchRand.set(p.pitchRandomCents);
      transpose.set(p.transpose);
      speed.set(p.speed);
      spread.set(p.spread);
      jitter.set(p.jitter);
      win.set(p.window);
      if (document.activeElement !== seed) seed.value = String(p.seed);
    },
  };
}
