# seq4h

A four-track step sequencer for the Anbernic RG40XX-V running Knulli. It sends
MIDI to an M-VAVE FM1 running Felucca firmware (or any other MIDI device). It is inspired by [SEQ-4](https://github.com/devrvega/SEQ-4) but shares
no code with it.

**State:** the engine, UI and timing run and are tested on a desktop Linux
machine. The ALSA MIDI output compiles but has **not been run against real
MIDI hardware**, and nothing has been run on the RG40XX-V yet.

## Features

- 4 tracks x 16 steps (1/16 notes), each with its own MIDI channel and length (1-16, so tracks can be polymetric)
- Per step: on/off, note, velocity, gate length, probability, and a **lock**
- A global **scale and root** (12 scales). Editing a note steps by scale degree.
- **Pattern generators** and **evolution** (below)
- Swing, mute per track, BPM 30-300
- MIDI clock, start and stop output (turn off with `--no-clock`)
- Pattern, generator and scale settings autosave on quit to `seq4h.pat` (plain text)
- 96 ticks per quarter note from a dedicated timing thread; measured 0.1 ms
  deviation on desktop Linux

The sequencer sends notes, clock and start/stop only. Effects, CCs and sounds
are left to the FM1.

## Making and evolving patterns

Generators **write into the step grid**, so you can always edit the result by
hand. Press **Select+X** to open the generate page for the current track, pick a
generator, set its parameters and press **A** to roll a new pattern. **Select+Y**
undoes the last roll. Steps you lock (**Select+A** on the grid, shown as a
yellow square) are never touched by a roll or by evolution.

| Generator | What it does | Parameters |
|---|---|---|
| EUCLID | Spreads hits evenly over the steps. Rhythm only by default, so your melody stays. | hits, rotation, pitch (keep / root / random) |
| WALK | A random walk through the scale. | max step size, up/down bias, density |
| MARKOV | Learns which notes follow which in the track's *current* phrase and writes a new phrase in that style. Rolling again learns from the new phrase, so it drifts like a game of telephone. | chaos (chance of a random note), rhythm (keep or random density) |
| CHAOS | Iterates the logistic map. Low `r` gives repeating patterns, high `r` gives chaos. | r, seed, density |
| CELLULAR | A cellular automaton (rules 30, 90, 110 and more) read as rhythm, with pitch from each cell's neighbourhood. Each roll advances one generation, so it scrolls through time. | rule, generation, seed (single / random) |
| WEIGHTED | Chance of each scale note, favouring root, fifth and third; like SEQ-4's stochastic mode. | tension (flatter weights), density, repeat chance |

**Evolve** changes a track every 1, 2, 4 or 8 bars while it plays. `AMOUNT` is
the chance each step changes. Choose what may change: pitch (one scale degree
up or down), rhythm (toggle a step), velocity, rotate (shift the whole pattern
one step), and regen (re-run the generator, so CELLULAR advances a generation
each time). Together with per-step probability, this gives patterns that keep
their character but never settle.

The generated notes come from the track's `LOWEST NOTE` and `RANGE` (octaves)
in the chosen scale.

## Controls

Handheld buttons (keyboard in brackets). **Grid page:**

| Input | Action |
|---|---|
| D-pad | move cursor (up/down = track, left/right = step) |
| A [Z] | step on/off |
| X [C] | mute track |
| B [X] + up/down | note up/down by scale degree |
| B + left/right | velocity -/+ 8 |
| Y [V] + up/down | gate +/- 10% |
| Y + left/right | probability -/+ 10% |
| Select [Tab] + left/right | track length |
| Select + up/down | track MIDI channel |
| Select + A | lock/unlock the step |
| Select + B [R] | roll a new pattern with the track's generator |
| Select + Y [U] | undo the last roll |
| Select + X [E] | switch between grid and generate page |
| L/R [Q/W] | BPM -/+ 1 (hold B for 10) |
| Y + L/R | swing |
| Start [Space] | play/stop |
| Select + Start [Esc] | quit |

**Generate page:** up/down chooses a row (track, generator, its parameters,
lowest note, range, evolve settings, scale, root), left/right changes the
value (hold B for bigger steps), A rolls, Select+Y undoes, Select+X returns to
the grid. L2 and R2, if your handheld reports them as triggers, also switch
page and roll; this is untested.

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

1. Connect the FM1's USB-C to the handheld's USB-C (its only port, also used
   for charging). The handheld must act as USB host. Other H700 projects
   report this works, but it is **unverified** for seq4h, as is whether it
   powers the FM1 and whether Knulli's kernel has the USB MIDI driver. Run
   `USB-MIDI-CHECK` from Ports to find out; see the checklist in
   [NOTES.md](NOTES.md).
2. On the FM1, set `MENU > MIDI > MIDI IN` to **CH1-4** (see the wiki for the
   current default).
3. Set the FM1's **clock source to USB** so it follows `seq4h`'s tempo: it is
   on the CLOCK page (tap HOME to cycle HOME, MIXER, CLOCK). Without it the
   FM1 keeps its own clock and only the notes line up.
4. Run `./seq4h --midi list` and check the FM1 appears, then
   `./seq4h --midi FM --midi-test` to hear one note on each track, then
   `./seq4h --midi FM` (or part of the name the list shows).

If the FM1 is unplugged or plugged in after start, seq4h shows
`MIDI: WAITING FOR ...` and connects when it appears. Without the ALSA
sequencer in the kernel, seq4h uses raw MIDI devices (`hw:C,D,S`) instead.

For the other `MIDI IN` modes:

- **CH5-8 / CH9-12 / CH13-16:** set each track's channel in `seq4h` with
  Select + up/down.
- **SEL:** run `./seq4h --channel N` so all four tracks use one channel. The
  FM1 then plays them all on the track you've selected on the device.
- **TRACK:** set the matching channels in `seq4h` the same way.

Settings are saved in `seq4h.pat`, so you only set channels once.

**Bluetooth:** the wiki text lists USB and TRS only, so don't count on it.

## Building for Knulli

Knulli (RG40XX-V, Allwinner H700) is 64-bit ARM Linux. One script builds,
tests and packages it, locally or in CI. It needs only Docker:

```sh
sequencer/knulli/build.sh
```

It cross-compiles in an Ubuntu 20.04 container (glibc 2.31, older than any
Knulli release), links the device's own SDL2 and ALSA libraries, runs the unit
tests natively and under qemu, draws one frame of the aarch64 binary under
qemu as a smoke test, and writes:

- `dist/seq4h`, the aarch64 binary
- `dist/seq4h-knulli-aarch64.zip`, holding `SEQ4H.sh`, `USB-MIDI-CHECK.sh`
  and `seq4h/seq4h`. Unzip it into `/userdata/roms/ports/` and refresh the
  game list. The zip keeps the execute bits.

The output is byte-for-byte reproducible. GitHub Actions runs the same script
on every change (workflow *seq4h aarch64 build*, artifact
`seq4h-knulli-aarch64`). The binary has not been run on Knulli yet.

## Layout

- `src/engine.c` sequencer core (pure C, unit tested in `tests/`)
- `src/gen.c` scales, generators and evolution (also unit tested)
- `src/player.c` timing thread
- `src/midi_alsa.c` ALSA sequencer output; `src/midi_null.c` stub/logging output
- `src/ui.c` SDL2 interface, built-in font, no extra libraries

See [NOTES.md](NOTES.md) for design decisions, what is untested, and next steps.
