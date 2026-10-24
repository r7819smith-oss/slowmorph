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
  directly; the offline renderer (Phase 5) runs the same code faster
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

## Mode B: Granular (Phase 2, done)

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
* Grains are always placed so they do not cross the loop seam when they
  fit inside the loop; muted/soloed-out slots are removed from the
  selection probabilities (the remaining samples fill in).
* Switching engines while playing crossfades the two engines over 150 ms.

**Limitation (loudness):** the normaliser estimates how coherent the grains
are from position and pitch randomness. Across the tested settings the
level stays within ±4.5 dB of Crossfade mode. Pure tones and very hot
material can still peak higher, because grains of the same tone with random
phases occasionally line up; the safety limiter catches those peaks and the
LIM light shows it. If this proves distracting in practice, a slow automatic
gain stage on the granular bus is the next step.

## Mode C: Spectral (Phase 3, done)

A phase-vocoder style STFT engine (`src/dsp/engines/spectral.ts`), running
in the worklet in plain TypeScript:

1. **Analysis.** Each audible sample is read at its own scan head (moving
   through the loop at Scan Speed) as a seamless loop (same equal-power seam
   as the crossfade engine), mixed to mono, Hann-windowed and transformed
   (FFT 512–8192, overlap 4× or 8×). Two frames one hop apart give each
   bin's magnitude and **instantaneous frequency**. At speed 1 the second
   frame of one hop is reused as the first frame of the next.
2. **Envelope / fine structure.** Each log-magnitude spectrum is smoothed
   twice over a band of ±20 % of frequency (at least ±120 Hz) to give a
   **spectral envelope**; the remainder is the **fine structure**
   (partials, noise texture).
3. **Interpolation** (weights from the timeline):
   * *Linear layer*: Σ wᵢ·Mᵢ, transparent magnitude interpolation.
   * *Hybrid layer*: exp(Σ wᵢᴱ·log Envᵢ + Σ wᵢᶠ·log Fineᵢ), geometric
     interpolation of envelope and fine structure with separate weights.
     **Timbre lead** bends the envelope weights ahead of (or behind) the
     fine-structure weights, so one sound's resonances can be imposed on the
     other's partials. Tested: half-way between a 300 Hz and a 520 Hz
     harmonic tone, lead +1 keeps >80 % of the energy on the 300 Hz series,
     lead −1 on the 520 Hz series.
   * The **Interpolation** control blends the two layers.
4. **Loudness.** The frame's energy is normalised to Σ wᵢ·(energy of
   source i), smoothed over frames and limited to ±12 dB. Tested: morphing
   dark noise into bright noise stays within ±2 dB.
5. **Smoothing** across frequency, **blur** across time (one-pole on
   magnitudes).
6. **Phase.** Source phases are never summed. Each output bin keeps a
   running phase advanced by the magnitude-weighted instantaneous
   frequency; with **identity phase locking**, bins around each spectral
   peak take their phase offsets from the dominant source, which keeps
   partials coherent. Tested: morphing a sound into its own phase-inverted
   copy, a linear crossfade cancels to silence at the midpoint; the
   spectral engine holds the level within 1.5 dB.
7. **Phase random** adds per-frame random phase (diffuse/whispering).
   **Stereo width** resynthesises L and R with opposite, fixed per-bin
   phase offsets above 200 Hz.
8. Inverse FFT, Hann synthesis window, overlap-add.

**Freeze** stops analysis entirely and keeps resynthesising the last hybrid
spectrum with advancing phases: a steady, sustained version of that moment
(tested: spectra 3 s apart correlate > 0.95, level within 1 dB).

**Real-time safety.** A frame's work is split into stages (one FFT each)
spread over the render blocks of the preceding hop, so no 128-frame block
carries a whole frame. Changing FFT size while playing fades out over 20 ms,
reallocates outside the audio path, and fades back in. Measured in Node
(V8, same engine as Chrome) with 3 sources: 2048/4× ≈ 6 %, 4096/8× ≈ 15 %,
8192/8× ≈ 16 % of real time on one core; worst single block at 8192 ≈ 2.3 ms
of a 2.67 ms budget, so 8192 is labelled as the heavy setting.
WebAssembly was not needed.

**Limitations (honest list):**
* Partials are interpolated **in place**: a partial of A fades while a
  partial of B grows; it does not glide from A's frequency to B's. True
  frequency morphing needs sinusoidal tracking (partial matching), a
  possible later addition.
* The spectral envelope is interpolated by level, not warped along the
  frequency axis, so formants cross-fade rather than slide.
* Like any STFT method it smears transients, more at large FFT sizes; the
  engine adds latency of one window plus one hop (≈ 53 ms at 2048/4×).
* Stereo sources are analysed as mono; width is recreated by decorrelation.

## Phase 4: performance controls (done)

