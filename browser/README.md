# SlowMorph

An experimental sample-morphing instrument for long-form ambient, drone and
electroacoustic music. Load 2–8 short samples, set a morph duration from 5
seconds to 2 hours, and the instrument transforms continuously from one
sample to the next.

Runs locally in Chrome or Edge on macOS. No accounts, no cloud services, and
your samples never leave the computer.

## Run it

Requires Node.js 18 or newer (`brew install node`).

```sh
cd browser
npm install
npm run dev        # opens on http://localhost:5173
```

For a fixed build that works offline: `npm run build`, then
`npm run preview` and open http://localhost:4173. (Browsers only allow
AudioWorklets from a web server, so opening `dist/index.html` directly from
Finder will not work.)

## Status

| Phase | Scope | State |
|---|---|---|
| 1 | Sample slots, seamless looping, timeline, duration, crossfade engine, transport | **Done and tested** |
| 2 | Granular engine | **Done and tested** |
| 3 | Spectral engine | **Done and tested** |
| 4 | Character/Hybrid, freeze per engine, drift, randomise, MIDI, effects | **Done and tested** |
| 5 | Projects, presets, WAV recording, offline render | **Done and tested** |
| 6 | M-VAVE FM1 feasibility study (no flashing) | **Done**: [report](https://claude.ai/artifact/GjzgHg22UZGA7Fu5XuVf7k). Crossfade and granular fit; spectral only reduced; nothing was flashed |

## Playing it (Phase 1)

* **Load samples**: drop a file on a slot or click **Load**. WAV, AIFF, MP3,
  AAC/M4A, FLAC and OGG. Files at any sample rate are converted to the audio
  device's rate. Up to 60 s per sample (1–15 s recommended).
* **Waveform markers**: grey handles at the bottom trim the sample; white
  handles at the top set the loop. Playback starts at the trim start and
  then loops between the loop markers with a seamless crossfade.
* **Per slot**: gain, Norm (peak to −1 dBFS), Rev (reverse), M (mute), S (solo).
* **Timeline**: drag the lettered nodes to change when each sample arrives;
  **Equal spacing** resets them. The lower lane shows each transition window:
  drag its edges to make a transition shorter or longer, click it to edit its
  curve below. Click or drag anywhere else to scrub.
* **Transport**: Restart, Play/Pause, Stop, Reverse, Freeze (holds the morph
  position while the sound keeps going), and Once / Loop / Ping-pong.
* **Keys**: Space play/pause, Esc stop, Home restart, R reverse, F freeze,
  ←/→ nudge the position when the timeline is focused (Shift for bigger steps).

## Granular mode (Phase 2)

Choose **Granular** under Morph engine. The timeline now sets the
*probability* that each grain comes from each sample, so at the half-way
point between A and B roughly half the grains are A and half are B.

* **Grain size** 10–500 ms, **Overlap** (grains sounding at once),
  **Density** (sparse ↔ continuous), **Position random** (how far grains
  wander from the scan head), **Pitch variation** (± cents per grain),
  **Transpose**, **Playback speed** (how fast each sample is scanned; 0 holds
  still, negative goes backwards), **Stereo spread**, **Onset jitter**
  (regular pulse ↔ asynchronous cloud), **Grain envelope** and **Seed**
  (same seed and settings give the same texture).
* Double-click any slider to reset it.

## Spectral mode (Phase 3)

Choose **Spectral**. The samples are analysed with an FFT and turned into a
single in-between spectrum, so the sound changes timbre instead of two
sounds overlapping.

* **FFT size**: small = sharper in time, large = smoother, more detailed
  tones (8k is the heaviest). **Quality**: Standard (4×) or High (8×).
* **Interpolation**: *linear* blends magnitudes; *hybrid* morphs the
  spectral envelope and the partials separately, which makes stranger,
  genuinely hybrid timbres.
* **Timbre lead**: with hybrid interpolation, move the next sample's
  "colour" ahead of (+) or behind (−) its pitches.
* **Spectral blur** (smear over time), **Smoothing** (over frequency),
  **Phase random** (diffuse/whispering), **Scan speed**, **Stereo width**.
* **Freeze** holds the current hybrid spectrum as a sustained sound.

See ARCHITECTURE.md for exactly how it works and its limitations.

## Performing (Phase 4)

* **Original Character ←→ Strange Hybrid**: the large slider under the
  engine buttons. 0 % is your settings, untouched; higher values push each
  engine toward stranger results (the line under the slider says what it
  does in the current engine). Double-click to reset.
* **Freeze** (❄ or F) holds the current moment: loops keep going in
  Crossfade, the grain cloud keeps going in Granular, the current hybrid
  spectrum sustains in Spectral.
* **Morph drift** slowly wanders the sound around the current position
  (dashed playhead).
* **Randomise** granular, spectral, curves or sound-shaping settings;
  **Undo** steps back.
* **MIDI**: click Enable MIDI, choose a parameter, click Learn and move a
  knob or hit a pad. Remove a mapping with ✕.
* **Sound shaping**: high-pass, low-pass, resonance, width, reverb and delay,
  with one bypass switch.

## Projects, presets and recording (Phase 5)

* **Projects** (⌘S / ⌘O, or Save… / Open…): a `.slowmorph` file holds every
  setting *and* the sample audio, so it opens on another Mac with nothing
  missing. Typical size is a few MB.
* **Presets**: six factory presets (Slow Dissolve, Granular Cloud, Spectral
  Ghost, Endless Drone, Unstable Memory, Field Recording Transformation).
  **Apply** changes the engine, morph settings, drift and sound shaping but
  keeps your samples. **Save as preset…** stores your own in this browser.
* **Record** (● in the header or the Record & Export panel) records exactly
  what you hear. Chrome/Edge ask where to save first and then stream to that
  file, so a two-hour take does not fill memory. Pick a length limit, or
  **Record full morph** to restart and record start to finish (plus a
  4-second tail) and stop by itself.
* **Render full morph (offline)** computes the whole morph without playing
  it, typically 30–60× faster than real time, with a short tail and a
  0.1-second fade at the end. Formats: 16-bit, 24-bit or 32-bit float WAV.

## Development

```sh
npm test              # unit + audio-rendering tests (Vitest)
npm run test:browser  # end-to-end test in headless Chromium (needs Playwright)
npm run typecheck
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the design of every engine and
its limitations.

## Licence

GPL-3.0. See [LICENSE](../LICENSE).
