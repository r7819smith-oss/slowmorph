/**
 * End-to-end browser test: builds the app, serves it, drives it in headless
 * Chromium, and analyses the real audio output captured by the recorder
 * tap. Checks loading (WAV at two rates, AIFF, a corrupt file), the morph
 * progression, clicks, clipping, silence, freeze, scrubbing and stop.
 *
 * Usage: node scripts/browser-test.mjs [--screenshot path.png]
 */
import { createRequire } from 'node:module';
import { spawn, execSync } from 'node:child_process';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

// Playwright is resolved with require() so a globally installed copy works
// too. On a Mac: `npm i -D playwright && npx playwright install chromium`.
let chromium;
try {
  ({ chromium } = createRequire(import.meta.url)('playwright'));
} catch {
  console.error('Playwright is not installed. Run: npm i -D playwright && npx playwright install chromium');
  process.exit(2);
}

const shotArg = process.argv.indexOf('--screenshot');
const screenshot = shotArg > 0 ? process.argv[shotArg + 1] : null;

// ---- test signals ----------------------------------------------------------------
function tone(freq, seconds, sr) {
  const n = Math.round(seconds * sr);
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / sr;
    out[i] = 0.4 * (0.75 + 0.25 * Math.sin(2 * Math.PI * 0.7 * t)) * Math.sin(2 * Math.PI * freq * t);
  }
  return out;
}
function wav16(ch, sr) {
  const n = ch.length;
  const b = Buffer.alloc(44 + n * 2);
  b.write('RIFF', 0); b.writeUInt32LE(36 + n * 2, 4); b.write('WAVE', 8); b.write('fmt ', 12);
  b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22); b.writeUInt32LE(sr, 24);
  b.writeUInt32LE(sr * 2, 28); b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34); b.write('data', 36); b.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) b.writeInt16LE(Math.round(Math.max(-1, Math.min(1, ch[i])) * 32767), 44 + i * 2);
  return b;
}
function aiff16(ch, sr) {
  const n = ch.length;
  const ssnd = 8 + n * 2;
  const b = Buffer.alloc(54 + n * 2);
  b.write('FORM', 0); b.writeUInt32BE(b.length - 8, 4); b.write('AIFF', 8);
  b.write('COMM', 12); b.writeUInt32BE(18, 16); b.writeInt16BE(1, 20); b.writeUInt32BE(n, 22); b.writeInt16BE(16, 26);
  const e = Math.floor(Math.log2(sr));
  b.writeUInt16BE(16383 + e, 28); b.writeUInt32BE(Math.floor((sr / 2 ** e) * 2 ** 31), 30); b.writeUInt32BE(0, 34);
  b.write('SSND', 38); b.writeUInt32BE(ssnd, 42); b.writeUInt32BE(0, 46); b.writeUInt32BE(0, 50);
  for (let i = 0; i < n; i++) b.writeInt16BE(Math.round(Math.max(-1, Math.min(1, ch[i])) * 32767), 54 + i * 2);
  return b;
}

const dir = mkdtempSync(join(tmpdir(), 'slowmorph-'));
const files = {
  a: join(dir, 'tone-220-44k.wav'),
  b: join(dir, 'tone-330-22k.aiff'),
  c: join(dir, 'tone-495-48k.wav'),
  bad: join(dir, 'corrupt.wav'),
};
writeFileSync(files.a, wav16(tone(220, 1.37, 44100), 44100));
writeFileSync(files.b, aiff16(tone(330, 1.11, 22050), 22050));
writeFileSync(files.c, wav16(tone(495, 1.53, 48000), 48000));
writeFileSync(files.bad, Buffer.concat([Buffer.from('RIFF\x00\x00\x00\x00WAVEfmt '), Buffer.alloc(200, 7)]));

// ---- serve -------------------------------------------------------------------------
execSync('npx vite build', { stdio: 'ignore' });
const server = spawn('npx', ['vite', 'preview', '--port', '4173', '--strictPort'], { stdio: 'ignore' });
await new Promise((r) => setTimeout(r, 1500));