**Original Character ←→ Strange Hybrid** (`src/dsp/character.ts`). At 0 %
every engine uses exactly the user's settings; raising it applies a fixed,
documented set of changes on top, smoothed over 80 ms:

| Engine | What the control changes |
|---|---|
| Crossfade | per-slot low-pass slowly wandering from 20 kHz down to 1.5 kHz at 100 %, slow amplitude shimmer up to ±2.5 dB. Still a pure amplitude crossfade: timeline gains are untouched. |
| Granular | grain size × (1 − 0.6c), position randomness + 0.8c·(1 − base), pitch variation + 400c² cents, onset jitter + c·(1 − base), timeline smear 0.2c (grains from neighbouring samples interleave sooner). |
| Spectral | interpolation toward hybrid + c·(1 − base), blur + 0.6c·(1 − base), smoothing + 0.3c, phase randomness + 0.45c², inharmonic warp + 0.6c², timeline smear 0.15c. |

No new uncontrolled randomness: the engines' own seeded generators supply
it, and each engine's normalisation holds the level (tested: sweeping the
control 0→100 % moves the level less than 3.5 dB in every engine).

**Freeze** holds the morph clock in every engine. Crossfade keeps the loops
running at the frozen mix; Granular stops its scan heads but keeps spawning
grains (a continuous cloud); Spectral stops analysing and sustains the last
hybrid spectrum with advancing phases. Resuming continues from the frozen
position.

**Drift** moves the *heard* position around the clock position with a
seeded smooth random curve (amount, destinations per minute, smoothness)
followed by a 0.5 s glide; turning it off glides back. The timeline shows it
as a dashed second playhead.

**Randomise** draws from musical ranges per group (granular, spectral,
transition curves, sound shaping) and keeps an undo stack of 30 steps.

**MIDI** (`src/control/midi.ts`): Web MIDI with MIDI-learn. Pick a parameter,
press Learn, move a knob (CC) or hit a pad (note; toggles freeze/playback).
Mappings are saved with the project. Mappable: morph position, character,
grain size, grain density, spectral blur, low-pass cutoff, drift amount,
reverb send, output level, freeze, play/pause.

**Sound shaping** (`src/dsp/effects.ts`), after the engines and before the
master/limiter: TPT state-variable high-pass and low-pass (resonance),
mid/side width, ping-pong delay with damped feedback and glided delay time
(no clicks when changing it), Freeverb-style reverb. One bypass switch with
a 20 ms crossfade; the effects start bypassed in the core so every engine
test runs dry.

## Phase 5: projects, presets, recording, offline render (done)

**Project file** (`src/state/serialize.ts`): `"SLOWMORPH\0"` magic, u32
format version (1), u32 JSON length, UTF-8 JSON of every setting, padding
to 4 bytes, then each slot's audio as raw float32 at the project's sample
rate. On open every field is read through a typed `pick` that falls back to
the default when a field is missing or the wrong type, so older or
hand-edited files still load; truncated or corrupt files raise a clear
error and leave the current project untouched. Audio saved at another
sample rate is resampled on open.

**Presets** (`src/state/presets.ts`) are settings only (engine, duration,
play mode, engine parameters, character, drift, effects, transition
curves); they never touch samples. User presets live in `localStorage`, so
they stay in that browser profile; projects are the portable format.

**Live recording** taps the final output (after the limiter) with a second
AudioWorklet that posts blocks to the main thread. With the File System
Access API (Chrome/Edge) the WAV is streamed to disk with positional writes
and its header patched on close, so memory use is flat however long the
take. Elsewhere it falls back to collecting in memory and downloading.
Limitation: a live recording also records any glitches the computer
causes (e.g. a busy CPU); offline render does not.

**Offline render** (`src/audio/offline-render.ts`, run in a Web Worker)
builds a fresh `MorphCore` from the same messages the worklet receives and
calls `process()` in chunks, streaming encoded WAV bytes out. It forces
once mode, forward direction and position 0, renders the morph plus a tail
(reverb/delay decay) and fades the last 0.1 s. Because every engine is
seeded, the same project renders bit-identically every time (tested). MIDI
movements and live drift changes are not part of an offline render; it
renders the saved settings.

## Testing

* `npm test`: Vitest unit and audio-rendering tests: curves, timeline
  interpolation, clock modes, duration parsing, loop seams, crossfade
  rendering (clicks via second-difference analysis, level stability via RMS
  windows, clipping, silence gaps, pause/stop/seek/reverse, sample swap,
  mute/solo, loop-mode wrap).
* `npm run test:browser`: builds the app, drives it in headless Chromium,
  loads WAV (44.1/48 kHz), AIFF (22.05 kHz, resampled) and a corrupt file,
  records the real output through the recorder tap and analyses it. From
  Phase 5 it also stubs the save dialog to check a project save/open round
  trip, a factory preset, a live 24-bit WAV recording and an offline render
  (38 checks).
