/**
 * Performance controls: Character, drift, randomise/undo, MIDI learn, and
 * the effects section.
 */

import type { App } from '../app';
import { CHARACTER_SUMMARY } from '../dsp/character';
import { MidiController, PARAMS, paramDef, type ParamId } from '../control/midi';
import type { RandomGroup } from '../control/randomise';
import { h, slider } from './dom';
import type { Panel } from './engine-panels';

const pct = (v: number) => `${Math.round(v * 100)}%`;

export function characterControl(app: App): Panel {
  const value = h('b', { class: 'character-value' });
  const desc = h('p', { class: 'character-desc' });
  const input = h('input', { type: 'range', min: '0', max: '1', step: '0.001', value: String(app.state.character), 'aria-label': 'Original Character to Strange Hybrid', class: 'character-range' });
  input.addEventListener('input', () => app.setCharacter(parseFloat(input.value)));
  input.addEventListener('dblclick', () => app.setCharacter(0));
  return {
    root: h(
      'div',
      { class: 'character' },
      h('div', { class: 'character-labels' }, h('span', {}, 'Original Character'), value, h('span', {}, 'Strange Hybrid')),
      input,
      desc,
    ),
    refresh() {
      if (document.activeElement !== input) input.value = String(app.state.character);
      value.textContent = pct(app.state.character);
      desc.textContent = CHARACTER_SUMMARY[app.state.engine];
    },
  };
}

export function driftPanel(app: App): Panel {
  const d = () => app.state.drift;
  const toggle = h('button', { type: 'button', class: 'small', onclick: () => app.setDrift({ enabled: !d().enabled }) }, 'Drift');
  const amount = slider({ label: 'Amount', min: 0, max: 0.5, step: 0.005, value: d().amount, format: pct, onInput: (v) => app.setDrift({ amount: v }), title: 'How far either side of the morph position the drift may wander' });
  const speed = slider({ label: 'Speed', min: 0.5, max: 60, step: 0.5, value: d().speed, log: true, format: (v) => `${v}/min`, onInput: (v) => app.setDrift({ speed: v }), title: 'New random destinations per minute' });
  const smooth = slider({ label: 'Smoothness', min: 0, max: 1, step: 0.01, value: d().smoothness, format: pct, onInput: (v) => app.setDrift({ smoothness: v }) });
  return {
    root: h('div', { class: 'param-group' }, h('h3', {}, 'Morph drift'), toggle, amount.root, speed.root, smooth.root),
    refresh() {
      toggle.classList.toggle('on', d().enabled);
      toggle.textContent = d().enabled ? 'Drift on' : 'Drift off';
      amount.set(d().amount);
      speed.set(d().speed);
      smooth.set(d().smoothness);
    },
  };
}

export function randomisePanel(app: App): Panel {
  const groups: [RandomGroup, string][] = [
    ['granular', 'Granular'],
    ['spectral', 'Spectral'],
    ['curves', 'Transition curves'],
    ['effects', 'Sound shaping'],
  ];
  const undo = h('button', { type: 'button', class: 'small', onclick: () => app.undoRandomise() }, 'Undo');
  return {
    root: h(
      'div',
      { class: 'param-group' },
      h('h3', {}, 'Randomise'),
      h('div', { class: 'button-row' }, ...groups.map(([g, label]) => h('button', { type: 'button', class: 'small', 'data-random': g, onclick: () => app.randomise(g) }, label))),
      undo,
    ),
    refresh() {
      undo.disabled = app.undoCount === 0;
      undo.textContent = app.undoCount ? `Undo (${app.undoCount})` : 'Undo';
    },
  };
}

