# seq4h handoff notes

Use this to resume work in a new chat or a Claude Project. For usage and
controls see [README.md](README.md). Branch: `claude/lucid-newton-7ah8m8`
(no PR opened yet).

## Goal

A four-track MIDI note-pattern sequencer for an **Anbernic RG40XX-V** running
**Knulli**, driving an **M-VAVE FM1** running **Felucca** firmware. It is
inspired by [SEQ-4](https://github.com/devrvega/SEQ-4) but shares no code with
it. The focus is *generating and evolving note patterns*: writing and editing
by hand, generating in several ways, and letting patterns drift through
probability and chance. Effects, CCs and sound design are left to the FM1.

## Decisions made

- **C, SDL2, ALSA sequencer.** The engine (`src/engine.c`, `src/gen.c`) is pure
  C with a send callback, so it is unit-tested on a normal machine. The ALSA
  sequencer backend was chosen because USB MIDI and BlueZ BLE-MIDI both show up
  as ALSA ports.
- **Timing:** a dedicated thread ticks at 96 PPQN using absolute
  `clock_nanosleep`. Measured 0.1 ms deviation on desktop Linux.
- **Generators write into the step grid** (not a separate live mode), so every
  result is editable and the save file stays simple. Locked steps are never
  touched.
- **Evolution** runs at bar boundaries (every 1, 2, 4 or 8 bars). It can mutate
  pitch, rhythm and velocity, rotate the pattern, or re-run the generator.
- **Generators:** EUCLID, WALK, MARKOV (learns from the current phrase), CHAOS
  (logistic map), CELLULAR (advances a generation per roll), WEIGHTED.
  Deliberately not copied from SEQ-4: its matriceal multi-lane editor and its
  effects/CC features.
- **Channels:** tracks default to MIDI channels 1-4, which matches Felucca's
  default `MIDI IN: CH1-4` (channels 1-4 play tracks 1-4). `--channel N` puts
  all tracks on one channel (Felucca's SEL mode).
- **Shrub was dropped** as the target firmware (it is single-channel and has no
  Bluetooth MIDI). Felucca is the target.

## State

Works and tested on desktop Linux:

- engine, generators, evolution, save/load (`make test`)
- timing thread
- SDL2 UI, checked through headless screenshots (`--screenshot`, `--keys`)

**Not verified:**

- **USB host mode on the RG40XX-V's USB-C port**, and whether it powers the
  FM1. This is the biggest unknown. Test with `./seq4h --midi list`.
- **The ALSA output against real hardware.** The build container had no
  sound devices, so the code compiles and falls back cleanly, nothing more.
- **Any run on the handheld**, and any aarch64 cross-build. The README suggests
  a command; the toolchain and library choice for Knulli is unsettled.
- **Face-button labels** (A/B may be swapped; `map_pad()` in `src/ui.c`).
- **L2/R2** page/roll shortcuts (it is unknown whether the handheld reports
  them as trigger axes).
- **Where Felucca's clock-source setting lives**; it must be USB for seq4h to
  set tempo and start/stop. I only saw the MIDI paragraph of the wiki that was
  pasted in, not the wiki itself (the build environment cannot reach it).
- Bluetooth: the Felucca wiki text lists only USB and TRS, so don't plan on it.
- `knulli/SEQ4H.sh` launcher is untested.

## Next steps (suggested order)

1. Run it on the RG40XX-V and answer the USB host question. If the port is
   device-only, try an OTG adapter or powered hub.
2. Add a GitHub Actions workflow that builds an aarch64 binary, so no local
   cross-compiler is needed.
3. Check button mapping and fullscreen on the device; adjust `map_pad()`.
4. Features discussed but not built:
   - a generator that reads another track (for example a bass line following
     the kick rhythm)
   - painting per-note chances for WEIGHTED, closer to SEQ-4's stochastic mode
   - Program Change on pattern load (Felucca loads a track's sound by preset
     number)
   - several pattern slots, chaining, and a song mode

## Handy commands

```sh
cd sequencer
make test                      # engine and generator tests
make                           # needs libsdl2-dev and libasound2-dev
./seq4h --midi none --demo --autoplay --verbose     # no hardware: print MIDI
SDL_VIDEODRIVER=dummy ./seq4h --midi none --demo --screenshot out.bmp \
    --keys "Up,+Tab,c,-Tab"    # replay key names, save a screenshot
```

`--keys` takes comma-separated SDL key names; `+name` presses only and
`-name` releases only. Keyboard map: arrows = D-pad, Z = A, X = B, C = X,
V = Y, Q/W = L/R, Tab = Select, Space = Start, E/R/U/K = page/roll/undo/lock.

## Files

- `src/engine.c`, `engine.h`: tracks, steps, tick processing, save/load
- `src/gen.c`, `gen.h`: scales, generators, evolution
- `src/player.c`: timing thread and lock
- `src/midi_alsa.c`, `midi_null.c`: MIDI output backends
- `src/ui.c`: SDL2 interface and built-in 5x7 font
- `tests/test_engine.c`: unit tests
- `knulli/SEQ4H.sh`: Ports launcher script
