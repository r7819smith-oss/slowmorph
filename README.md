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
| 3 | Spectral engine | Next |
| 4 | Character/Hybrid, freeze per engine, drift, randomise, MIDI, effects | Planned |
| 5 | Projects, presets, WAV recording, offline render | Planned (recorder tap already in place) |
| 6 | M-VAVE FM1 feasibility study (no flashing) | Planned |

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

## Development

```sh
npm test              # unit + audio-rendering tests (Vitest)
npm run test:browser  # end-to-end test in headless Chromium (needs Playwright)
npm run typecheck
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the design, including the planned
granular and spectral algorithms.
