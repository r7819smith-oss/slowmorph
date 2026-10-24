/** Project, preset, recording and export controls. */

import type { App } from '../app';
import type { Session } from '../session';
import { formatTime } from '../dsp/clock';
import { canStreamToDisk } from '../audio/wav-sink';
import type { WavFormat } from '../audio/wav';
import { h, segmented } from './dom';
import type { Panel } from './engine-panels';

export function recordButton(session: Session): Panel {
  const time = h('span', { class: 'rec-time' });
  const btn = h(
    'button',
    { type: 'button', class: 'tbtn rec', title: 'Record the output to WAV', 'aria-label': 'Record', onclick: () => (session.recording.active ? void session.stopRecording() : void session.startRecording({ maxSeconds: recordLimit(), fullMorph: false })) },
    '●',
  );
  return {
    root: h('div', { class: 'rec-wrap' }, btn, time),
    refresh() {
      const r = session.recording;
      btn.classList.toggle('on', r.active);
      time.textContent = r.active ? formatTime(r.seconds) + (r.maxSeconds ? ` / ${formatTime(r.maxSeconds)}` : '') : '';
    },
  };
}

let limitSelect: HTMLSelectElement | null = null;
function recordLimit(): number | null {
  const v = Number(limitSelect?.value ?? 0);
  return v > 0 ? v : null;
}

export function sessionPanel(app: App, session: Session): Panel {
  // project
  const name = h('input', { type: 'text', class: 'project-name', 'aria-label': 'Project name', value: session.projectName, spellcheck: false });
  name.addEventListener('change', () => (session.projectName = name.value.trim() || 'Untitled'));
  const openInput = h('input', { type: 'file', accept: '.slowmorph', hidden: true });
  openInput.addEventListener('change', () => {
    const f = openInput.files?.[0];
    if (f) void session.openProjectFile(f);
    openInput.value = '';
  });
  const open = h('button', { type: 'button', class: 'small', title: 'Open a .slowmorph project (⌘O)', onclick: () => openInput.click() }, 'Open…');
  const save = h('button', { type: 'button', class: 'small', title: 'Save the project with its samples (⌘S)', onclick: () => void session.saveProject() }, 'Save…');

  // presets
  const presetSel = h('select', { class: 'midi-select', 'aria-label': 'Preset' });
  const presetDesc = h('small', { class: 'preset-desc' });
  const applyPreset = h('button', { type: 'button', class: 'small', onclick: () => {
    const p = session.presets.find((x) => x.name === presetSel.value);
    if (p) session.applyPreset(p);
  } }, 'Apply');
  const savePreset = h('button', { type: 'button', class: 'small', onclick: () => {
    const n = prompt('Preset name', 'My preset');
    if (n && n.trim()) session.saveUserPreset(n.trim());
  } }, 'Save as preset…');
  const deletePreset = h('button', { type: 'button', class: 'small', onclick: () => session.deleteUserPreset(presetSel.value) }, 'Delete');
  presetSel.addEventListener('change', () => refreshPresetDesc());
  const refreshPresetDesc = () => {
    const p = session.presets.find((x) => x.name === presetSel.value);
    presetDesc.textContent = p ? p.description : '';
    deletePreset.hidden = !p || p.factory;
  };

  // recording
  limitSelect = h(
    'select',
    { class: 'midi-select', 'aria-label': 'Recording length' },
    ...[[0, 'Until stopped'], [60, '1 min'], [300, '5 min'], [600, '10 min'], [1800, '30 min'], [3600, '60 min'], [7200, '120 min']].map(([v, l]) => h('option', { value: String(v) }, String(l))),
  );
  const fmt = segmented(
    [
      { value: 'pcm16', label: '16-bit' },
      { value: 'pcm24', label: '24-bit' },
      { value: 'float32', label: '32-bit float' },
    ],
    session.format,
    (v: WavFormat) => {
      session.format = v;
      fmt.set(v);
    },
    'WAV format',
  );
  const rec = h('button', { type: 'button', class: 'small rec-btn', onclick: () => (session.recording.active ? void session.stopRecording() : void session.startRecording({ maxSeconds: recordLimit(), fullMorph: false })) });
  const recFull = h('button', { type: 'button', class: 'small', title: 'Restart the morph and record it from start to end, then stop automatically', onclick: () => void session.startRecording({ maxSeconds: null, fullMorph: true }) }, 'Record full morph');
  const render = h('button', { type: 'button', class: 'small', title: 'Render the whole morph to WAV faster than real time', onclick: () => void session.renderOffline() }, 'Render full morph (offline)');
  const cancel = h('button', { type: 'button', class: 'small', onclick: () => session.cancelRender() }, 'Cancel');
  const progress = h('progress', { max: '1', value: '0', class: 'render-progress' });
  const recStatus = h('small', { class: 'rec-status' });
  const message = h('div', { class: 'session-msg', role: 'status' });

  const root = h(
    'section',
    { class: 'panel session' },
    h('div', { class: 'param-group' }, h('h3', {}, 'Project'), h('div', { class: 'button-row' }, name, open, save), openInput, h('h3', { class: 'gap' }, 'Presets'), h('div', { class: 'button-row' }, presetSel, applyPreset, deletePreset), presetDesc, savePreset),
    h(
      'div',
      { class: 'param-group' },
      h('h3', {}, 'Record & export'),
      h('div', { class: 'button-row' }, rec, h('small', {}, 'Length'), limitSelect),
      h('div', { class: 'button-row' }, recFull, render, cancel),
      progress,
      h('div', { class: 'button-row' }, h('small', {}, 'Format'), fmt.root),
      recStatus,
      h('small', { class: 'muted' }, canStreamToDisk() ? 'Recordings stream straight to a file you choose, so long takes do not use memory.' : 'Recordings are kept in memory and downloaded when they stop.'),
    ),
    message,
  );

  let presetKey = '';
  return {
    root,
    refresh() {
      const key = session.presets.map((p) => p.name).join('|');
      if (key !== presetKey) {
        presetKey = key;
        const cur = presetSel.value;
        presetSel.replaceChildren(
          h('optgroup', { label: 'Factory' }, ...session.presets.filter((p) => p.factory).map((p) => h('option', { value: p.name }, p.name))),
          ...(session.userPresets.length ? [h('optgroup', { label: 'Yours' }, ...session.userPresets.map((p) => h('option', { value: p.name }, p.name)))] : []),
        );
        if (cur && session.presets.some((p) => p.name === cur)) presetSel.value = cur;
        refreshPresetDesc();
      }
      if (document.activeElement !== name) name.value = session.projectName;
      const r = session.recording;
      rec.textContent = r.active ? `■ Stop recording (${formatTime(r.seconds)})` : '● Record';
      rec.classList.toggle('on', r.active);
      recFull.disabled = r.active || session.render.active;
      render.disabled = r.active || session.render.active;
      cancel.hidden = !session.render.active;
      progress.hidden = !session.render.active;
      progress.value = session.render.progress;
      recStatus.textContent = r.active
        ? `Recording${r.mode === 'fullMorph' ? ' the full morph' : ''} to ${r.destination}`
        : session.render.active
          ? `Rendering… ${Math.round(session.render.progress * 100)}%`
          : session.render.message ?? r.message ?? '';
      message.textContent = session.message ?? '';
      void app;
    },
  };
}
