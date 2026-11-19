# seq4h

A four-track step sequencer for the Anbernic RG40XX-V running Knulli. It sends
MIDI to an M-VAVE FM1 running Felucca firmware (or any other MIDI device). It is inspired by [SEQ-4](https://github.com/devrvega/SEQ-4) but shares
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

## Connecting to the FM1 running Felucca

From the [Felucca wiki](https://github.com/hugelton/Felucca/wiki) (MIDI section,
as quoted to me; I could not open the wiki myself):

- Felucca takes MIDI on **USB** and the **TRS MIDI IN** jack. With
  `MENU > MIDI > MIDI IN` set to **CH1-4**, MIDI channels 1-4 play tracks 1-4.
  That is `seq4h`'s default (track 1 on channel 1 ... track 4 on channel 4), so
  the four sequencer tracks drive the four Felucca tracks with no setup.
- Other `MIDI IN` modes: **CH5-8**, **CH9-12**, **CH13-16** (another block of
  four, for example for two FM-1s on one cable), **SEL** (every channel plays
  the *selected* track) and **TRACK** (each track on its own channel, 1-16, OMNI
  or OFF, set in `MENU > MIDI > T1-T4 CHANNEL`).
- Felucca responds to notes, pitch bend, sustain and panic, and to MIDI clock
  from internal, USB or TRS. It also responds to track CCs (5 glide, 7 level,
  10 pan, 71 resonance, 72/73/75 release/attack/decay, 74 brightness,
  91/93/94 reverb/chorus/delay sends) and Program Change (loads a track's
  sound by its preset number). `seq4h` does not send CCs or Program Change yet.

Setup:

1. Connect the FM1's USB-C to the handheld's USB-C. The handheld must act as
   USB host. Whether the RG40XX-V's port can do that, and whether it powers the
   FM1, is **unverified**; you may need a USB-C OTG adapter or a powered hub.
2. On the FM1, set `MENU > MIDI > MIDI IN` to **CH1-4** (see the wiki for the
   current default).
3. Set the FM1's **clock source to USB** so it follows `seq4h`'s tempo and
   start/stop. I don't know where that setting lives in Felucca's menu; check
   the wiki. Without it the FM1 keeps its own clock and only the notes line up.
4. Run `./seq4h --midi list` and check the FM1 appears, then
   `./seq4h --midi FM` (or part of the name the list shows).

For the other `MIDI IN` modes:

- **CH5-8 / CH9-12 / CH13-16:** set each track's channel in `seq4h` with
  Select + up/down.
- **SEL:** run `./seq4h --channel N` so all four tracks use one channel. The
  FM1 then plays them all on the track you've selected on the device.
- **TRACK:** set the matching channels in `seq4h` the same way.

Settings are saved in `seq4h.pat`, so you only set channels once.

**Bluetooth:** the wiki text lists USB and TRS only, so don't count on it.

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
