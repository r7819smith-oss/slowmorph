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

## Testing without hardware

`make check` runs the whole hardware-free harness (`tests/harness.sh`) and
writes everything to `out/`. CI runs the same thing on every push that touches
`sequencer/` (`.github/workflows/sequencer-harness.yml`) and uploads the
screenshots and MIDI captures as the `seq4h-harness` artifact.

- **MIDI capture backend** (`src/midi_capture.c`): `--midi capture:FILE`
  writes every message with a microsecond timestamp instead of sending it.
  `--capture FILE` does the same *alongside* a real port, so a hardware run
  records exactly what reached the FM1.
- **`seq4h-render`** (`tools/render.c`): runs the engine offline, stamped with
  ideal tick times. Exact and repeatable; used for soak tests (256 bars of
  generators + evolution in `tests/fixtures/evolve.pat`).
- **`--headless SECS`**: the real binary and timing thread with no window;
  plays, stops cleanly, exits, does not autosave.
- **`tools/midicheck.py`** checks a capture: Start first and Stop last, notes
  only on channels 1-4 (tracks 1-4), every note-on released, one note per
  channel at a time, all-notes-off at Stop, clock at 24 PPQN and the right
  tempo, every onset on the 1/16 grid (with swing), and with `--demo` that the
  demo pattern plays as written. `tests/test_midicheck.py` proves it catches
  broken input (wrong channel, hung note, late note, slow clock and so on).
  It also reads `aseqdump` output (`--aseqdump`), without timing.
- **Screenshots**: nine UI states driven by `--keys` with
  `SDL_VIDEODRIVER=dummy`, checked for size, not blank, and that each key
  sequence changed the screen (`tools/shotcheck.py`, also converts to PNG).
- **ALSA loopback**: if `/dev/snd/seq` exists, seq4h sends through the real
  ALSA backend into `aseqdump`, and the dump is checked too.

Findings from the harness so far:

- **Fixed:** swing followed each track's own step number, so a track with an
  odd length (say 7) swung the wrong 16ths against the other tracks. Swing now
  follows the shared 1/16 grid (`seq_tick`, with a unit test).
- **Known, not fixed:** with `--channel N` (Felucca SEL mode) all four tracks
  share a channel, so when two tracks play the same note at once, the first
  note-off ends both. The demo does this (kick and bass both on C2).
- Desktop timing (shared cloud VM, not real-time): p99 under 0.5 ms, with rare
  single-tick hiccups of 4-12 ms. The handheld numbers are tonight's test.

## First hardware test: RG40XX-V to FM1 (Felucca)

Work down the list and note what happens at each step; the first failure is
the useful bit. Part A needs no FM1.

**What you need:** the handheld on Wi-Fi with SSH enabled (somewhere in Knulli's
network or services settings; I have not checked where), the `seq4h` aarch64 binary and `knulli/SEQ4H.sh`, the FM1
with Felucca, a USB-C to USB-C data cable, and an OTG adapter or powered USB hub
as a fallback. Copy the binary to `/userdata/roms/ports/seq4h/` and the script
to `/userdata/roms/ports/`.

**A. The handheld on its own**

1. `ssh root@<handheld-ip>`, then `cd /userdata/roms/ports/seq4h && ./seq4h --help`.
   It should print the options. A "not found" or missing-library error means
   the binary or its libraries don't match Knulli: copy the error exactly.
2. Timing check, no display: `./seq4h --midi capture:/tmp/h.cap --demo --headless 20`.
   Copy `/tmp/h.cap` to a PC (`scp root@<ip>:/tmp/h.cap .`) and run
   `tools/midicheck.py h.cap --demo --duration 20 --tolerance-ms 2 --max-ms 8`.
   All PASS means the H700 keeps time. Keep the "note timing error" line.
3. `ls /dev/snd/` should list `seq`. If not, try `modprobe snd-seq`.
4. `./seq4h --midi list` with nothing plugged in: no ports (or only system
   ones) is expected; "cannot open ALSA sequencer" means step 3 failed.
5. Launch **SEQ4H** from the Ports menu. Does it open full screen? Press each
   button and note what it does (the button mapping is being checked separately;
   just write down any that are swapped).

**B. Connect the FM1**

6. FM1 first: set `MENU > MIDI > MIDI IN` to **CH1-4**, and the clock source
   to **USB** if you can find it.
7. Plug FM1 USB-C into the handheld. Does the FM1 power up from it? Then over
   SSH: `dmesg | tail -20` (look for "new USB device" and the FM1's name) and
   `cat /proc/asound/cards`. If nothing appears, try the OTG adapter, then a
   powered hub, and note which worked.
8. `./seq4h --midi list` should show the FM1. Note the exact name.
9. `./seq4h --fullscreen --midi FM --demo --autoplay --capture /tmp/fm1.cap`
   (use part of the name from step 8). Expected: track 1 kick, track 2 hats,
   track 3 a bass line, track 4 a lead that skips some notes.
10. Channels: on the grid page, mute each track in turn (X). The matching FM1
    track, and only that one, should go silent.
11. Start/stop (Start button): the FM1 stops with no hanging notes. Change
    tempo with L/R: the FM1's tempo follows only if its clock source is USB.
12. Let it run 10 minutes. Listen for drift, stutters or stuck notes. Quit with
    Select+Start; everything should stop.
13. Check what was sent: copy `/tmp/fm1.cap` back and run
    `tools/midicheck.py fm1.cap` (add `--bpm` if you changed tempo, and drop
    `--demo` if you edited the pattern).
14. Optional: SEL mode. Set the FM1's MIDI IN to SEL and run
    `./seq4h --midi FM --demo --autoplay --channel 1`.

**Send back:** the `--midi list` output, the `dmesg` lines, the two
midicheck reports, which cable or adapter worked, and notes on buttons and
sound.

## Handy commands

```sh
cd sequencer
make test                      # engine and generator tests
make check                     # whole hardware-free harness, results in out/
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
- `src/midi_capture.c`: capture-to-file and tee MIDI backends
- `tools/render.c`: offline renderer (`seq4h-render`)
- `tools/midicheck.py`, `tools/shotcheck.py`: capture and screenshot checks
- `tests/test_engine.c`: unit tests
- `tests/harness.sh`, `tests/test_midicheck.py`, `tests/fixtures/`: harness
- `knulli/SEQ4H.sh`: Ports launcher script
