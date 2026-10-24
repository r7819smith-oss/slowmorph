/**
 * SlowMorph entry point: builds the interface and runs the display loop.
 * Audio runs in the AudioWorklet; this thread only draws and forwards
 * control changes.
 */

import './styles.css';
import { App } from './app';
import { formatTime, parseDuration } from './dsp/clock';
import { CURVE_TYPES, gainToDb, type CurveType } from './dsp/curves';
import { activeSlots, getSegment, slotLetter } from './state/project';
import { h, segmented, slider } from './ui/dom';
import { SlotsView } from './ui/slot-view';
import { TimelineView } from './ui/timeline-view';
import { crossfadePanel, granularPanel, spectralPanel } from './ui/engine-panels';
import { characterControl, driftPanel, effectsPanel, midiPanel, randomisePanel } from './ui/perform-panels';
import { MidiController } from './control/midi';
import { Session } from './session';
import { recordButton, sessionPanel } from './ui/session-panel';

const app = new App();
(window as unknown as { slowmorph: App }).slowmorph = app;
const session = new Session(app);
(window as unknown as { slowmorphSession: Session }).slowmorphSession = session;

const DURATION_PRESETS: [string, number][] = [
  ['30s', 30], ['1m', 60], ['5m', 300], ['10m', 600], ['20m', 1200], ['30m', 1800], ['60m', 3600],
];

// ---- header / transport -------------------------------------------------------

const btn = (label: string, title: string, onclick: () => void, cls = '') =>
  h('button', { type: 'button', class: `tbtn ${cls}`, title, 'aria-label': title, onclick }, label);

const playBtn = btn('▶', 'Play / pause (Space)', () => app.togglePlay(), 'play');
const stopBtn = btn('■', 'Stop (Esc)', () => app.stop());
const restartBtn = btn('⏮', 'Restart (Home)', () => app.restart());
const reverseBtn = btn('⇄', 'Reverse morph direction (R)', () => app.setDirection(app.live.direction === 1 ? -1 : 1));
const freezeBtn = btn('❄', 'Freeze: hold the current morph position (F)', () => app.toggleFreeze());
const stateBadge = h('span', { class: 'state' });
const recBtn = recordButton(session);
const meterL = h('i');
const meterR = h('i');
const limitLed = h('span', { class: 'led', title: 'Safety limiter active' }, 'LIM');
const master = slider({
  label: 'Out',
  min: -48,
  max: 6,
  step: 0.5,
  value: app.state.masterDb,
  format: (v) => `${v.toFixed(1)} dB`,
  onInput: (v) => app.setMasterDb(v),
});
const engineLabel = h('span', { class: 'engine-label' });

const header = h(
  'header',
  { class: 'top' },
  h('h1', {}, 'Slow', h('span', {}, 'Morph')),
  h('div', { class: 'transport' }, restartBtn, playBtn, stopBtn, reverseBtn, freezeBtn),
  recBtn.root,
  h('div', { class: 'status' }, engineLabel, stateBadge),
  h('div', { class: 'master' }, h('div', { class: 'meter', title: 'Output level' }, meterL, meterR), limitLed, master.root),
);

const banner = h('div', { class: 'banner', hidden: true });
banner.addEventListener('click', () => void app.engine.resume());

// ---- timeline section ----------------------------------------------------------

const timeline = new TimelineView(app);
const elapsedEl = h('b');
const remainEl = h('b');
const totalEl = h('b');
const durationInput = h('input', { type: 'text', class: 'duration-input', 'aria-label': 'Exact duration', placeholder: 'e.g. 12:30 or 45m', spellcheck: false });
const commitDuration = () => {
  const v = parseDuration(durationInput.value);
  if (v === null) {
    durationInput.classList.add('bad');
    return;
  }
  durationInput.classList.remove('bad');
  app.setDuration(v);
};
durationInput.addEventListener('change', commitDuration);
durationInput.addEventListener('keydown', (e) => e.key === 'Enter' && commitDuration());
const presetRow = segmented(
  DURATION_PRESETS.map(([label, s]) => ({ value: String(s), label })),
  String(app.state.durationSec),
  (v) => app.setDuration(Number(v)),
  'Duration presets',
);
const playMode = segmented(
  [
    { value: 'once', label: 'Once', title: 'Play the morph once and hold the last sample' },
    { value: 'loop', label: 'Loop', title: 'Loop the whole morph (glides back to the first sample)' },
    { value: 'pingpong', label: 'Ping-pong', title: 'Morph forwards then backwards, forever' },
  ],
  app.state.playMode,
  (v) => app.setPlayMode(v),
  'Play mode',
);
const equalBtn = h('button', { type: 'button', class: 'small', title: 'Spread the sample nodes evenly', onclick: () => app.equalSpacing() }, 'Equal spacing');