export function midiPanel(app: App, midi: MidiController): Panel {
  const enable = h('button', { type: 'button', class: 'small', onclick: () => void midi.enable() }, 'Enable MIDI');
  const status = h('small', { class: 'midi-status' });
  const select = h('select', { class: 'midi-select', 'aria-label': 'Parameter to learn' }, ...PARAMS.map((p) => h('option', { value: p.id }, p.label)));
  const learn = h('button', { type: 'button', class: 'small', onclick: () => midi.learn(midi.learning ? null : (select.value as ParamId)) }, 'Learn');
  const list = h('ul', { class: 'midi-list' });
  const refresh = () => {
    enable.hidden = midi.status === 'on';
    status.textContent =
      midi.status === 'on'
        ? midi.inputs.length
          ? `Inputs: ${midi.inputs.join(', ')}`
          : 'MIDI on, no inputs connected'
        : midi.status === 'unsupported'
          ? 'Web MIDI is not available in this browser'
          : midi.status === 'denied'
            ? 'MIDI access was refused'
            : 'Off';
    learn.disabled = midi.status !== 'on';
    learn.classList.toggle('on', !!midi.learning);
    learn.textContent = midi.learning ? `Move a control for “${paramDef(midi.learning)?.label}”…` : 'Learn';
    list.replaceChildren(
      ...app.state.midi.map((m, i) =>
        h(
          'li',
          {},
          `${paramDef(m.param)?.label ?? m.param}: ${m.type === 'cc' ? 'CC' : 'Note'} ${m.number} · ch ${m.channel + 1} `,
          h('button', { type: 'button', class: 'icon', title: 'Remove mapping', onclick: () => app.setMidiMappings(app.state.midi.filter((_, j) => j !== i)) }, '✕'),
        ),
      ),
    );
  };
  midi.onChange = refresh;
  return {
    root: h('div', { class: 'param-group' }, h('h3', {}, 'MIDI'), h('div', { class: 'button-row' }, enable, status), h('div', { class: 'button-row' }, select, learn), list),
    refresh,
  };
}

export function effectsPanel(app: App): Panel {
  const e = () => app.state.effects;
  const enable = h('button', { type: 'button', class: 'small', onclick: () => app.setEffects({ enabled: !e().enabled }) });
  const hz = (v: number) => (v >= 1000 ? `${(v / 1000).toFixed(1)} kHz` : `${Math.round(v)} Hz`);
  const lp = slider({ label: 'Low-pass', min: 40, max: 20000, step: 1, value: e().lowpassHz, log: true, format: hz, onInput: (v) => app.setEffects({ lowpassHz: v }) });
  const hp = slider({ label: 'High-pass', min: 20, max: 8000, step: 1, value: e().highpassHz, log: true, format: hz, onInput: (v) => app.setEffects({ highpassHz: v }) });
  const res = slider({ label: 'Resonance', min: 0, max: 1, step: 0.01, value: e().resonance, format: pct, onInput: (v) => app.setEffects({ resonance: v }) });
  const width = slider({ label: 'Stereo width', min: 0, max: 2, step: 0.01, value: e().width, format: pct, onInput: (v) => app.setEffects({ width: v }) });
  const rv = slider({ label: 'Reverb send', min: 0, max: 1, step: 0.01, value: e().reverbSend, format: pct, onInput: (v) => app.setEffects({ reverbSend: v }) });
  const rvSize = slider({ label: 'Reverb size', min: 0, max: 1, step: 0.01, value: e().reverbSize, format: pct, onInput: (v) => app.setEffects({ reverbSize: v }) });
  const dl = slider({ label: 'Delay send', min: 0, max: 1, step: 0.01, value: e().delaySend, format: pct, onInput: (v) => app.setEffects({ delaySend: v }) });
  const dlTime = slider({ label: 'Delay time', min: 0.02, max: 2, step: 0.01, value: e().delayTime, log: true, format: (v) => `${Math.round(v * 1000)} ms`, onInput: (v) => app.setEffects({ delayTime: v }) });
  const dlFb = slider({ label: 'Feedback', min: 0, max: 0.92, step: 0.01, value: e().delayFeedback, format: pct, onInput: (v) => app.setEffects({ delayFeedback: v }) });
  const grid = h('div', { class: 'param-grid' }, hp.root, lp.root, res.root, width.root, rv.root, rvSize.root, dl.root, dlTime.root, dlFb.root);
  return {
    root: h('section', { class: 'panel effects-panel' }, h('div', { class: 'section-head' }, h('h3', {}, 'Sound shaping'), enable), grid),
    refresh() {
      const p = e();
      enable.textContent = p.enabled ? 'On' : 'Bypassed';
      enable.classList.toggle('on', p.enabled);
      grid.classList.toggle('bypassed', !p.enabled);
      lp.set(p.lowpassHz);
      hp.set(p.highpassHz);
      res.set(p.resonance);
      width.set(p.width);
      rv.set(p.reverbSend);
      rvSize.set(p.reverbSize);
      dl.set(p.delaySend);
      dlTime.set(p.delayTime);
      dlFb.set(p.delayFeedback);
    },
  };
}
