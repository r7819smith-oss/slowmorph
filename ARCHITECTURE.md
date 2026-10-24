# SlowMorph architecture

SlowMorph is a local browser instrument: TypeScript, the Web Audio API and an
AudioWorklet, built with Vite. It has no runtime dependencies, makes no
network requests, and samples never leave the computer.

## Threads and modules

```
main thread (UI)                         audio thread (AudioWorklet)
─────────────────                        ───────────────────────────
src/main.ts      layout, keys, rAF loop  src/audio/worklet.ts  thin wrapper
src/ui/*         canvas timeline, slots          │
src/app.ts       actions + project state ──msg──▶ src/dsp/core.ts  MorphCore
src/state/*      ProjectState (seconds)  ◀─status─  ├ clock.ts      morph clock
src/audio/decode.ts  WAV/MP3/… + AIFF               ├ timeline.ts   fractions
src/audio/audio-engine.ts  context, nodes           ├ slot.ts       declick/swap
                                                    ├ loop-player.ts seamless loops
                                                    ├ engines/*     crossfade (A),
                                                    │               granular (B),
                                                    │               spectral (C)
                                                    └ limiter.ts    safety ceiling
```

* **All DSP lives in `src/dsp/`** and is plain TypeScript with no Web Audio
  dependency. The worklet wraps it for real time; the unit tests drive it
  directly; the offline renderer (Phase 5) will run the same code faster
  than real time. Each engine implements `MorphEngine` and can be tested
  on its own.
* **The morph clock runs on the audio thread**, so the morph advances
  sample-accurately and keeps going when the UI is busy or the tab is in the
  background. The UI receives ~30 status messages a second (position, mix
  fractions, meters) and draws at display rate from that.
* **No allocation in `process()`.** Buffers, fade tables and voice pools are
  allocated when samples or settings arrive, never per block.
* **Everything that can click is smoothed:** morph fractions (30 ms, longer
  glides after a seek or loop wrap), slot gain/mute/solo (20 ms), transport
  start/stop/pause (30 ms fades), sample replacement and loop-marker changes
  (8 ms declick fade, swap at zero, fade back in), and loop seams (equal-power
  crossfade, default 60 ms).
* **Safety limiter.** A peak limiter at −1 dBFS sits at the very end. It only
  engages on overs and the header shows a LIM light when it does, so it
  cannot quietly hide level problems in an engine.

## Timeline model

The morph is a normalised axis 0..1 spread over the chosen duration (5 s to
120 min). Each loaded sample owns a node; nodes stay in slot order. Between
adjacent nodes, a **transition window** (start/end as fractions of the
segment) says where the change happens, and a **curve** (linear, S-curve,
ease in, ease out, with a strength control) shapes it. Outside the window the
nearer sample plays alone.

`computeFractions(model, position)` returns each slot's mix fraction. They
always sum to 1 and are continuous in position (tested). Engines decide what
a fraction means: amplitude in Crossfade, grain-selection probability in
Granular, interpolation weight in Spectral.

Play modes: **once** (holds the final sample, still sounding, at the end),
**loop** (wraps with a ~3 s glide back to the first sample, not a jump) and
**ping-pong**. Reverse flips direction; freeze holds the position while audio
continues.

## Mode A: Crossfade (Phase 1, done)

Every slot loops independently in a `LoopPlayer` (silent slots keep
advancing so loops stay independent). Fractions become gains through the
chosen law: **equal-power** (`√f`, constant power for unrelated sounds, the
default) or **linear** (constant amplitude for near-identical sounds). Loop
seams use an equal-power crossfade between the loop tail and the loop head,
so loops can start at frame 0. Reverse playback reads through a mirrored
index (no reversed copy).

## Mode B: Granular (Phase 2, planned design)

* A fixed pool of grain voices (no allocation while playing), scheduled
  sample-accurately inside the worklet.
* Each new grain picks its **source by weighted random choice** using the mix
  fractions as probabilities, from a seeded PRNG (the Random Seed control
  makes a texture repeatable).
* Each source has a **scan head** moving through its loop region at the
  Playback Speed; grains start at the scan head plus Position Randomness, so
  short samples become long, non-repeating textures.
* Grain size 10–500 ms, pitch variation (cents), stereo spread (per-grain
  equal-power pan), selectable window (Hann, Tukey, Gaussian, trapezoid).
  Reads use cubic interpolation so pitch shifts stay clean.
* **Overlap** sets how many grains overlap on average (onset interval = size
  ÷ overlap); **density** is the probability each scheduled grain actually
  sounds, from sparse clouds to a continuous wash.
* Output is normalised by the expected grain overlap energy so changing
  size, overlap or density does not change loudness much.
* Freeze stops the scan heads; grains keep spawning, so the cloud continues.

## Mode C: Spectral (Phase 3, planned design)

A phase-vocoder style STFT engine, running in the worklet:

* Each active source (the two or three with the largest fractions) is
  analysed in real time at its own scan position: Hann window, FFT size
  512–8192, 4× overlap. Two analysis frames one hop apart give each bin's
  **instantaneous frequency**.
* **Magnitude interpolation in two layers.** Each spectrum is split into a
  smooth **spectral envelope** (cepstral smoothing) and a **fine structure**
  (magnitude ÷ envelope). Envelopes and fine structures are interpolated
  separately, in the log domain, so formants/resonances morph independently
  of partials. This is a timbre transformation, not a mix: a 50 % point is
  one spectrum, not two layered sounds.
* **Phase.** Mixing source phases cancels. Instead the engine keeps one
  running phase per output bin and advances it by an interpolated
  instantaneous frequency (classic phase-vocoder accumulation), so there is
  no cancellation and no frame-to-frame discontinuity. Optional phase
  randomisation adds diffuse smearing at the hybrid end.
* Spectral blur (temporal smoothing of magnitudes) and spectral smoothing
  (across frequency), overlap-add resynthesis with output normalisation.
* Freeze holds the current hybrid frame and keeps advancing phases, with a
  small phase jitter to avoid a static buzz.
* Cost at FFT 2048: about 5 FFTs per 512-sample hop, well within a worklet
  budget in plain JS. WebAssembly is kept as an option if profiling shows a
  need at the largest FFT sizes.
* Known limitation to be documented with the implementation: transients are
  smeared by any STFT method, more so at large FFT sizes.

## Testing

* `npm test`: Vitest unit and audio-rendering tests: curves, timeline
  interpolation, clock modes, duration parsing, loop seams, crossfade
  rendering (clicks via second-difference analysis, level stability via RMS
  windows, clipping, silence gaps, pause/stop/seek/reverse, sample swap,
  mute/solo, loop-mode wrap).
* `npm run test:browser`: builds the app, drives it in headless Chromium,
  loads WAV (44.1/48 kHz), AIFF (22.05 kHz, resampled) and a corrupt file,
  records the real output through the recorder tap and analyses it.
