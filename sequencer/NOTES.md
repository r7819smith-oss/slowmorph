# seq4h handoff notes

Use this to resume work in a new chat or a Claude Project. For usage and
controls see [README.md](README.md). The sequencer is on `main`. USB MIDI
work continues on `claude/project-thread-pro3ph`; generator/evolution work on
`claude/project-thread-rg0c0f` (PR #4).

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
- **Controls are for navigation and editing only** (Roy, 2026-11-19: no
  finger drumming / live performance shortcuts). Four pages: GRID, STEP,
  TRACK, GENERATE. L1/R1 = page, L2/R2 = track, B = back to grid; list pages
  use up/down = row, left/right = value, hold Y = big step.
- **Input** goes through `src/input.c`: a table from keyboard keys, raw
  joystick buttons/hats/axes and SDL game-controller buttons/axes to seq4h
  buttons and actions, overridable from `seq4h.input`. On Knulli the shipped
  file sets `mode joystick`, because H700 pads can carry a GUID that SDL
  matches to the wrong controller mapping (reported for the RG35XX SP).
- **Generators write into the step grid** (not a separate live mode), so every
  result is editable and the save file stays simple. Locked steps are never
  touched.
- **Evolution** runs at bar boundaries (every 1, 2, 4 or 8 bars). It can mutate
  pitch, rhythm and velocity, rotate the pattern, or re-run the generator.
- **Generators:** EUCLID, WALK, MARKOV (learns from the current phrase), CHAOS
  (logistic map), CELLULAR (advances a generation per roll), WEIGHTED,
  FOLLOW (another track's rhythm, plus interval/harmony), ARP (scale chords),
  TURING (looping shift register; state kept in `GenSettings.state`, advanced
  one loop per roll), MOTIF (short phrase developed by transpose/invert/retro).
  New generator types are appended to `GenType` so saved type numbers stay valid.
- **Evolution extras:** anchor + `pull` (drifted steps return to the anchor),
  `EVO_PROB` (probability drift, 10-100%), `min_hits`/`max_hits` (0 = no max).
  Order each bar: pull, then regen, then mutations, then rotate, then density.
  Rotate now rotates around locked steps instead of refusing when any is locked.
- **Transforms** (`gen_transform`): reverse, invert, transpose, shift, thin,
  fill, humanize. All skip locked steps and stay within the track length.
- **Scale snap:** `seq_set_scale(s, scale, root, snap)` moves notes into the
  new scale. The UI still sets `s->scale` directly; wiring it is pending.
- **Save format** (still `seq4h 1`): new `gs` (generator state), `ev` (pull,
  min, max) and `a` (anchor step) lines. Old files load with neutral defaults;
  old binaries ignore the new lines.
  Deliberately not copied from SEQ-4: its matriceal multi-lane editor and its
  effects/CC features.
- **Channels:** tracks default to MIDI channels 1-4, which matches Felucca's
  default `MIDI IN: CH1-4` (channels 1-4 play tracks 1-4). `--channel N` puts
  all tracks on one channel (Felucca's SEL mode).
- **Shrub was dropped** as the target firmware (it is single-channel and has no
  Bluetooth MIDI). Felucca is the target.

## State

Works and tested on desktop Linux:

- engine, generators, evolution, save/load (`make test`; `tests/test_engine.c`
  and `tests/test_gen.c`, also clean under ASan/UBSan)
- timing thread
- SDL2 UI, checked through headless screenshots (`--screenshot`, `--keys`)

**Not verified:**

- **USB host mode on the RG40XX-V's USB-C port**, and whether it powers the
  FM1. See *USB MIDI on the RG40XX-V* below for what is known and the test
  checklist.
- **The ALSA output against real hardware.** The build container had no
  sound devices, so the code compiles and falls back cleanly, nothing more.
- **Any run on the handheld.** The aarch64 build now runs in CI
  (`.github/workflows/seq4h.yml`) but the binary has not been run on Knulli.
- **Button codes on a Knulli unit.** The defaults (README, "Button mapping")
  come from a published log of an RG40XX-V on stock firmware: A=0 B=1 Y=2
  X=3 L1=4 R1=5 Select=6 Start=7 L2=10 R2=11, D-pad on hat 0. L2/R2 are
  digital buttons there, not trigger axes. Confirm with `--show-input` and fix
  any difference in `seq4h.input`, not in code.
- **Felucca's clock-source labels.** The wiki puts the clock source on the
  CLOCK page (tap HOME: HOME > MIXER > CLOCK) but does not name the options.
  Whether Felucca follows MIDI Start/Stop is not documented.
- Bluetooth: the Felucca wiki text lists only USB and TRS, so don't plan on it.
- `knulli/SEQ4H.sh` launcher is untested.

## Next steps (suggested order)

1. Run the checklist below on the RG40XX-V and fill in the results table.
2. If the Knulli kernel has no USB MIDI driver, build the modules (see below).
3. Check button codes (`SEQ4H_SHOW_INPUT=1`, then read `seq4h.log`) and
   fullscreen on the device; adjust `seq4h.input`.
4. Generate-page UI for the new engine features (approved by Roy
   2026-11-19): EVO PROB, PULL / SET ANCHOR, MIN/MAX HITS, transforms, slots
   and chains, hold, scale snap, and a STYLE row in the M-button main menu.
5. Features discussed but not built:
   - painting per-note chances for WEIGHTED, closer to SEQ-4's stochastic mode
   - Program Change on pattern load (Felucca loads a track's sound by preset
     number)
   - a song mode on top of slot chains

## USB MIDI on the RG40XX-V

### What is known (researched 2026-11-19, not tested)

- **One USB-C port.** Reviews and Anbernic's spec page describe a single
  USB-C port for charging (5V/1.5A) and data; Anbernic lists "wired controller
  connection", which implies USB host on that port. The
  [m8c-rg35xx-knulli](https://github.com/jamesMcMeex/m8c-rg35xx-knulli) author
  reports driving a Dirtywave M8 (a USB device) from an RG40XXV under Knulli,
  so host mode on the port very likely works. Nobody reports whether the port
  supplies enough power for another device.
- **Charging and the FM1 share the port.** While the FM1 is plugged in, the
  handheld runs on battery unless a USB-C hub with power pass-through works
  on it (hit and miss on handhelds; test before relying on it).
- **The kernel may lack the USB MIDI driver.** In the 2024 Knulli H700 SDK
  (kernel 4.9.170, `linux-sunxi64-legacy.config`) `CONFIG_SND_USB_AUDIO` was
  not set, and the m8c port ships its own `snd-usb-audio.ko`,
  `snd-usbmidi-lib.ko` and `snd-hwdep.ko`. Newer Knulli builds may include
  them; the check script answers this. The ALSA sequencer (`snd-seq`) may be
  missing too.
- **Felucca** ([wiki](https://github.com/hugelton/Felucca/wiki)): USB is
  class-compliant MIDI in and out plus a USB audio input. `MENU > MIDI >
  MIDI IN` defaults to **CH1-4**, so channels 1-4 play tracks 1-4 (seq4h's
  default). The clock source is on the **CLOCK** page (tap HOME to cycle HOME,
  MIXER, CLOCK); the wiki says "clock from internal, USB or TRS". `MENU >
  SYSTEM > USB SERIAL` OFF makes it a plain audio + MIDI device. Program
  Change loads a track's sound (since 1.5.1). Response to Start/Stop is not
  documented. Stable line is 1.8.x (1.9.0 is a beta).

### Code changes for this

- `src/midi_alsa.c` now **falls back to raw MIDI** (`/dev/snd/midiC*D*`) when
  the ALSA sequencer is missing, so only `snd-usb-audio` is needed.
  `--midi list` shows both kinds (`seq` and `raw` lines); `--midi` matches
  either name, e.g. `--midi FM` or `--midi hw:1,0,0`.
- **Hot-plug:** if the FM1 is not there at start, or is unplugged, seq4h keeps
  running, shows `MIDI: WAITING FOR ...` in the header, and reconnects within
  about a second when it appears. If that happens while playing, it restarts
  from step 1 with a fresh Start so the FM1's clock is back in phase.
- **Clock master:** MIDI clock now runs continuously, also while stopped, so
  the FM1 follows seq4h's tempo before Start. Start/Stop as before.
- **`--midi-test`** plays C4, E4, G4, C5 on channels 1-4 in turn, then all four
  together, then sends All Notes Off and exits.
- `knulli/USB-MIDI-CHECK.sh`: a Ports-menu diagnostic that logs USB role,
  devices, kernel sound config, modules, ALSA cards and MIDI ports to
  `seq4h/usb-midi-check.log`.
- `knulli/SEQ4H.sh` logs to `seq4h/seq4h.log`, runs `modprobe` for the USB
  MIDI modules, and loads `.ko` files from `seq4h/modules/` if you add them.
- CI (`.github/workflows/seq4h.yml`) builds an aarch64 binary on Debian
  bookworm (glibc 2.36, dynamic SDL2 and ALSA) and uploads
  `seq4h-knulli-aarch64` with both scripts.

### Test checklist

Fill in the results table as you go. Tick each box or note what happened.

**A. Install**

- [ ] Download the `seq4h-knulli-aarch64` artifact from the latest
      *seq4h aarch64 build* run on GitHub Actions and unzip it.
- [ ] Copy `ports/SEQ4H.sh`, `ports/USB-MIDI-CHECK.sh` and the `ports/seq4h/`
      folder to `/userdata/roms/ports/` (SHARE partition `roms/ports`, or over
      SSH/SMB). Over SSH run `chmod +x` on both scripts and `seq4h/seq4h`
      (the zip drops the execute bit).
- [ ] Update the game list; both entries appear under Ports.

**B. Driver check (no FM1)**

- [ ] With the FM1 unplugged, run *USB-MIDI-CHECK* from Ports.
- [ ] Read `seq4h/usb-midi-check.log`. Note the kernel version, and whether
      `CONFIG_SND_USB_AUDIO` is `y`, `m` or missing, and the same for
      `CONFIG_SND_SEQUENCER`.
- [ ] Note any `/sys/class/usb_role/*/role` or `musb` mode lines (tells us
      whether the port is OTG and how its role is switched).

**C. USB host and power**

- [ ] Charge both devices fully. Switch the FM1 on.
- [ ] Connect FM1 USB-C to RG40XX-V USB-C with a USB-C to USB-C **data**
      cable. If nothing happens, try a USB-C OTG adapter (USB-C male to USB-A
      female) plus an A-to-C cable.
- [ ] Run *USB-MIDI-CHECK* again. Under *USB devices* the FM1 should appear
      (vendor:product, name). Record both.
- [ ] If the FM1 is listed but `/proc/asound/cards` has no new card, the
      kernel lacks `snd-usb-audio`: stop here and go to *If the driver is
      missing*.
- [ ] Power: does the FM1 show it is charging or powered from USB? Does the
      handheld's battery drain noticeably faster? (Check after 30 minutes.)
- [ ] Optional (Roy prefers direct USB; a hub is the fallback): a hub with
      power pass-through. Does the handheld charge and still see the FM1?

**D. Ports and notes (SSH)**

- [ ] `cd /userdata/roms/ports/seq4h && ./seq4h --midi list`. Record the
      `seq` and/or `raw` lines for the FM1.
- [ ] On the FM1, `MENU > MIDI > MIDI IN` = **CH1-4**. Give the four tracks
      clearly different sounds.
- [ ] `./seq4h --midi FM --midi-test` (use part of the name from the list if
      it is not "FM"). Expect track 1 C4, track 2 E4, track 3 G4, track 4 C5,
      then all four. Record which track plays each note.
- [ ] Repeat with the sequencer missing if the log showed it is built as a
      module: `rmmod snd-seq-midi` then `--midi-test` again (checks the raw
      MIDI fallback). Skip if not possible.

**E. Handheld as clock master**

seq4h is the clock master: it sends MIDI clock (24 per quarter note)
continuously, even while stopped, plus Start (from step 1) and Stop. The FM1
follows it. `--no-clock` turns all of this off.

Set Felucca to follow external clock over USB:

1. On the FM1, tap **HOME** until the **CLOCK** page shows (HOME, MIXER,
   CLOCK). It holds BPM, swing and the clock source.
2. Set the **clock source** to **USB** (the wiki lists internal, USB or TRS;
   the exact label on screen is unconfirmed, so record it below). Not TRS:
   that is the 3.5 mm jack.
3. If the BPM field is greyed out or follows incoming clock, that confirms it.

Checks:

- [ ] Start *SEQ4H* from Ports, with seq4h **stopped**. The FM1's BPM already
      shows seq4h's tempo (120 for the demo), because clock runs while stopped.
- [ ] Change BPM on the TRACK page (hold Y for steps of 10). The FM1's BPM follows
      within a second or so.
- [ ] Press Start: all four tracks play on the matching Felucca tracks.
      Anything tempo-synced on the FM1 (arpeggiator, LFO SYNC, delay) locks
      to seq4h.
- [ ] Press Start again (stop): notes stop, nothing hangs. Does the FM1's own
      sequencer start and stop with seq4h? (Felucca's Start/Stop handling is
      not documented, so record what happens.)
- [ ] Swing: seq4h's swing applies to its own notes only. Note whether
      Felucca's CLOCK-page swing also affects them.
- [ ] Mute a track (X): the matching FM1 track goes silent.
- [ ] Run `--channel 1` with `MIDI IN` = SEL: everything plays on the FM1's
      selected track.

**F. Hot-plug**

- [ ] Start *SEQ4H* with the FM1 unplugged: header shows
      `MIDI: WAITING FOR ...`.
- [ ] Plug the FM1 in: within about 2 seconds the header shows its name, and
      the FM1 picks up the tempo. Start plays it.
- [ ] Unplug while playing, plug back in: seq4h restarts the pattern from
      step 1 with a fresh Start, so the FM1 is back in phase. Nothing crashes;
      note any hung notes.

**G. Timing and endurance**

- [ ] Play 30 minutes at 120 BPM with evolution on. No drift between the FM1's
      clock-synced parts and seq4h, no stuck notes, no crash.
- [ ] Listen at 200+ BPM with all four tracks busy: are steps even?
- [ ] Check `seq4h/seq4h.log` for `raw MIDI messages dropped`.

**H. Handheld UI**

- [ ] Fullscreen fills the screen; text is readable.
- [ ] Start once with `SEQ4H_SHOW_INPUT=1` and press every button. The top
      right shows the code (`IN: JOY 10 > L2`); `seq4h.log` lists them and
      the pad's name and GUID. Note any code that differs from the table in
      README "Button mapping" and fix it in `seq4h.input`.
- [ ] Face buttons: on the grid A toggles a step, X opens the STEP page,
      Y locks, B returns to the grid.
- [ ] L1/R1 change page, L2/R2 change track.
- [ ] Select+Start quits and the pattern is saved (`seq4h.pat` updated).

**Results**

| Item | Result |
|---|---|
| Knulli version / kernel | |
| `CONFIG_SND_USB_AUDIO` / `CONFIG_SND_SEQUENCER` | |
| FM1 seen as USB device (vid:pid, name) | |
| Cable or adapter that worked | |
| FM1 powered by handheld? | |
| MIDI port name(s) in `--midi list` | |
| Channels 1-4 -> tracks 1-4 | |
| Felucca CLOCK page: source label for USB | |
| FM1 follows tempo while stopped / on Start / Stop | |
| Hot-plug | |
| 30 min run | |
| Button mapping, L2/R2 | |

### If the driver is missing

Build `snd-usb-audio.ko`, `snd-usbmidi-lib.ko` and `snd-hwdep.ko` (and
`snd-seq*.ko` if wanted) against the exact Knulli kernel, the way the m8c port
does: Knulli's `rg35xx-plush-sdk` aarch64 toolchain, the kernel source and
`linux-sunxi64-legacy.config`, set `CONFIG_SND_USB_AUDIO=m`, build modules.
Put the `.ko` files in `/userdata/roms/ports/seq4h/modules/`; `SEQ4H.sh` loads
them. The `vermagic` must match `uname -r` on the device. This is not
automated yet; it is the next job if check B shows the driver is absent.

## Handy commands

```sh
cd sequencer
make test                      # engine and generator tests
make                           # needs libsdl2-dev and libasound2-dev
./seq4h --midi none --demo --autoplay --verbose     # no hardware: print MIDI
./seq4h --midi list                                 # sequencer and raw MIDI ports
./seq4h --midi FM --midi-test                       # one note per channel 1-4
SDL_VIDEODRIVER=dummy ./seq4h --midi none --demo --screenshot out.bmp \
    --keys "Down,c,Down,Right"  # replay key names, save a screenshot
```

`--keys` takes comma-separated SDL key names; `+name` presses only and
`-name` releases only. Keyboard map: arrows = D-pad, Z = A, X = B, C = X,
V = Y, Q/W = L1/R1, 1/2 = L2/R2, Tab = Select, Space = Start,
E/R/U/K = page/roll/undo/lock,
Esc = quit. `./seq4h --dump-input` prints the whole map.

## Files

- `src/engine.c`, `engine.h`: tracks, steps, tick processing, save/load
- `src/gen.c`, `gen.h`: scales, generators, evolution
- `src/player.c`: timing thread and lock
- `src/midi_alsa.c`, `midi_null.c`: MIDI output backends
- `src/ui.c`: SDL2 interface and built-in 5x7 font
- `tests/test_engine.c`: engine unit tests
- `tests/test_gen.c`: tests for the newer generators, transforms and evolution
- `src/input.c`, `input.h`: button mapping and the `seq4h.input` file
- `tests/test_input.c`: mapping tests
- `knulli/seq4h.input`: RG40XX-V button map
- `knulli/SEQ4H.sh`: Ports launcher script
- `knulli/USB-MIDI-CHECK.sh`: USB/MIDI diagnostic for the device
