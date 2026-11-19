# seq4h

A four-track step sequencer for the Anbernic RG40XX-V running Knulli. It sends
MIDI to an M-VAVE FM1 running Shrub (or any other MIDI device). It is inspired by [SEQ-4](https://github.com/devrvega/SEQ-4) but shares
no code with it.

**State:** the engine, UI and timing run and are tested on a desktop Linux
machine. The ALSA MIDI output compiles but has **not been run against real
MIDI hardware**, and nothing has been run on the RG40XX-V yet.

## Features

- 4 tracks x 16 steps (1/16 notes), each with its own MIDI channel and length (1-16, so tracks can be polymetric)
- Per step: on/off, note, velocity, gate length, probability
- Swing, mute per track, BPM 30-300
- MIDI clock, start and stop output (turn off with `--no-clock`)
- Pattern autosaved on quit to `seq4h.pat` (plain text)
- 96 ticks per quarter note from a dedicated timing thread; measured 0.1 ms
  deviation on desktop Linux

## Controls

Handheld buttons (keyboard in brackets):

| Input | Action |
|---|---|
| D-pad | move cursor (up/down = track, left/right = step) |
| A [Z] | step on/off |
| X [C] | mute track |
| B [X] + up/down | note up/down |
| B + left/right | velocity -/+ 8 |
| Y [V] + up/down | gate +/- 10% |
| Y + left/right | probability -/+ 10% |
| Select [Tab] + left/right | track length |
| Select + up/down | track MIDI channel |
| L/R [Q/W] | BPM -/+ 1 (hold B for 10) |
| Y + L/R | swing |
| Start [Space] | play/stop |
| Select + Start [Esc] | quit |

Face buttons follow SDL's naming (A = south button). If the RG40XX-V's labels
are swapped on your build, swap them in `map_pad()` in `src/ui.c`.

## Build and run on a desktop Linux machine

```sh
sudo apt install build-essential libsdl2-dev libasound2-dev
make test            # engine unit tests
make
./seq4h --midi list  # show MIDI output ports
./seq4h --midi FM    # port whose name contains "FM"; no --midi = first port
```

Without hardware: `./seq4h --midi none --verbose --autoplay` prints the MIDI
it would send.

## Connecting to the FM1 running Shrub

From the [Shrub user guide](https://shrub.audio/guide/) (MIDI and USB section):

- Shrub listens on **USB MIDI** and the **TRS MIDI IN** jack, on the single
  channel set by its **MIDI CH** setting (OMNI, 1-16 or MPE). It is not
  multitimbral, so all four seq4h tracks play the same sound.
- It responds to notes with velocity, program change, MIDI clock with
  start/stop, and CC 120/123 (panic). seq4h sends exactly these.
- The guide lists no Bluetooth MIDI input, so **Bluetooth is not an option
  with Shrub**. Use USB.
- Shrub follows incoming MIDI clock, including tempo. While seq4h is playing
  it sets Shrub's tempo; press PLAY on the FM1 to take the lead back.

Setup:

1. On the FM1, set Shrub's MIDI CH to **OMNI**, or to a channel number and run
   `./seq4h --channel N` with the same N (1-16). With OMNI the track channels
   don't matter.
2. Connect the FM1's USB-C to the handheld's USB-C. The handheld must act as
   USB host. Whether the RG40XX-V's port can do that is **unverified**; you may
   need a USB-C OTG adapter or a powered hub. The FM1 is the USB device and
   also streams its audio over this link.
3. Run `./seq4h --midi list` and check the FM1 appears, then
   `./seq4h --midi FM --channel 1` (or part of the name that `--midi list`
   shows).

Tracks share one channel, so if two tracks play the *same* note at once, the
first note-off cuts both. Give tracks different pitch ranges to avoid it.

Other MIDI devices work the same way; use per-track channels (Select + up/down)
for multitimbral gear.

## Building for Knulli

Knulli (RG40XX-V, Allwinner H700) is 64-bit ARM Linux. Cross-compile on a
desktop with an aarch64 toolchain and the arm64 SDL2 and ALSA dev libraries,
for example:

```sh
make CC=aarch64-linux-gnu-gcc PKG=aarch64-linux-gnu-pkg-config
```

Then copy `seq4h` and `knulli/SEQ4H.sh` as described in the script. The binary
needs a matching libc and SDL2 on the device; the toolchain choice is still to
be settled and a CI build is a sensible next step.

## Layout

- `src/engine.c` sequencer core (pure C, unit tested in `tests/`)
- `src/player.c` timing thread
- `src/midi_alsa.c` ALSA sequencer output; `src/midi_null.c` stub/logging output
- `src/ui.c` SDL2 interface, built-in font, no extra libraries