const timelineSection = h(
  'section',
  { class: 'panel timeline-panel' },
  h(
    'div',
    { class: 'times' },
    h('div', {}, h('small', {}, 'Elapsed'), elapsedEl),
    h('div', {}, h('small', {}, 'Remaining'), remainEl),
    h('div', {}, h('small', {}, 'Duration'), totalEl),
    h('div', { class: 'grow' }),
    playMode.root,
    equalBtn,
  ),
  timeline.root,
  h('div', { class: 'duration-row' }, h('small', {}, 'Morph duration'), presetRow.root, durationInput),
);

// ---- modes + character ---------------------------------------------------------

const modes = segmented(
  [
    { value: 'crossfade', label: 'Crossfade', title: 'Mode A: amplitude crossfading between independently looping samples' },
    { value: 'granular', label: 'Granular', title: 'Mode B: grains drawn from the samples with timeline probabilities' },
    { value: 'spectral', label: 'Spectral', title: 'Mode C: STFT spectral interpolation and resynthesis' },
  ],
  app.state.engine,
  (v) => app.setEngine(v),
  'Morph engine',
);
const character = characterControl(app);
const modeSection = h('section', { class: 'panel modes' }, h('div', {}, h('small', {}, 'Morph engine'), modes.root), character.root);

// ---- engine parameters -----------------------------------------------------------

const panels = { crossfade: crossfadePanel(app), granular: granularPanel(app), spectral: spectralPanel(app) };
const enginePanel = h('div', { class: 'engine-panel' });

const segTitle = h('span');
const curveSel = segmented(
  CURVE_TYPES.map((c) => ({ value: c, label: { linear: 'Linear', sCurve: 'S-curve', easeIn: 'Ease in', easeOut: 'Ease out' }[c] })),
  'linear' as CurveType,
  (v) => app.updateSegment(timeline.selectedSegment, { curve: v }),
  'Transition curve',
);
const shape = slider({ label: 'Curve strength', min: 0, max: 1, step: 0.01, value: 0.5, format: (v) => v.toFixed(2), onInput: (v) => app.updateSegment(timeline.selectedSegment, { shape: v }) });
const winStart = slider({ label: 'Window start', min: 0, max: 1, step: 0.01, value: 0, format: (v) => `${Math.round(v * 100)}%`, onInput: (v) => app.updateSegment(timeline.selectedSegment, { tStart: v }) });
const winEnd = slider({ label: 'Window end', min: 0, max: 1, step: 0.01, value: 1, format: (v) => `${Math.round(v * 100)}%`, onInput: (v) => app.updateSegment(timeline.selectedSegment, { tEnd: v }) });
const applyAll = h('button', { type: 'button', class: 'small', onclick: () => app.applySegmentToAll(timeline.selectedSegment) }, 'Apply to all transitions');
const segPanel = h('div', { class: 'seg-editor' }, h('h3', {}, 'Transition ', segTitle), curveSel.root, shape.root, winStart.root, winEnd.root, applyAll);

const paramsSection = h(
  'section',
  { class: 'panel params' },
  enginePanel,
  segPanel,
  h(
    'div',
    { class: 'param-group help' },
    h('h3', {}, 'Keys'),
    h('p', {}, h('kbd', {}, 'Space'), ' play/pause · ', h('kbd', {}, 'Esc'), ' stop · ', h('kbd', {}, 'Home'), ' restart · ', h('kbd', {}, 'R'), ' reverse · ', h('kbd', {}, 'F'), ' freeze · ', h('kbd', {}, '←'), h('kbd', {}, '→'), ' nudge position (timeline focused)'),
  ),
);

const slots = new SlotsView(app);

const midi = new MidiController(app);
(window as unknown as { slowmorphMidi: MidiController }).slowmorphMidi = midi;
const drift = driftPanel(app);
const randomise = randomisePanel(app);
const midiUi = midiPanel(app, midi);
const effects = effectsPanel(app);
const performSection = h('section', { class: 'panel perform' }, drift.root, randomise.root, midiUi.root);
const sessionUi = sessionPanel(app, session);
session.onChange = () => {
  sessionUi.refresh();
  recBtn.refresh();
};

document.getElementById('app')!.append(header, banner, timelineSection, modeSection, slots.root, paramsSection, performSection, effects.root, sessionUi.root);
sessionUi.refresh();

// ---- state → UI ----------------------------------------------------------------

