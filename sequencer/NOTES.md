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
- **M opens a main menu** for session-wide settings (Roy, 2026-11-19; a
  music-style choice will go there, added by the pattern engine work). Under
  Knulli M is joy 11. Knulli also uses M for its hotkeys (M + another
  button). Add rows in `menu_row_*` in `src/ui.c`.
- **Button codes are shown on the device, no SSH** (2026-11-19): M > BUTTON
  CODES toggles a top-right readout (`JOY 10 = START`: raw code, then the
  seq4h button). The Ports entry `SHOW-BUTTONS.sh` starts seq4h with it on,
  for when M itself is wrong. Each press is also logged to `seq4h.log`.
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
- **32 steps, hold, slots, chains, styles:** `SEQ_STEPS` 32 (tracks start at
  16), `SeqStep.hold` ties a note over extra steps, 8 slots per track with a
  jump or morph chain advanced at bar boundaries (before evolution), and
  `Style` (AMBIENT default, TECHNO, MELODIC) which sets `gen_set_type`
  defaults; `style_apply` builds a full starting set and is used on first run.
  UI: GENERATE page rows (list scrolls; A does the row's action), HOLD on the
  STEP page, grid shows 16 steps at a time, MUSIC STYLE and NEW FROM STYLE in
  the M menu.
- **Per-track approach and manual tracks** (Roy, 2026-11-19): each track has its
  own generator; MANUAL (`GEN_NONE`) is the blank, write-it-yourself mode.
  Decisions made without asking: a manual track's evolution stays off unless
  turned on (evolving a hand-written pattern is allowed, just opt-in);
  `style_apply(..., keep_manual=1)` from the menu keeps MANUAL tracks that have
  notes and then also keeps scale/root; `XF_CLEAR` blanks a track; the grid
  shows `gen_short_name()` per track.
- **Save format** (still `seq4h 1`): new `gs` (generator state), `ev` (pull,
  min, max), `a`/`ah` (anchor step/hold), `h` (hold), `sl`/`ss`/`sh` (slots),
  `ch` (chain) and `style` lines. Old files load with neutral defaults;
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
- **Button codes on a Knulli unit, partly confirmed.** First hardware run
  (2026-11-19, Knulli on Batocera 42, kernel 4.9.170): seq4h ran from Ports
  and drove the FM1 over USB MIDI. The first defaults came from a stock
  firmware log (A=0 ... Start=7, L2/R2=10/11) and were wrong: L1 started
  playback and Start acted like L2. Knulli numbers the pad ("Deeplay-keys")
  3 higher: vol-=1 vol+=2 A=3 B=4 Y=5 X=6 L1=7 R1=8 Select=9 Start=10 M=11
  stick=12 L2=13 R2=14, D-pad hat 0 (the SDL mapping other Knulli ports such
  as m8c use for this pad). Those are now the defaults. Roy's press-by-press
  report on the old build fits this table for every button (A acted as X,
  B/Y as L1/R1, X as Select, L1 as Start, Start as L2, M as R2, L2 as M;
  R1, R2, Select and the stick did nothing). Confirm on the new build with
  M > BUTTON CODES.
- **Felucca's clock-source labels.** The wiki puts the clock source on the
  CLOCK page (tap HOME: HOME > MIXER > CLOCK) but does not name the options.
  Whether Felucca follows MIDI Start/Stop is not documented.
- Bluetooth: the Felucca wiki text lists only USB and TRS, so don't plan on it.
- `knulli/SEQ4H.sh` launcher works on Roy's RG40XX-V (2026-11-19).

## Next steps (suggested order)

1. Run the checklist below on the RG40XX-V and fill in the results table.
2. If the Knulli kernel has no USB MIDI driver, build the modules (see below).
3. Check button codes (M > BUTTON CODES, or SHOW-BUTTONS in Ports) and
   fullscreen on the device; adjust `seq4h.input`.
4. Try the ambient starting set on the FM1 and tune `STYLE_SET` /
   `STYLE_DEF` in `src/gen.c` by ear (Roy's main style is ambient).
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

### Hardware results (Roy's RG40XX-V, 2026-11-19)

- Knulli on Batocera 42, kernel **4.9.170** (`#14 SMP PREEMPT Thu Dec 25
  12:29:54 CST 2025`), aarch64. Same kernel line as the 2024 H700 SDK, so
  `snd-usb-audio` may still be missing; the next check log answers it.
- `/sys/devices/platform/soc/usbc0/otg_role` reads **`usb_host`**: the USB-C
  port is already in host mode with nothing plugged in.
- The first `USB-MIDI-CHECK.sh` stopped right after that line. Its next read
  was `usbc0/usb_host`, a sunxi control attribute, which blocked (or the
  launcher killed the script) and nothing after it reached the log. v2 of the
  script (`script: USB-MIDI-CHECK v2` at the top of the log) no longer reads
  that file, puts every command under a time limit, has fallbacks for
  missing tools (`lsusb`, `zcat`, `aconnect`, `amidi`, `modprobe`), reads USB
  devices and interfaces from `/sys/bus/usb/devices`, and syncs the log after
  every section.

### Code changes for this

- `src/midi_alsa.c` now **falls back to raw MIDI** (`/dev/snd/midiC*D*`) when
  the ALSA sequencer is missing, so only `snd-usb-audio` is needed.
  `--midi list` shows both kinds (`seq` and `raw` lines); `--midi` matches
  either name, e.g. `--midi FM` or `--midi hw:1,0,0`.
- **Hot-plug:** if the FM1 is not there at start, or is unplugged, seq4h keeps
  running, shows `MIDI: WAITING` in the header, and reconnects within about a
  second when it appears. On reconnect it sends a note-off for every held and
  last-played note, then All Sound Off (CC120) and All Notes Off (CC123) on
  each channel, so a note left hanging while unplugged stops. If playing, it
  keeps its position and re-sends Stop + Start at the next bar line, so the
  FM1's clock is back in phase without the pattern jumping back to step 1.
  (Roy's test, 2026-11-19: Start/Play/Stop worked; unplugging mid-play left
  the current note hanging, and replugging glitched before resyncing. The old
  code restarted from step 1 on reconnect and sent no CC120. Both fixed.)
- **Header status:** top right shows `MIDI: OK` (green) with the port name,
  `MIDI: WAITING` (yellow) with the name it looks for, or `MIDI: NONE`. Each
  connect and disconnect is written to `seq4h/seq4h.log` as
  `seq4h: MIDI connected to ...` / `seq4h: MIDI WAITING FOR ...`. (Roy's
  2026-11-19 photo showed no status; the header had stopped drawing it.)
- **Clock master:** MIDI clock now runs continuously, also while stopped, so
  the FM1 follows seq4h's tempo before Start. Start/Stop as before.
- **`--midi-test`** plays C4, E4, G4, C5 on channels 1-4 in turn, then all four
  together, then sends All Notes Off and exits.
- `knulli/USB-MIDI-CHECK.sh`: a Ports-menu diagnostic that logs USB role,
  devices (with interface class and bound driver), kernel sound config,
  modules, ALSA cards, raw MIDI and sequencer ports to
  `seq4h/usb-midi-check.log`. Every section runs even if one fails, and the
  last line of a complete log is `--- end`.
- `knulli/SEQ4H.sh` logs to `seq4h/seq4h.log`, runs `modprobe` for the USB
  MIDI modules, and loads `.ko` files from `seq4h/modules/` if you add them.
- CI (`.github/workflows/seq4h.yml`) runs `knulli/build.sh` and uploads
  `seq4h-knulli-aarch64` (see *aarch64 build*).

### Test checklist

Fill in the results table as you go. Tick each box or note what happened.

**A. Install**

- [ ] Download the `seq4h-knulli-aarch64` artifact from the latest
      *seq4h aarch64 build* run on GitHub Actions (or run
      `knulli/build.sh`). Inside is `seq4h-knulli-aarch64.zip`.
- [ ] Unzip `seq4h-knulli-aarch64.zip` into `/userdata/roms/ports/` (SHARE
      partition `roms/ports`, or over SSH/SMB). You get `SEQ4H.sh`,
      `USB-MIDI-CHECK.sh` and `seq4h/`. The zip keeps the execute bits; if
      you copied the files some other way, run `chmod +x` on both scripts and
      `seq4h/seq4h`.
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
- [ ] Turn on M > BUTTON CODES (or start SHOW-BUTTONS from Ports) and press
      every button. The top right shows the code and its action
      (`JOY 10 = START`); `seq4h.log` lists them and the pad's name and
      GUID. Note any code that differs from the table in README "Button
      mapping" and fix it in `seq4h.input`.
- [ ] Face buttons: on the grid A toggles a step, X opens the STEP page,
      Y locks, B returns to the grid.
- [ ] L1/R1 change page, L2/R2 change track.
- [ ] A short press of M opens the main menu and doesn't trigger a Knulli
      hotkey; M or B closes it. M should show as joy 11.
- [ ] Select+Start quits and the pattern is saved (`seq4h.pat` updated).

**Results**

| Item | Result |
|---|---|
| Knulli version / kernel | Batocera 42 base, kernel 4.9.170 (2026-11-19) |
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
| Button mapping, L2/R2 | stock table wrong; old-build report fits the Knulli table for every button; recheck on new build |

### If the driver is missing

Build `snd-usb-audio.ko`, `snd-usbmidi-lib.ko` and `snd-hwdep.ko` (and
`snd-seq*.ko` if wanted) against the exact Knulli kernel, the way the m8c port
does: Knulli's `rg35xx-plush-sdk` aarch64 toolchain, the kernel source and
`linux-sunxi64-legacy.config`, set `CONFIG_SND_USB_AUDIO=m`, build modules.
Put the `.ko` files in `/userdata/roms/ports/seq4h/modules/`; `SEQ4H.sh` loads
them. The `vermagic` must match `uname -r` on the device. This is not
automated yet; it is the next job if check B shows the driver is absent.
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
- **Screenshots**: twelve UI states (all four pages) driven by `--keys` with
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

### Harness add-ons for the hardware checklist

These slot into the test checklist above and turn "it sounded fine" into
numbers. `tools/midicheck.py` runs on any PC with Python 3; copy capture files
off the handheld with `scp root@<handheld-ip>:/tmp/h.cap .`.

- **After B (no FM1): does the H700 keep time?**
  `./seq4h --midi capture:/tmp/h.cap --demo --headless 20`, then on a PC
  `tools/midicheck.py h.cap --demo --duration 20 --tolerance-ms 2 --max-ms 8`.
  All PASS means the timing thread holds up on the handheld. Record the
  "note timing error" and "clock error" lines.
- **During E: record what reached the FM1.** Add `--capture /tmp/fm1.cap` to
  the seq4h command (it tees, so the FM1 still gets everything), then run
  `tools/midicheck.py fm1.cap --bpm N` (N = the tempo you played at; add
  `--demo` if you didn't edit the pattern). This separates "seq4h sent the
  wrong thing" from "the FM1 did something unexpected with it".
- **During G:** the same with `--capture` on the 30-minute run (a few MB).


## aarch64 build

Decided 2026-11-19. `knulli/build.sh` is the single build path, used locally
and by CI:

- **Cross-compile, not emulate.** An Ubuntu 20.04 container
  (`knulli/Dockerfile`) with `gcc-aarch64-linux-gnu` and the arm64
  `libsdl2-dev` and `libasound2-dev` from ports.ubuntu.com (multiarch). The
  first CI version ran a native build in an emulated arm64 Debian container,
  which was slow; Debian bullseye was also dropped there because its security
  archive is gone.
- **glibc 2.31 floor.** Ubuntu 20.04's glibc is older than any Knulli release.
  In practice the binary needs only `GLIBC_2.17` symbols.
- **Dynamic SDL2 and ALSA.** It needs `libSDL2-2.0.so.0`, `libasound.so.2`,
  `libpthread.so.0` and `libc.so.6`, which Knulli ships. Using the device's
  SDL means Knulli's own video and input backends are used. The code only
  calls SDL functions from 2.0.5 or older, so the device's SDL version does
  not matter. If a future Knulli lacked one of these, bundle the `.so` next to
  the binary and set `LD_LIBRARY_PATH` in `SEQ4H.sh`.
- `-mcpu=cortex-a53` for the H700. The script fails if the result is not
  aarch64 or lacks ALSA.
- **Tests:** unit tests natively, the same tests built for aarch64 under
  `qemu-aarch64-static`, then the real binary draws a frame with
  `SDL_VIDEODRIVER=dummy`. This proves it links and runs against arm64 SDL,
  not that it works on the handheld.
- **Reproducible:** `-ffile-prefix-map`, a stripped binary and a zip with
  fixed timestamps give identical SHA-256s on every run.
- **Package:** `dist/seq4h-knulli-aarch64.zip` in the Ports layout. CI
  uploads the zip inside the artifact, because GitHub artifacts drop the
  execute bit.
- Behind a proxy that blocks plain HTTP, the apt sources need switching to
  https (the cloud build container needed this; GitHub's runners do not).

## Handy commands

```sh
cd sequencer
make test                      # engine and generator tests
make check                     # whole hardware-free harness, results in out/
make                           # needs libsdl2-dev and libasound2-dev
knulli/build.sh                # aarch64 binary + Ports zip in dist/ (Docker)
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
- `src/midi_capture.c`: capture-to-file and tee MIDI backends
- `tools/render.c`: offline renderer (`seq4h-render`)
- `tools/midicheck.py`, `tools/shotcheck.py`: capture and screenshot checks
- `tests/test_engine.c`: engine unit tests
- `tests/test_gen.c`: tests for the newer generators, transforms and evolution
- `src/input.c`, `input.h`: button mapping and the `seq4h.input` file
- `tests/test_input.c`: mapping tests
- `knulli/seq4h.input`: RG40XX-V button map
- `tests/harness.sh`, `tests/test_midicheck.py`, `tests/fixtures/`: harness
- `knulli/SEQ4H.sh`: Ports launcher script
- `knulli/SHOW-BUTTONS.sh`: Ports entry that starts seq4h with button codes shown
- `knulli/USB-MIDI-CHECK.sh`: USB/MIDI diagnostic for the device
- `knulli/build.sh`, `knulli/Dockerfile`: aarch64 cross-build and packaging
