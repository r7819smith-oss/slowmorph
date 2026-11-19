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
- Clock master: MIDI clock (also while stopped), start and stop output (turn off with `--no-clock`)
- Pattern, generator and scale settings autosave on quit to `seq4h.pat` (plain text)
- 96 ticks per quarter note from a dedicated timing thread; measured 0.1 ms
  deviation on desktop Linux

The sequencer sends notes, clock and start/stop only. Effects, CCs and sounds
are left to the FM1.

## Making and evolving patterns

Generators **write into the step grid**, so you can always edit the result by
hand. Go to the **GENERATE** page for the current track (L1/R1), pick a
generator, set its parameters and press **A** to roll a new pattern. **X**
undoes the last roll. Steps you lock (**Y** on the grid, shown as a yellow
square) are never touched by a roll or by evolution.

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

The controls are for moving around and editing values; there are no live
performance shortcuts. Four pages, in this order: **GRID**, **STEP**,
**TRACK** and **GENERATE**. The tabs under the title show where you are and
which track you are on. Keyboard keys are in brackets.

**Everywhere**

| Input | Action |
|---|---|
| L1 / R1 [Q / W] | previous / next page |
| L2 / R2 [1 / 2] | previous / next track |
| B [X] | back to the grid |
| Start [Space] | play / stop |
| Select + Start [Tab + Space, or Esc] | quit (the pattern is saved) |

**GRID page**: all four tracks.

| Input | Action |
|---|---|
| D-pad [arrows] | move the cursor (up/down = track, left/right = step) |
| A [Z] | step on / off |
| X [C] | edit this step (opens the STEP page) |
| Y [V] | lock / unlock this step |

**STEP, TRACK and GENERATE pages**: a list of values for the current track.
Up/down picks a row, left/right changes it, and holding **Y** makes the
change bigger.

| Page | Rows | Also |
|---|---|---|
| STEP | step number, on/off, note (scale degree; hold Y = octave), velocity, gate, probability, lock | A = step on/off |
| TRACK | track, MIDI channel, length, mute, BPM, swing, MIDI port (shown only) | |
| GENERATE | track, generator and its parameters, lowest note, range, evolve settings, scale, root | A = roll a new pattern, X = undo the roll |

The STEP page edits the step under the grid cursor, and its STEP row moves
that cursor, so you can walk through a pattern without going back to the
grid. A preview of the current track sits on the right of each list page.

## Button mapping

Every button can be remapped with a plain text file, `seq4h.input`, read from
the working directory at start (or `--input FILE`). Lines you leave out keep
their default, so the file only needs the changes. `--dump-input` prints the
full mapping in use, which is a good starting point.

### RG40XX-V under Knulli

The RG40XX-V's built-in pad shows up as a joystick called `ANBERNIC-keys`.
These are the raw SDL joystick codes it sends, from a published log of an
RG40XX-V on Anbernic's stock firmware
([source](https://github.com/swetoast/Anbernic-Firmware-Application-Development-Guide)).
Knulli should report the same codes, but **they have not yet been checked on
a Knulli unit**; run
`./seq4h --show-input` once to confirm (below).

| Button | Code | seq4h default |
|---|---|---|
| A | joy 0 | a |
| B | joy 1 | b |
| Y | joy 2 | y |
| X | joy 3 | x |
| L1 | joy 4 | l |
| R1 | joy 5 | r |
| Select | joy 6 | select |
| Start | joy 7 | start |
| Menu (held) | joy 8 | not used |
| Stick press | joy 9 | not used |
| **L2** | **joy 10** | **l2** (previous track) |
| **R2** | **joy 11** | **r2** (next track) |
| Menu (short press) | joy 13 | not used |
| D-pad | hat 0 (up/right/down/left) | up/right/down/left |
| Left stick | axes 0 (x) and 1 (y) | not used |

L2 and R2 are plain on/off buttons, not analog trigger axes. Note X and Y
are 3 and 2, not in label order.

`knulli/seq4h.input` holds this map and sets `mode joystick`. Copy it next
to the binary. Joystick mode matters: some H700 pads report an ID that SDL
mistakes for a different controller, and SDL's game-controller layer would
then scramble the buttons. Raw joystick mode reads the codes above directly.

### Checking the codes on your device

Run `./seq4h --show-input` (the Knulli launcher passes it when
`SEQ4H_SHOW_INPUT=1`). The top right of the screen then shows the last input
and what it does, for example `IN: JOY 10 > PAGE` or `IN: JOY 13 > NONE`, and
the same lines go to stderr (the launcher writes them to `seq4h.log`). On
start it also logs each pad's name, GUID and its numbers of buttons, hats and
axes. Press every button once; if one shows a different code from the table,
add a line for it to `seq4h.input`.

### File format

One mapping per line: `SOURCE CODE BUTTON`. `#` starts a comment, case does
not matter and later lines win.

| Source | Code | Example |
|---|---|---|
| `joy` | raw joystick button index, 0-31 | `joy 10 page` |
| `hat` | `up`, `down`, `left`, `right` (any hat) | `hat up up` |
| `axis` | raw axis index with `+` or `-`, 0-15 | `axis 1- up` |
| `pad` | SDL game-controller button or axis name (`a`, `leftshoulder`, `back`, `lefttrigger`, `leftx-` ...) | `pad righttrigger roll` |
| `key` | SDL key name, `_` for spaces | `key Left_Shift lock` |

`BUTTON` is one of `up down left right a b x y l r l2 r2 select start`, a
shortcut action `page` (next page), `roll`, `undo`, `lock` (this step) or
`quit`, or `none` to unmap. Two settings also go in the
file:

- `mode auto` (default): pads SDL recognises use the `pad` lines, other
  joysticks use `joy`/`hat`/`axis`. `mode joystick` always uses the raw
  lines; `mode controller` only opens recognised pads.
- `threshold N` (default 16000, out of 32767): how far an axis must move to
  count as a press.

Examples: swap A and B with `joy 0 b` and `joy 1 a`; make the stick press
undo the last roll with `joy 9 undo`; quit with a short Menu press with `joy 13 quit`.

On a desktop, the `pad` defaults use SDL's names (A = the bottom face button)
with the triggers as L2/R2; keyboard defaults are listed in the Controls
tables, plus E/R/U/K for page/roll/undo/lock.

## Build and run on a desktop Linux machine

```sh
sudo apt install build-essential libsdl2-dev libasound2-dev
make test            # engine unit tests
make
./seq4h --midi list  # show MIDI output ports
./seq4h --midi FM    # port whose name contains "FM"; no --midi = first port
```

Without hardware: `./seq4h --midi none --verbose --autoplay` prints the MIDI
it would send. `make check` runs the hardware-free test harness: it captures
the MIDI to files and checks notes, channels and timing, and renders UI
screenshots into `out/`. See [NOTES.md](NOTES.md#testing-without-hardware).

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

- **CH5-8 / CH9-12 / CH13-16:** set each track's channel in `seq4h` on the
  TRACK page.
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
- `dist/seq4h-knulli-aarch64.zip`, holding `SEQ4H.sh`, `USB-MIDI-CHECK.sh`,
  `seq4h/seq4h` and `seq4h/seq4h.input`. Unzip it into `/userdata/roms/ports/` and refresh the
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
- `src/input.c` button mapping: defaults and the `seq4h.input` file (unit tested)

See [NOTES.md](NOTES.md) for design decisions, what is untested, and next steps.