const results = [];
const check = (name, ok, detail = '') => {
  results.push({ name, ok, detail });
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? `  (${detail})` : ''}`);
};

const browser = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 1100 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  await page.goto('http://localhost:4173/');
  await page.waitForFunction(() => window.slowmorph?.engine.recorder);
  const sr = await page.evaluate(() => window.slowmorph.engine.sampleRate);
  check('audio engine starts', true, `context ${sr} Hz`);

  const load = async (slot, file) => {
    await page.setInputFiles(`.slot[data-slot="${slot}"] input[type=file]`, file);
    await page.waitForFunction((s) => {
      const st = window.slowmorph.state.slots[s];
      return !st.loading && (st.audio || st.error);
    }, slot);
  };
  await load(0, files.a);
  await load(1, files.b);
  await load(2, files.c);
  const loaded = await page.evaluate(() => window.slowmorph.state.slots.map((s) => ({ ok: !!s.audio, err: s.error, sr: s.audio?.sampleRate, orig: s.audio?.originalSampleRate, frames: s.audio?.frames })));
  check('WAV 44.1 kHz loads', loaded[0].ok && loaded[0].sr === sr, JSON.stringify(loaded[0]));
  check('AIFF 22.05 kHz loads and is resampled', loaded[1].ok && loaded[1].orig === 22050 && Math.abs(loaded[1].frames - 1.11 * sr) < sr * 0.002, JSON.stringify(loaded[1]));
  check('WAV 48 kHz loads', loaded[2].ok, JSON.stringify(loaded[2]));

  await page.click('button.slot.add');
  await load(3, files.bad);
  const badMsg = await page.textContent('.slot[data-slot="3"] .slot-msg');
  check('corrupt file shows a clear error', /could not be decoded/.test(badMsg ?? ''), badMsg ?? '');
  await page.click('.slot[data-slot="3"] .slot-head button[title="Remove this slot"]');

  // 12-second morph
  await page.fill('.duration-input', '12');
  await page.press('.duration-input', 'Enter');
  const dur = await page.evaluate(() => window.slowmorph.state.durationSec);
  check('manual duration entry', dur === 12, `${dur}s`);

  // record the whole morph from play to end
  await page.evaluate(() => window.slowmorph.engine.recorder.start());
  await page.click('button.tbtn.play');
  const samples = [];
  for (let i = 0; i < 26; i++) {
    await page.waitForTimeout(500);
    samples.push(await page.evaluate(() => ({ p: window.slowmorph.live.position, f: window.slowmorph.live.fractions.slice(0, 3) })));
  }
  const stats = await page.evaluate(async () => {
    const [L, R] = await window.slowmorph.engine.recorder.stop();
    const sr = window.slowmorph.engine.sampleRate;
    // find first non-silent sample (start of play)
    let start = 0;
    while (start < L.length && Math.abs(L[start]) < 1e-4) start++;
    const from = start + Math.round(0.05 * sr);
    const to = Math.min(L.length, start + Math.round(12.5 * sr));
    let maxD2 = 0, peak = 0, nan = false, minRms = Infinity;
    for (let i = from; i < to; i++) {
      for (const x of [L, R]) {
        if (!Number.isFinite(x[i])) nan = true;
        const d = Math.abs(x[i] - 2 * x[i - 1] + x[i - 2]);
        if (d > maxD2) maxD2 = d;
        if (Math.abs(x[i]) > peak) peak = Math.abs(x[i]);
      }
    }
    const w = Math.round(0.02 * sr);
    for (let a = from; a + w < to; a += w) {
      let s = 0;
      for (let i = a; i < a + w; i++) s += L[i] * L[i];
      minRms = Math.min(minRms, Math.sqrt(s / w));
    }
    const zc = (t) => {
      const a = start + Math.round(t * sr);
      let z = 0;
      for (let i = a + 1; i < a + 0.5 * sr; i++) if (L[i - 1] <= 0 && L[i] > 0) z++;
      return z / 0.5;
    };
    return { frames: L.length, start, maxD2, peak, nan, minRms, f0: zc(0.2), f1: zc(5.8), f2: zc(11.3) };
  });
  const ref = 0.4 * Math.pow((2 * Math.PI * 495) / sr, 2) * 10 ** (-3 / 20);
  check('recorded audio is non-silent and finite', !stats.nan && stats.minRms > 0.02, `min 20ms RMS ${stats.minRms.toFixed(3)}`);
  check('no clicks during a full 3-sample morph', stats.maxD2 < ref * 2.5, `max 2nd diff ${stats.maxD2.toExponential(2)} vs smooth ref ${ref.toExponential(2)}`);
  check('no clipping', stats.peak < 0.9, `peak ${stats.peak.toFixed(3)}`);
  check('starts on A (220 Hz)', Math.abs(stats.f0 - 220) < 10, `${stats.f0} Hz`);
  check('middle is B (330 Hz)', Math.abs(stats.f1 - 330) < 10, `${stats.f1} Hz`);
  check('ends on C (495 Hz)', Math.abs(stats.f2 - 495) < 10, `${stats.f2} Hz`);
  const mono = samples.every((s, i) => i === 0 || s.p >= samples[i - 1].p - 1e-9);
  check('morph position advances monotonically to the end', mono && samples.at(-1).p === 1, `final ${samples.at(-1).p}`);
  const endState = await page.evaluate(() => ({ t: window.slowmorph.live.transport, ended: window.slowmorph.live.ended }));
  check('once mode holds the last sample at the end (still sounding)', endState.t === 'playing' && endState.ended, JSON.stringify(endState));

  // scrub by clicking the timeline
  const box = await page.locator('.timeline-canvas').boundingBox();
  await page.mouse.click(box.x + 18 + (box.width - 36) * 0.25, box.y + box.height / 2);
  await page.waitForTimeout(300);
  const scrubbed = await page.evaluate(() => window.slowmorph.live);
  check('click on timeline scrubs the position', Math.abs(scrubbed.position - 0.25) < 0.05, `pos ${scrubbed.position.toFixed(3)}`);

  // freeze holds position
  await page.keyboard.press('f');
  await page.waitForTimeout(200); // let the status catch up with the hold
  const p0 = await page.evaluate(() => window.slowmorph.live.position);
  await page.waitForTimeout(1200);
  const p1 = await page.evaluate(() => window.slowmorph.live.position);
  check('freeze holds the morph position', Math.abs(p1 - p0) < 1e-6, `${p0.toFixed(4)} → ${p1.toFixed(4)}`);
  await page.keyboard.press('f');
  await page.waitForTimeout(600);
  const p2 = await page.evaluate(() => window.slowmorph.live.position);
  check('unfreeze continues from the frozen position', p2 > p1 && p2 - p1 < 0.1, `${p1.toFixed(4)} → ${p2.toFixed(4)}`);

  // reverse
  await page.keyboard.press('r');
  await page.waitForTimeout(600);
  const p3 = await page.evaluate(() => window.slowmorph.live.position);
  check('reverse runs the morph backwards', p3 < p2, `${p2.toFixed(4)} → ${p3.toFixed(4)}`);
  await page.keyboard.press('r');

  // drag node B to 0.8
  const nodeY = box.y + 20 + 13;
  await page.mouse.move(box.x + 18 + (box.width - 36) * 0.5, nodeY);
  await page.mouse.down();
  await page.mouse.move(box.x + 18 + (box.width - 36) * 0.8, nodeY, { steps: 8 });
  await page.mouse.up();
  const pos = await page.evaluate(() => {
    const s = window.slowmorph.state;
    return s.slots.slice(0, 3).map((x) => s.timeline.positions[x.id]);
  });
  check('dragging a node moves it on the timeline', Math.abs(pos[1] - 0.8) < 0.02, JSON.stringify(pos.map((p) => p?.toFixed(3))));

  // ---- granular engine -----------------------------------------------------------
  await page.click('.modes button[data-value="granular"]');
  await page.click('.duration-row button[data-value="30"]');
  await page.evaluate(() => window.slowmorph.seek(0));
  await page.waitForTimeout(300);
  await page.evaluate(() => window.slowmorph.engine.recorder.start());
  await page.waitForTimeout(4000);
  const gstats = await page.evaluate(async () => {
    const [L] = await window.slowmorph.engine.recorder.stop();
    const sr = window.slowmorph.engine.sampleRate;
    let peak = 0, nan = false, minRms = Infinity;
    const w = Math.round(0.05 * sr);
    for (let a = Math.round(0.3 * sr); a + w < L.length; a += w) {
      let s = 0;
      for (let i = a; i < a + w; i++) {
        if (!Number.isFinite(L[i])) nan = true;
        s += L[i] * L[i];
        peak = Math.max(peak, Math.abs(L[i]));
      }
      minRms = Math.min(minRms, Math.sqrt(s / w));
    }
    return { peak, nan, minRms, grains: window.slowmorph.live.grains, engine: window.slowmorph.state.engine };
  });
  check('granular engine selected and producing grains', gstats.engine === 'granular' && gstats.grains > 0, `${gstats.grains} grains sounding`);
  check('granular output is continuous (no gaps) and finite', !gstats.nan && gstats.minRms > 0.01, `min 50ms RMS ${gstats.minRms.toFixed(3)}`);
  check('granular output does not clip', gstats.peak < 0.9, `peak ${gstats.peak.toFixed(3)}`);
  await page.locator('.granular-panel input[aria-label="Grain size"]').fill('0.2');
  const gsize = await page.evaluate(() => window.slowmorph.state.granular.sizeMs);
  check('granular controls update the engine', gsize < 40, `size ${gsize} ms`);

  // ---- spectral engine -----------------------------------------------------------
  await page.click('.modes button[data-value="spectral"]');
  await page.evaluate(() => window.slowmorph.seek(0.4));
  await page.waitForTimeout(500);
  await page.evaluate(() => window.slowmorph.engine.recorder.start());
  await page.waitForTimeout(1500);
  await page.click('.spectral-panel button[data-value="4096"]');
  await page.waitForTimeout(1500);
  const sstats = await page.evaluate(async () => {
    const [L] = await window.slowmorph.engine.recorder.stop();
    const sr = window.slowmorph.engine.sampleRate;
    let peak = 0, nan = false, minRms = Infinity, maxD2 = 0;
    const w = Math.round(0.05 * sr);
    for (let a = Math.round(0.2 * sr); a + w < L.length; a += w) {
      let s = 0;
      for (let i = a; i < a + w; i++) {
        if (!Number.isFinite(L[i])) nan = true;
        s += L[i] * L[i];
        peak = Math.max(peak, Math.abs(L[i]));
        maxD2 = Math.max(maxD2, Math.abs(L[i] - 2 * L[i - 1] + L[i - 2]));
      }
      minRms = Math.min(minRms, Math.sqrt(s / w));
    }
    return { peak, nan, minRms, maxD2, engine: window.slowmorph.state.engine, fft: window.slowmorph.state.spectral.fftSize };
  });
  check('spectral engine selected', sstats.engine === 'spectral' && sstats.fft === 4096, JSON.stringify({ e: sstats.engine, fft: sstats.fft }));
  check('spectral output is continuous and finite (incl. FFT size change)', !sstats.nan && sstats.minRms > 0.01, `min 50ms RMS ${sstats.minRms.toFixed(3)}`);
  check('spectral output does not clip or click', sstats.peak < 0.9 && sstats.maxD2 < ref * 4, `peak ${sstats.peak.toFixed(3)}, max 2nd diff ${sstats.maxD2.toExponential(2)}`);

  // set up a nicer view for the screenshot: 10 minute morph, a third in
  await page.click('.duration-row button[data-value="600"]');
  await page.click('button.small:has-text("Equal spacing")');
  await page.evaluate(() => window.slowmorph.seek(0.31));
  await page.waitForTimeout(400);
  if (screenshot) await page.screenshot({ path: screenshot, fullPage: true });

  // stop → silence and position reset
  await page.keyboard.press('Escape');
  await page.waitForTimeout(400);
  const stopped = await page.evaluate(() => ({ t: window.slowmorph.live.transport, p: window.slowmorph.live.position, pk: window.slowmorph.live.peakL }));
  check('stop silences output and resets position', stopped.t === 'stopped' && stopped.p === 0 && stopped.pk === 0, JSON.stringify(stopped));

  check('no page errors', errors.length === 0, errors.join(' | '));
} finally {
  await browser.close();
  server.kill();
}
const failed = results.filter((r) => !r.ok);
console.log(`\n${results.length - failed.length}/${results.length} browser checks passed`);
process.exit(failed.length ? 1 : 0);