function refreshSegmentEditor(): void {
  const act = activeSlots(app.state);
  const n = act.length - 1;
  if (timeline.selectedSegment >= n) timeline.selectedSegment = Math.max(0, n - 1);
  const i = timeline.selectedSegment;
  segPanel.classList.toggle('disabled', n < 1);
  if (n < 1) {
    segTitle.textContent = '(load two samples)';
    return;
  }
  const a = act[i];
  const b = act[i + 1];
  const seg = getSegment(app.state, a.slot, b.slot);
  segTitle.textContent = `${slotLetter(a.index)} → ${slotLetter(b.index)}`;
  curveSel.set(seg.curve);
  shape.set(seg.shape);
  winStart.set(seg.tStart);
  winEnd.set(seg.tEnd);
}
timeline.onSelectSegment = () => refreshSegmentEditor();

function refreshSettings(): void {
  const s = app.state;
  presetRow.set(String(s.durationSec));
  if (document.activeElement !== durationInput) durationInput.value = formatTime(s.durationSec);
  totalEl.textContent = formatTime(s.durationSec);
  playMode.set(s.playMode);
  modes.set(s.engine);
  const panel = panels[s.engine];
  if (enginePanel.firstChild !== panel.root) enginePanel.replaceChildren(panel.root);
  panel.refresh();
  master.set(s.masterDb);
  equalBtn.classList.toggle('on', s.timeline.autoSpace);
  engineLabel.textContent = `Mode: ${s.engine[0].toUpperCase()}${s.engine.slice(1)}`;
  character.refresh();
  drift.refresh();
  randomise.refresh();
  midiUi.refresh();
  effects.refresh();
}

function refreshTransport(): void {
  const l = app.live;
  const playing = l.transport === 'playing';
  playBtn.textContent = playing ? '❚❚' : '▶';
  playBtn.classList.toggle('on', playing);
  reverseBtn.classList.toggle('on', l.direction === -1);
  freezeBtn.classList.toggle('on', l.frozen);
  stateBadge.textContent = l.frozen ? 'Frozen' : l.ended ? 'End' : l.transport === 'playing' ? (l.direction === 1 ? 'Playing' : 'Reversing') : l.transport === 'paused' ? 'Paused' : 'Stopped';
  stateBadge.dataset.state = l.frozen ? 'frozen' : l.transport;
  const cs = l.contextState;
  if (cs === 'failed') {
    banner.hidden = false;
    banner.textContent = l.contextMessage ?? 'Audio failed.';
    banner.classList.add('error');
  } else if (playing && cs !== 'running') {
    banner.hidden = false;
    banner.classList.remove('error');
    banner.textContent = 'Audio is paused by the browser or the audio device changed. Click here to resume.';
  } else banner.hidden = true;
}

app.on((kind) => {
  if (kind === 'all' || kind === 'settings') refreshSettings();
  if (kind === 'all' || kind === 'transport') refreshTransport();
  if (kind === 'all' || kind === 'timeline' || kind === 'slots') refreshSegmentEditor();
  if (kind === 'timeline') randomise.refresh();
});
refreshSettings();
refreshTransport();
refreshSegmentEditor();

// ---- display loop --------------------------------------------------------------

let peakHoldL = 0;
let peakHoldR = 0;
function frame(): void {
  timeline.frame();
  const l = app.live;
  const dur = app.state.durationSec;
  const elapsed = l.position * dur;
  elapsedEl.textContent = formatTime(elapsed);
  remainEl.textContent = formatTime(l.direction === 1 ? dur - elapsed : elapsed);
  peakHoldL = Math.max(l.peakL, peakHoldL * 0.92);
  peakHoldR = Math.max(l.peakR, peakHoldR * 0.92);
  const pct = (p: number) => `${Math.max(0, Math.min(100, ((gainToDb(p) + 60) / 60) * 100))}%`;
  meterL.style.width = pct(peakHoldL);
  meterR.style.width = pct(peakHoldR);
  limitLed.classList.toggle('on', l.limiting);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

// ---- keyboard ------------------------------------------------------------------

window.addEventListener('keydown', (e) => {
  if ((e.metaKey || e.ctrlKey) && (e.key === 's' || e.key === 'o')) {
    e.preventDefault();
    if (e.key === 's') void session.saveProject();
    else (document.querySelector('.session input[type=file]') as HTMLInputElement | null)?.click();
    return;
  }
  const t = e.target as HTMLElement;
  if (t.tagName === 'INPUT' && (t as HTMLInputElement).type === 'text') return;
  if (e.metaKey || e.ctrlKey || e.altKey) return;
  switch (e.key) {
    case ' ': app.togglePlay(); break;
    case 'Escape': app.stop(); break;
    case 'Home': app.restart(); break;
    case 'r': case 'R': app.setDirection(app.live.direction === 1 ? -1 : 1); break;
    case 'f': case 'F': app.toggleFreeze(); break;
    default: return;
  }
  e.preventDefault();
});

// Resume audio on the first interaction (browsers start contexts suspended).
window.addEventListener('pointerdown', () => void app.engine.resume(), { once: true });
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && app.live.transport === 'playing') void app.engine.resume();
});
