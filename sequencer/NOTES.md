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
- **Mute and solo from the grid** (Roy, 2026-11-19, after testing build #68):
  pressing left from step 1 puts the grid cursor on the track label column
  (T1-T4). There A toggles mute, B toggles solo, up/down change track and
  right goes back to step 1. Only on the label does B mean solo; everywhere
  else B is still "back to grid". Several tracks can be soloed; while any
  track is soloed only soloed, unmuted tracks play, and notes held by tracks
  that go silent get a note-off at once (`seq_track_audible`,
  `release_silenced` in `src/engine.c`). Labels show an M badge (white) and
  an S badge (yellow); muted tracks are grey, tracks silenced by someone
  else's solo are dimmed. The TRACK page MUTE row shows the same state
  (`MUTED`, `OFF, SOLO`, `OFF, NOT SOLO`). **Mute is saved, solo is not**
  (decision made without asking): solo is a temporary listen, and a saved
  solo would make tracks seem broken the next time seq4h starts.
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
- **Techno and Melodic sets** (2026-11-19, tuned to match the ambient set):
  `TrackPreset` gained `shape` (a finishing pass, `style_shape()` in
  `src/gen.c`) and per-track generator parameters (`setp`/`p`). Shapes write
  accents, gates and chord-aware notes into every step, so a roll (which only
  changes on/off and notes) or evolution keeps the feel, and use locks to pin
  the core: techno's offbeat hats; melodic's bass roots on each chord change
  and the resting halves of the call (T3) and answer (T4). Techno: 128 BPM, A
  phrygian, kick EUCLID 4 (no evolution), hats EUCLID 4 rotate 2 plus ghosts
  (rhythm/chance/level drift), bass FOLLOW T1 GAPS with root/octave cells and a
  turnaround, stab TURING 16 (slow REGEN). Melodic: 100 BPM, D dorian, 32
  steps, a progression picked from `PROGS` (from a fresh start: Dm for a bar, then C;
  NEW FROM STYLE picks again), bass and arp are EUCLID with PITCH KEEP so rolls keep chord tones,
  call/answer are MOTIF. Ambient is untouched (a test pins its first-run
  pattern hash). The engine has no ratchets, so techno has none; adding them
  would be an engine change (step field, player, save format). Tests:
  `test_style_presets` in `tests/test_gen.c` (deterministic, in key, kick and
  hat positions, bass avoids the kick, arp and strong call notes on chord
  tones, answer copies the call's rhythm, all still true after 32 bars).
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
- **BLANK style** (Roy, 2026-11-19, after testing build #84): a fourth
  `Style`, `STYLE_BLANK` (appended, so saved style numbers stay valid).
  NEW FROM STYLE with BLANK clears all four tracks to MANUAL, 16 steps, every
  step off (notes preset to C2/C3/C4/C5 snapped into the scale, vel 100, gate
  50, prob 100), evolution off, no anchor, chain off, mute and solo off.
  Decisions made without asking: BLANK **keeps the current BPM, swing, scale,
  root and MIDI channels** (so a blank page fits whatever key you were in;
  set them on the TRACK/GENERATE pages), and **keeps stored slots** (they are
  your saved work; overwrite them with SAVE TO SLOT). keep_manual is ignored:
  BLANK is a full start-over. As a style for picking a generator, BLANK uses
  the table defaults like MELODIC.
- **Saving survives a kill** (Roy, 2026-11-19: after quit and relaunch the
  current pattern came back but the stored slots did not). The save format
  already held every slot, the chains, per-track generator and mute; the
  file was only written when `ui_run` returned normally. Most likely the
  process was being killed (Knulli/Batocera's exit hotkey on Ports kills the
  process; SDL turns SIGTERM into a normal quit, SIGKILL leaves no chance),
  so nothing was written, and because `seq_init` seeds the RNG with a fixed
  value the first-run ambient set looked like "the last pattern". Now: the
  UI autosaves two seconds after any button press and once a minute
  (`autosave()` in `src/ui.c`), only writing when the file contents changed,
  copying the state under the player lock and writing outside it so MIDI
  timing is untouched. Every write goes through `seq_save_file()`: write
  `seq4h.pat.tmp`, `fsync`, rename, so a power cut leaves the old or the new
  file, never half. `seq4h.log` gets `loaded ... (N stored slots)` at start,
  `autosaved ...` on the first autosave and `saved ...` at quit, so a log
  shows whether a save happened. Format unchanged (`seq4h 1`); old saves
  load as before. Tests: `test_save_all_slots` (all 32 slots, chains,
  generators, evolution, mutes; save-load-save is byte-identical; old file
  loads) and the harness `persist` section (real binary, slots stored by
  replayed keys, SIGKILL, then relaunch and quit gives the same file).
- **More ambient variety** (Roy, 2026-11-19, overnight list item 3). The
  AMBIENT style and its starting set are unchanged and stay the first-run
  default; everything new is opt-in or in the new DRONE style.
  - Generators `GEN_DRONE`, `GEN_DRIFT`, `GEN_BELL`, appended after MOTIF so
    saved type numbers stay valid (see README for what they do). They carry
    ambient-leaning defaults in the `GENS` table, and `style_defaults` only
    applies `STYLE_DEF` to EUCLID..MOTIF, so the other style threads never
    need rows for them. DRONE and DRIFT read their starting pitch from the
    track's current first note (like WALK), so no extra generator state is
    saved. `gen_apply` can now also set hold, gate and probability per step.
  - Playback colour on `SeqTrack`: `chord`, `swell`, `swell_depth`, `rest`
    (GENERATE rows CHORD, SWELL, SWELL DEPTH, RESTS, at the bottom of the
    list so earlier row positions and the harness key sequences stay put).
    They change what is sent, never the step grid. Chords are the only
    polyphony: the engine keeps the chord notes of the sounding note
    (`voice[]`) and of the last note (`last_voice[]`), so retrigger, stop,
    mute/solo and `seq_panic` release them all. Chord notes are sent at 80%
    velocity on the track's own channel. Decisions made without asking:
    chord intervals fall back to the other third/seventh or the nearest scale
    note when not in the scale; swell is a triangle offset a quarter period
    per track; a rest lasts 1-4 bars and is always followed by at least one
    played bar; rests let held notes ring out; the running rest is not saved
    and `seq_start` clears it.
  - **Not verified on the FM1:** whether a Felucca track plays several notes
    at once on its channel. If it is monophonic, a chord track will just
    play its top note; turn CHORD off on that track.
  - Evolution every 16, 32 or 64 bars (`SEQ_MAX_EVERY`), and AMOUNT moves
    in 1% steps below 10%.
  - `STYLE_DRONE` (appended after BLANK): A dorian, 66 BPM, DRONE bass with
    FIFTH (32 steps, regen every 32 bars), DRONE pad with TRIAD (28 steps,
    every 16), DRIFT (20 steps, every 16, rests 10%), BELL (27 steps, pitch
    and velocity drift every 4 bars, rests 15%), slow swells on the first
    three. It uses AMBIENT's generator defaults. Tune by ear.
  - Save format still `seq4h 1`: new `pc t chord swell depth rest` line per
    track. Old files load with all of it off; old binaries ignore the line
    and clamp an EVERY above 8 to 8.
  - Tests: `tests/test_ambient.c` (generators, chord notes in every scale,
    chord playback and release, swell curve, rests, 64-bar evolution, save
    and load, AMBIENT unchanged, DRONE style for 1000 bars with no hung
    notes). Harness: 256-bar renders of the AMBIENT and DRONE starting sets
    (`seq4h-render --style NAME`; per-style colour in `STYLE_COLOUR`, so the TrackPreset rows are untouched; `midicheck.py --max-hold N` allows held
    notes, `--poly` for chords) and four DRONE screenshots.
- **Slot names and the SLOTS page** (Roy, 2026-11-19: "a simple way to name
  and pick saved slots from the M menu"). M > SLOTS lists the **current
  track's** 8 slots (slots and chains are per track, so a per-track list
  matches what CHAIN plays; L2/R2 switches track on the page). A load (undo
  with X on GENERATE), X save here, Y rename, Select opens a COPY TO /
  CLEAR SLOT / BACK panel. Decisions made without asking: a second press
  confirms only what loses work (X onto a full slot, copying onto a full
  slot, clearing); loading is immediate because it can be undone. Copy takes
  the name along; clear resets the name. Select was free (only Select+Start
  quits), so it opens the panel. Name editor: 8 characters from
  ` A-Z 0-9 -`, up/down letter, left/right move, X space, A keep, B cancel;
  Start still plays/stops and M cancels and closes the menu. Names live in
  `SeqSlot.name` ("" = default "SLOT n") and are saved as `sn T K NAME`
  lines (space as `_`), only for named slots, so the format stays
  `seq4h 1`: older builds skip the line, older files load with default
  names. The GENERATE page's SLOT row shows the name too. Tests:
  `test_slot_names` (names, trimming, copy, clear, save/load, old file),
  harness screenshots `slots-*` and a persist check that a rename made on
  the page is autosaved. Not yet tried on hardware.
- **Note-length bars and offsets** (Roy, 2026-11-20; items 1 and 2 of
  *Piano roll, drum grid, note lengths and offsets* below).
  - Grid: a held note is drawn as a bar through the steps it lasts for (in a
    dimmer track colour), ending part-way through the last step at its gate,
    or at the next note on the track, which cuts it off. It wraps round the
    pattern end as playback does. The "+N" label stays. The help line under
    the grid shows hold as `GATE 50 +2`.
  - Offset: `SeqStep.nudge`, -11..+11 engine ticks (24 ticks per step), set
    with the STEP page OFFSET row (shown as EARLY/LATE n% of a step; hold Y
    for 4 ticks at a time). A step plays at grid time + swing + offset, so a
    step is due either in its own 1/16 or, when early, in the one before
    (`seq_tick` checks the current and the next step each tick, and plays
    both if both are due). An early step 1 plays at Start the first time.
    Gate and hold count from where the note really starts. MIDI clock is
    unchanged. On the grid the block moves left or right inside its box.
  - Save: `o T STEP N`, `so T SLOT STEP N`, `ao T STEP N` after the step's
    line, written only for non-zero offsets; older builds skip them, older
    files load with no offsets; still `seq4h 1`.
  - Rolls reset offsets on unlocked steps (generators write whole steps);
    evolution, slots, undo and the anchor keep them.
  - Known limit: an early step 1 in a new bar is read just before the bar
    line, so on the bar where a chain moves to the next slot (or evolution
    changes step 1) it plays the old bar's step 1.
  - Tests: `test_nudge` (late, early, early first step, swing + offset, two
    steps due on one tick, gate from the real start, unchanged playback
    without offsets, save/load with slots and anchor, clamping); harness
    screenshots `step-offset` and `grid-hold-offset`.
- **Shuffle, never on grid, spread, EVO OFFSET** (Roy, 2026-11-20: "hihats
  on every 16th, none of them on the grid or in the same place").
  - Live shuffle: TRACK page `SHUFFLE` (0..11 ticks, `SeqTrack.shuffle`) and
    `NEVER ON GRID` (`SeqTrack.offgrid`, keeps notes >= `SEQ_OFFGRID_MIN` = 2
    ticks off the grid). Each step plays at a pre-drawn offset `jit[step]`
    (its OFFSET plus random +/-shuffle, clamped to +/-11); right after it
    plays, the offset for its next pass is drawn, so the early look-ahead
    always knows when a step is due. `played[step]` (1 + the 1/16 number it
    last played on) stops a step playing twice in one pass when the redraw
    moves it. Draws only use the RNG when shuffle or NEVER ON GRID is on, so
    patterns without them play exactly as before (ambient hash unchanged).
    Saved as `sf T SHUFFLE OFFGRID`, written only when set.
  - `XF_SPREAD` transform: deals every unlocked note an offset from a
    shuffled deck of -A..-2 and 2..A (BY = A ticks; below 2 means 11, the
    most), so no two notes share an offset until all are used (22 offsets
    at the most). `XF_GRID` ("ON GRID") sets unlocked offsets back to 0.
  - `EVO_OFFSET` (bit 64, `EVO_ALL` now 127; GENERATE row `EVO OFFSET`):
    moves a step's offset 1-3 ticks either way; with NEVER ON GRID it hops
    over the grid instead of landing on it. The GENERATE page has one more
    row, so the harness key sequences that walk past it gained a Down.
  - `tools/midicheck.py --offset N [--min-offset M]` accepts notes up to N
    ticks off the grid (still on an engine tick) and at least M off it; the
    harness renders `tests/fixtures/shuffle.pat` (shuffled never-on-grid
    hats and spread notes with EVO OFFSET) for 128 bars with it. Tests:
    `test_shuffle`; screenshots `track-shuffle`, `generate-spread`,
    `grid-after-spread`.
- **Piano roll and drum grid** (Roy, 2026-11-21: "lets sort the piano roll
  an drumpad"; items 3 and 4 of *Piano roll, drum grid, note lengths and
  offsets*).
  - Opening: Select on the GRID, acted on when Select is let go and only if
    no other button was pressed while it was held (`Ui.sel_used`), so
    Select+Start still quits. Opens `P_ROLL` on a note track and `P_DRUMS`
    on a drum track; B or Select goes back; L2/R2 switch track and pick the
    view that fits it; L1/R1 count from GRID. The GRID tab stays lit.
  - Piano roll: rows are the scale's notes (`gen_note_degree` /
    `gen_degree_note`), 14 at a time, scrolling to follow the cursor; root
    rows lighter. Bars are gate + hold long, cut at the next note on the
    track (no wrap drawn), shifted by OFFSET; off-scale notes outlined; chord
    notes faint. Controls as the roadmap: A note on/off, hold Y left/right
    length (quarters up to a step, then whole steps of HOLD), hold Y up/down
    moves the note a degree, hold X left/right offset, hold X up/down
    velocity. Y and X alone do nothing there. No save-file change.
  - Drum track: `SeqTrack.type` (TRACK page TYPE, row 1; the rows below
    moved down one, so harness key sequences for TRACK rows gained a Down)
    and `SeqDrums` (velocity 0 = no hit, and chance, per lane and step) in
    the track, its anchor and every slot. The note steps are kept while a
    track is a drum track and the other way round. The engine plays every
    lane hitting on a due step as a one-shot of `SEQ_DRUM_GATE` (12 ticks)
    on the lane's note, several at once on the channel; two lanes on the same
    note retrigger it. Drum steps use swing and live shuffle but no OFFSET
    (`draw_offset` uses 0 as the step's own offset). Per-lane lock
    (`drum_lock`), not per step.
  - DRUM MAP: `Seq.lane_note` / `lane_name`, one map for all drum tracks,
    GM defaults at first (36 38 42 46 39 37 45 49); now the FELUCCA preset
    (see the next entry). M > DRUM MAP: left/right changes a note and plays
    it, A plays it. `seq_audition` plays a note now and sends its
    note-off half a beat later from `seq_tick`, playing or stopped; the roll
    and drum grid also use it when a note or hit is placed while stopped.
  - Generator: DRUM KIT (`gen_drum_roll`, `src/drum.c`): per-style lane
    parts (hits per 16 steps from a start step, velocity and chance ranges,
    ghosts), scaled to the track length and BUSY (`drum_busy`, 0-100, 50 =
    as written). Three kits: ambient (also DRONE and BLANK), techno, melodic.
    `gen_roll`, `gen_evolve_bar`, `gen_transform`, `gen_set_anchor`, chain
    morph and `style_apply` hand drum tracks to `src/drum.c`. Evolution:
    RHYTHM moves one hit to a free neighbour (hit count kept), VELOCITY /
    PROB nudge hits, ROTATE, REGEN, anchor pull; min/max hits are not used
    on drums. Transforms INVERT, TRANSPOSE, SPREAD and ON GRID say "NOT FOR
    DRUM TRACKS". NEW FROM STYLE keeps the type and rolls a kit; BLANK
    empties the lanes. No style makes a drum track by itself, so the ambient
    set Roy likes plays exactly as before (ambient hash unchanged).
  - Save: `dm LANE NOTE NAME` (map), `ty T TYPE LOCKS BUSY`, and one line
    per hit: `dh T LANE STEP VEL PROB`, `dsh T SLOT ...`, `dah T ...`.
    Older builds skip them; older files load as note tracks with the GM map.
    Still `seq4h 1`.
  - Known limits: FOLLOW reads another track's note steps, not its drum
    lanes; on a drum track the main grid's A/X open the drum grid rather
    than editing in place.
  - Tests: `tests/test_drum.c` (two lanes at once, gates, chance 0, the map
    read live, stop and type switch release notes, shuffle never on grid,
    mute, save/load of type, lanes, locks, busy, slots, anchor and map, old
    files, lane names, techno kit, locked lanes, BUSY, RHYTHM keeps counts,
    anchor pull, REVERSE twice, styles, audition), also under ASan/UBSan;
    harness render `render-drums-128-bars` (`tests/fixtures/drums.pat`,
    `--poly`) and screenshots `roll-page`, `roll-edited`, `roll-track2`,
    `track-type-drums`, `grid-drums`, `step-drums`, `drums-page`,
    `drums-edited`, `drums-label`, `drum-map`, `drum-map-rename`.
- **Fixes from Roy's test of build #127** (2026-11-21: stepping between
  steps while editing, the length of new notes, and drums "mapping to the
  wrong sounds and limited in the midi notes").
  - Step navigation: new buttons `prev` / `next` (`B_PREV`, `B_NEXT`),
    default on the analog stick's x axis (`axis 0-` / `axis 0+`, pad LEFTX,
    keyboard `,` / `.`). They move the step on every editing page (grid,
    STEP, TRACK, GENERATE, piano roll, drum grid), wrapping at the track
    length. On the STEP page holding X + left/right does the same from any
    row, so it works even if the stick's axis number turns out different
    (BUTTON CODES shows it). R1 from the piano roll or drum grid goes to
    STEP, and B there goes back to where you came from (`Ui.step_back`).
  - Piano roll: Y + left/right also sets `Ui.roll_len` (start 1.00 step),
    and A on an empty step writes a note that long (gate, then HOLD). Shown
    as NEW NOTES: LEN on the info line. Not saved.
  - Felucca's drum map, from its firmware source
    (github.com/hugelton/Felucca, `eng_drum.c`): the DRUM engine designs its
    8 lanes on KICK 36, SNARE 38, CLAP 39, HAT CL 42, HAT OP 46, TOM 45,
    RIM 37, BELL 56 (`DRUM_LANE_NOTE`), and plays every note 35..81 as one
    of 11 sounds, pitched by some semitones (`DRUM_GM`); notes outside play
    as their octave of 36..47. seq4h's default had 7 of the 8 already, with
    CRASH 49 (a cymbal) instead of BELL 56. The new default is the FELUCCA
    preset; GM stays as a second preset. `DRUM_GM` is copied into
    `src/drum.c` (`seq_felucca_sound`, `seq_felucca_next_sound`).
  - So "wrong sounds" with the right notes most likely means the FM1 track
    on that channel is not on the DRUM engine (Felucca's MIDI IN CH1-4:
    channel n plays FM1 track n). DRUM MAP now says which channel the drums
    play on and that this FM1 track must use DRUM.
  - DRUM MAP page: rows show note, Felucca sound and semitones; hold Y +
    left/right jumps to the next Felucca sound; last row PRESET
    (FELUCCA/GM, A twice applies, "(IN USE)" when it matches); rename moved
    from Y to X (Y is now the hold modifier).
  - Save: a `dmv 2` line before the `dm` lines. A file with `dm` lines, no
    `dmv` and the untouched old GM map (saved by build #127) is moved to
    FELUCCA on load; a changed map is kept. Older builds skip `dmv`.
  - Tests: `test_drum` (`test_map`, `test_map_next`, `test_map_migrate`);
    screenshots `drum-map-next-sound`, `drum-map-preset`, `step-next-x`,
    `step-next-stick`, `roll-new-length`.
- **NEW FROM STYLE track by track, more styles, sparse ambient drums, roll
  locks** (Roy's feedback on build #133, 2026-11-21, PR 29):
  - The old NEW FROM STYLE silently kept MANUAL tracks with notes (that is
    the "one track stays, three are new" Roy saw). Now M has four rows T1-T4
    under MUSIC STYLE, each KEEP AS IT IS, BLANK, or a style's part; MUSIC
    STYLE sets all four. Parts are the style's own track presets
    (`STYLE_SET[style].t[part]`), so part 2 of TECHNO can go on track 3.
    `style_apply_tracks` (`src/gen.c`) does it; `style_apply(keep_manual)`
    still exists for the first run and the renderer.
  - Tempo rule (decision made without asking): when every track is replaced,
    MUSIC STYLE sets tempo, swing, scale and root; if any track is KEPT all
    four stay, so new parts fit what is kept and a BPM you set isn't lost.
  - Each track remembers its style and part (`SeqTrack.style`, `.part`,
    saved as `ts T STYLE PART`; files without it take the `style` line for
    every track). The track's style picks its generator defaults on GENERATE
    and its drum kit. Older builds skip the `ts` line.
  - New styles HIP HOP (88, G minor, swing 6), DRUM N BASS (172, A minor),
    HOUSE (124, F dorian, swing 2), TRIP HOP (80, D minor, swing 3), each
    with a drum part on T4 and its own kit. Kits now take written 16-step
    patterns with up to 3 variants per lane (`LanePart.pat` in `src/drum.c`),
    so boom bap and two-step are possible; BUSY below 50% drops off-beat
    hits. A DRUMS part on a note track (`SH_DRUM_NOTES`, `gen_drum_notes`)
    plays the kit's main hit per step as DRUM MAP notes; a roll there rolls
    a new kit pattern. New shape `SH_CHORD_ROOTS` puts notes on the chord
    roots of the progression (hip hop keys, house chords, with CHORD 7TH).
    Generator defaults: TRIP HOP as AMBIENT, DNB and HOUSE as TECHNO,
    HIP HOP as MELODIC.
  - Ambient "busier than at first": checked against build #68's code
    (commit 743e820). The four ambient note tracks are unchanged: same
    presets and generators, the first-run hash `AMBIENT_HASH` still matches,
    and over 50 seeds x 64 bars the note counts per bar are identical
    (2.2, 3.9, 5.3, 1.7). The difference is the drum track: with T4 a DRUM
    track, NEW FROM STYLE rolled the ambient kit, 5.3 hits a bar against
    1.7 notes for the old T4. The ambient kit is now about 1.8 a bar
    (kick most bars on the one, two soft closed hats, open hat, rim, tom
    and bell all at 10-45% chance, spread over the track's 23 steps so it
    drifts against the others); `test_ambient_kit_sparse` keeps it under 3.
    The other part of "busier" was the kept MANUAL track. FM1 sounds may
    still play a part.
  - Piano roll: Y pressed and let go on its own locks / unlocks the step
    under the cursor (Y with a direction is still length / move note).
    A locked note keeps its yellow corner; a locked step's column gets a
    dim yellow frame.
  - Tests: `test_style_tracks`, `test_new_styles`, `test_ambient_kit_sparse`
    (`tests/test_gen.c`); renders `render-hip-hop` etc.; screenshots
    `menu-mixed`, `grid-mixed`, `menu-hiphop`, `grid-hiphop`,
    `drums-hiphop`, `grid-dnb`, `roll-locked`.
- **Mute and solo on the drum grid** (2026-11-21, Roy: "a missed
  opportunity to be able to mute and solo on the drum grid as well as the
  grid"). Per drum lane, the same as the grid's track mute/solo (PR 10):
  left of step 1 puts the cursor on the lane name, **A mute, B solo**. The
  lane's audition moved from A to **X**; **Y** still locks. B on the lane
  name is solo, so Select (or right, then B) goes back to the grid.
  - Engine: `SeqTrack.drum_mute` and `drum_solo` (a bit per lane),
    `seq_lane_mute_toggle`, `seq_lane_solo_toggle`, `seq_lane_audible`.
    `play_drums` skips silent lanes; a toggle sends note-off for a lane
    that just went silent. Track mute/solo still apply on top. Generators,
    evolution and slots ignore mute/solo (a muted lane still evolves).
    BLANK clears both.
  - Save: `ty T TYPE LOCKS BUSY MUTES` (5th field; a track with lane mutes
    always writes the line). Lane solos are not saved, like track solos.
    Older files without the field load unmuted; older builds read the first
    four fields and ignore it.
  - Screen: grey M and yellow S badges under the lane name, silent lanes
    dim on the drum grid and in the drum track's row on the main grid.
  - Tests: `test_drum` `test_lane_mute_solo` (mute, solo, solo on a muted
    lane, hit released on mute, saved mutes, unsaved solos, old `ty` line);
    screenshots `drums-lane-mute`, `drums-lane-solo`.
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
- **TRS MIDI into the FM1 not working yet** (2026-11-21): USB cable → DIN → host box → FM1 works; DIN → TRS adapter → FM1 doesn't, most likely a Type B adapter. See *USB MIDI cable to the FM1's TRS in*.
- `knulli/SEQ4H.sh` launcher works on Roy's RG40XX-V (2026-11-19).

## Next steps (suggested order)

1. Run the short sheet for PR 29 (`tests/hardware-sheet-pr29.md`: per-track
   NEW FROM STYLE, the new styles, the sparser ambient kit, roll locks) and
   test 10 of `tests/hardware-sheet-133.md`, then add to the results table.
   Still open: a hub and the FM1 audio fluttering.
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
   - a JANGADA preset on M > DRUM MAP (FELUCCA and GM are built)

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

### Stopping the handheld charging the FM1 (researched 2026-11-19, not tested)

Roy doesn't want the RG40XX-V's battery spent charging the FM1 (the M-VAVE
FM-1 has its own 2000 mAh battery and takes power and data over one USB-C
port). Findings:

- **No supported switch.** The H700 Knulli kernel (4.9, sunxi USB) exposes no
  documented sysfs control to turn off the 5 V it puts out in host mode or to
  cap host current. `axp2202-usb/input_current_limit` is the *input* (charger)
  limit, not output. `otg_role` can't usefully be changed: writing it is
  blocked, and leaving host mode would stop MIDI anyway.
- **The only known hack is unsafe.** On the RG35XX SP (same SoC and PMIC), a
  Knulli user cut VBUS by clearing GPIO PI16 (`usb_drv_vbus`) through
  `/dev/mem` ([knulli-linux#112](https://github.com/knulli-cfw/knulli-linux/issues/112)).
  That is a raw register write, device-specific, unconfirmed on the
  RG40XX-V, and the OTG manager may switch it back. Not built, and no menu
  option: it could also stop the FM1 connecting at all, since many USB
  devices only enumerate when they see VBUS.
- **Practical options, safest first:**
  1. Charge the FM1 fully before a session. A full battery draws little, so
     the drain on the handheld is mostly the FM1 running, not charging.
  2. Use a **powered USB hub** (already the fallback): the hub's own supply
     feeds the FM1, and the handheld only talks data. Pick one that doesn't
     back-feed the host.
  3. A USB-C "data only"/power-blocker adapter would stop the charging, but
     the FM1 may then not connect; try only as an experiment.
- **Measuring it:** run `USB-MIDI-CHECK.sh` (v3 has a read-only *power*
  section: each power supply's status, current and voltage, any VBUS
  regulator, and what each USB device asks for in `bMaxPower`) with and
  without the FM1 plugged in. seq4h's battery lines in `seq4h.log` give the
  drain over a session; compare a run with the FM1 freshly charged against
  one where it was low.

### USB MIDI cable to the FM1's TRS in (Roy, 2026-11-21)

Goal: the FM1 runs on its own battery or charger, so the handheld stops
charging it. Roy has a generic USB MIDI cable (USB on one end, 5-pin DIN
plugs marked IN and OUT on the other) and a DIN to 3.5 mm TRS adapter.

**What happened (2026-11-21):**

- Handheld → powered "USB MIDI host" box: the box's host light stayed off and
  seq4h showed `MIDI: WAITING`. That can never work. The box is a USB *host*
  and so is the handheld, and two hosts can't talk to each other. A host box
  is for plugging USB MIDI *devices* (like the FM1) into DIN gear.
- Handheld → USB MIDI cable → DIN → TRS adapter → FM1 TRS MIDI in: no sound
  with either DIN plug.
- **Works:** handheld → USB MIDI cable → its DIN OUT plug → the host box's
  DIN in → host box USB → FM1. So seq4h does find the cable and sends through
  it, and the cable's OUT plug is the one that carries seq4h's MIDI. The host
  box powers the FM1, so the handheld no longer charges it. In this chain the
  FM1 gets MIDI over USB from the box, so **set Felucca's clock source back
  to USB** (HOME > MIXER > CLOCK).
- So the part that fails is DIN → TRS adapter → FM1. The FM-1's TRS in is
  **Type A** (the Roland/Arturia/Make Noise wiring). Users who tried both
  types report that Type B adapters (Korg/Akai/Novation) don't work
  ([Elektronauts](https://www.elektronauts.com/t/m-vave-fm-1/252170?page=7),
  [matrixsynth](https://www.matrixsynth.com/2026/10/how-to-hook-up-your-m-vave-fm-1-to.html)).
  Felucca's MIDI menu has no input-port setting (MIDI IN only picks channels:
  CH1-4 by default, or TRACK), so there is nothing in Felucca to switch on
  for TRS. **The most likely cause is a Type B adapter, or one that isn't
  for this direction.**

**Checks for the TRS route (no SSH):**

1. **The adapter must be Type A.** Look for "TRS-A", "Type A" or "Roland" on
   the adapter or its listing. "Type B", "Korg", "Akai" or "Novation" means
   it won't work with the FM1. If it's unmarked, assume it's the wrong type.
   An adapter that comes with Korg, Novation or Akai gear is Type B. With a
   multimeter: Type A has DIN pin 5 to the tip, pin 4 to the ring and pin 2
   to the sleeve. Type B has pins 4 and 5 swapped.
2. **The adapter must have a female DIN socket and a male 3.5 mm plug.** The
   cable's DIN plugs are male. A TRS-to-DIN adapter made for a synth's
   *output* (male DIN plug) won't fit without a DIN coupler.
3. **Use the cable's DIN plug marked OUT** (proven by the host-box chain). On
   some cheap cables the labels are swapped, so if OUT does nothing with a
   known Type A adapter, try IN.
4. **Use a 3-pole stereo 3.5 mm plug.** A 2-pole mono plug won't carry MIDI.
5. **On the FM1:** MENU > MIDI > MIDI IN = CH1-4 (seq4h's channels 1-4).
   Clock source = TRS when MIDI comes in on TRS, USB when it comes from the
   host box.
6. **In seq4h:** M > MIDI OUTPUT should list the cable (often
   `USB2.0-MIDI` or `USB MIDI Interface`), show it with `*`, and say
   `USB MIDI, DRIVER SND-USB-AUDIO` under USB DEVICES SEEN. The header then
   says `MIDI: OK`. If the FM1 and the cable are both plugged in (through a
   hub), pick the cable here with A. The choice is kept for the next start.
7. **In the log** (`/userdata/roms/ports/seq4h/seq4h.log`, readable from the
   SD card or Knulli's network share): `seq4h: USB devices seen` lists each
   device with its ID, name, `MIDI` or `not MIDI`, and the driver. It's
   written at start and whenever something is plugged in or out. The next
   line is `seq4h: MIDI connected to ...` or `MIDI WAITING FOR ...`.

If a known Type A adapter still gives nothing, a ready-made DIN-to-TRS-A
cable or a USB MIDI interface with a 3.5 mm TRS-A out removes the adapter
from the chain. The host-box chain works now and can stay the setup.

### M > MIDI OUTPUT page (2026-11-21)

Lists the MIDI output ports (ALSA sequencer ports, or raw MIDI devices when
the kernel has no sequencer, the same order seq4h connects in) with
**AUTO (first found)** at the top. A picks one: it sends all notes off to the
old port, connects to the new one, and saves the name to `seq4h.midi` next
to `seq4h.pat`. The next start uses that port unless `SEQ4H_MIDI` / `--midi`
names another. AUTO stores an empty line. `*` marks the port in use. The
right side shows the connection, the chosen port, every USB device from
`/sys/bus/usb/devices` (hub, USB MIDI and driver, or "not a USB MIDI
device"), and a hint: nothing detected, not MIDI class, no driver, no port
yet, or OK (then swap DIN plugs / Type A / channels). seq4h has never
filtered by device name, so `WAITING` always means Knulli shows no MIDI
output port at all. Screenshots
come from `tests/harness.sh screenshots` (`midi-*`, which uses fake USB and
`SEQ4H_TEST_PORTS`).

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
- **Battery (2026-11-19, Roy asked):** the header shows `BAT 78%` between
  SWING and the play state, with `+` while the charger is in (status
  Charging or Full). Dim normally, amber below 20%, red below 10%; nothing is
  drawn if no battery is found (desktop). `src/battery.c` scans
  `/sys/class/power_supply/*/type` for `Battery` and reads `capacity` and
  `status` every 10 s on the UI thread, outside the player lock, with
  non-blocking opens, so a missing or slow file can't touch MIDI timing.
  `seq4h.log` gets a line at start, every hour, when the charger goes in or
  out, and at quit, e.g. `seq4h: 2026-11-19 21:00:05 battery 78%
  Discharging, 1h00m since start`; start and quit lines bracket the 30-minute
  test. `SEQ4H_POWER_SUPPLY_DIR` points it at a fake tree for tests
  (`tests/test_battery.c`, harness shot `bat17-grid`). Not yet checked on the
  RG40XX-V: the supply's name and whether Knulli reports `status`.
  **Fix (2026-11-19, Roy's report):** the first build showed `BAT 8%` while
  Knulli showed 38%. Knulli's number is not the kernel's `capacity`: its
  batteryplus daemon estimates it (by default from voltage) and writes it to
  `/tmp/battery.percent`. seq4h now shows that file's number when it exists,
  so it matches Knulli, and falls back to the kernel gauge. Both go in the
  log (`battery 38% Discharging (kernel gauge 8%)`), and at start seq4h logs
  every power supply (type, status, capacity, voltage_now, online) and the
  Knulli file. Caution: Knulli's voltage mode is reported to read high on
  other axp2202 devices ([knulli-linux#124](https://github.com/knulli-cfw/knulli-linux/issues/124)),
  so near empty trust the kernel gauge; `knulli-settings-set
  system.batteryplus.mode pmic` makes Knulli use the gauge too.
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

Plain steps, no SSH needed. Everything is done on the handheld, the FM1 and
a Mac. Tick each box or write what happened. Steps that need SSH are in
*Appendix: advanced checks (SSH)* at the end, and are optional.

**How to install a new build**

1. On the Mac, open the repo on GitHub and click **Actions**.
2. On the left, click **seq4h aarch64 build**. Click the newest run with a
   green tick whose branch is **main**.
3. Scroll to **Artifacts** at the bottom and click
   **seq4h-knulli-aarch64** to download it.
4. Unzip it. Inside is another zip, `seq4h-knulli-aarch64.zip`. Unzip that
   one too. (Safari may already have done the first unzip for you.) You now
   have `SEQ4H.sh`, `SHOW-BUTTONS.sh`, `USB-MIDI-CHECK.sh` and a `seq4h`
   folder.
5. Switch the handheld off and put its SD card in the Mac, or open the
   handheld's **SHARE** over the network.
6. Copy all of those into `roms/ports/`. When the Mac asks, choose
   **Replace** for every file. Replacing the `seq4h` folder's files is fine:
   your saved patterns (`seq4h.pat`) are not in the zip, so they stay.
   If you once edited `seq4h/seq4h.input` yourself, replace it anyway: an
   old one overrides the new button codes.
7. Eject the card, put it back and switch the handheld on.
8. On the handheld, press Start for the main menu, then **Game settings >
   Update game lists**. **SEQ4H**, **SHOW-BUTTONS** and **USB-MIDI-CHECK**
   appear under **Ports**.

**How to read the logs on the Mac**

seq4h writes `roms/ports/seq4h/seq4h.log`, and USB-MIDI-CHECK writes
`roms/ports/seq4h/usb-midi-check.log`. Quit seq4h first (Select + Start),
then open the SD card or SHARE on the Mac and open the file in TextEdit.
The newest run is at the bottom; each seq4h run starts with a
`=== ... seq4h start` line.

**Before you start**

1. Charge the FM1 fully first. The handheld powers the FM1 over USB and
   will otherwise spend its own battery charging it (see *Stopping the
   handheld charging the FM1*).
2. On the FM1, clear its own pattern: **REC > CLEAR**. Otherwise the FM1's
   built-in sequencer plays along with seq4h when Start is sent.
3. On the FM1, set **MENU > MIDI > MIDI IN** to **CH1-4**.
4. On the FM1, tap **HOME** until the **CLOCK** page shows, and set the
   clock source to **USB**. From then on the FM1's BPM follows seq4h.
5. Connect the FM1 to the handheld with a USB-C to USB-C data cable.

**A. Connection**

1. [ ] Start **SEQ4H** from Ports. Top right shows **MIDI: OK** in green
   with the FM1's name.
2. [ ] The FM1's BPM shows seq4h's tempo before you press Start.
3. [ ] Unplug the FM1: the header shows **MIDI: WAITING** in yellow. Plug it
   back in: **MIDI: OK** again within about 2 seconds.

**B. Playing**

1. [ ] Press **Start**: the four tracks play on FM1 tracks 1-4.
2. [ ] Press **Start** again: everything stops, no note hangs.
3. [ ] On the **TRACK** page change BPM (hold Y for steps of 10): the FM1's
   BPM follows within a second.
4. [ ] Try 200 BPM or more with busy tracks: steps sound even.
5. [ ] On the grid, press left from step 1 to reach the track label. **A**
   mutes the track, **B** solos it. The FM1 track goes silent or alone.
6. [ ] Unplug the FM1 while playing, then plug it back in. A note may hang
   while it is unplugged; it stops when you plug back in, and playing
   carries on in time.

**C. Buttons and screen**

1. [ ] Press **M**, choose **BUTTON CODES**, press **A**. Press every
   button: the top right shows the code and what it does, e.g.
   `JOY 10 = START`. Compare with the table in README *Button mapping*.
   (If M itself does nothing, start **SHOW-BUTTONS** from Ports instead.)
2. [ ] Fullscreen fills the screen and text is readable.
3. [ ] L1/R1 change page, L2/R2 change track, B goes back to the grid.
4. [ ] A short press of M opens the main menu without a Knulli hotkey
   firing; M or B closes it.

**D. Patterns**

1. [ ] M > **MUSIC STYLE** and **NEW FROM STYLE** give a new set for that
   style. Try AMBIENT, TECHNO, MELODIC and BLANK.
2. [ ] On the **GENERATE** page each track can have its own generator,
   including **MANUAL**.
3. [ ] Steps 17-32 are reachable on the grid (it shows 16 at a time).
4. [ ] Save a pattern to a slot, load it again, and set a chain.
5. [ ] Quit with **Select + Start**, start SEQ4H again: the pattern and
   slots are still there. Also make an edit, wait 5 seconds and switch the
   handheld off without quitting: after restart the edit is still there
   (autosave).

**D2. Note lengths, offsets and shuffle (2026-11-20 builds)**

1. [ ] On the grid, put the cursor on a note, press X, go to HOLD and set
   +2. Back on the grid (B), a bar runs from that note across the next two
   steps. Add a note inside the bar: the bar now stops there.
2. [ ] On the STEP page set OFFSET to LATE 25% on one note and EARLY 25% on
   another. The blocks on the grid move right and left. Play: the late note
   lands just after the beat, the early one just before. The FM1's tempo
   doesn't change.
3. [ ] Quit and restart: the offsets and holds are still there.
4. [ ] Shuffle: put hats on every step of a track. On the TRACK page set
   SHUFFLE to UP TO 25% and NEVER ON GRID to ON. Play: the hats sound loose
   and never quite repeat. Set SHUFFLE back to OFF: still never on the beat,
   but steady.
5. [ ] Spread: on GENERATE pick TRANSFORM SPREAD and press A. The blocks on
   the grid move to different places. Turn on EVO OFFSET (and EVOLVE every
   2 bars): the groove drifts slowly. TRANSFORM ON GRID puts it back.

**D3. Piano roll, drum grid and drum notes (build after PR 25, 2026-11-21)**

1. [ ] **Drums on the right FM1 track.** On the FM1, pick the track you want
   for drums and set it to Felucca's **DRUM** engine. Remember its number:
   FM1 track 1 listens on MIDI channel 1, track 2 on channel 2, and so on.
   In seq4h go to the **TRACK** page, pick a track, set **TYPE** to
   **DRUMS** and **MIDI CHANNEL** to that FM1 track's number.
2. [ ] **Drum map.** Press **M**, go down to **DRUM MAP**, press **A**. The
   right side says "PLAYS ON Tn, MIDI CHANNEL c" and "FM1 TRACK c MUST USE
   THE DRUM ENGINE": check c is your FM1 drum track. Go down to the last
   row, **PRESET**: it should read FELUCCA (IN USE). If not, press **A**
   twice. Now press **A** on each lane: KICK, SNARE, HAT CL, HAT OP, CLAP,
   RIM, TOM, BELL should each play that sound.
3. [ ] **Choosing other sounds.** On a lane, hold **Y** and press right a
   few times: it jumps to the next Felucca sound each time (rim, snare,
   clap, hats, tom, cymbal, bell, clave, conga) and plays it. Plain
   left/right moves one note (the same sound higher or lower, shown as +2,
   -4 ...). **X** renames the lane.
4. [ ] **Piano roll, new note length.** On the grid, put the cursor on a
   note track and press **Select** on its own. Hold **Y** and press right
   until the yellow **NEW NOTES: LEN** reads 2.00. Press **A** on a few empty
   spots: each new note is 2 steps long. Select goes back to the grid.
5. [ ] **Moving between steps while editing.** On the grid press **X** to
   open the STEP page. Push the **analog stick** left/right: the step number
   changes and the row you were on stays picked. Then hold **X** and press
   left/right on the d-pad: the same. (If the stick does nothing, open M >
   BUTTON CODES, move the stick, and tell me what it shows.) From the piano
   roll, **R1** goes to STEP and **B** comes back to the piano roll.
6. [ ] Select + Start still quits.
7. [ ] On the drum track press **Select**: the drum grid opens. Go to
   **GENERATE**, press **A** on the TRACK row to roll a kit, then Play:
   kick, hats and the rest sound together on the FM1. In the drum grid A
   adds and removes hits, hold X + up/down changes loudness, Y locks a lane.
8. [ ] Quit and restart: the drum track, its hits and the DRUM MAP are kept.

If a sound is still wrong after step 2, write down the lane and what you
hear instead, and send `seq4h.log`.

**D4. Mute and solo on the drum grid (2026-11-21)**

1. [ ] On the drum track press **Select** to open the drum grid and press
   **Start** to play. Press left until the cursor is on the lane names
   (yellow box on KICK).
2. [ ] Press **A**: KICK shows a grey **M**, its hits go dim and the kick
   stops on the FM1. Press **A** again: it comes back.
3. [ ] Press **down** to HAT CL and press **B**: a yellow **S**, every other
   lane goes dim and only the closed hat plays. Go to SNARE and press **B**
   too: snare and hat play together. Press **B** on both again: all lanes
   play.
4. [ ] **X** on a lane name plays its note once (this used to be A).
   **Y** still locks the lane.
5. [ ] Mute a lane, press **Select** to go back to the grid: that lane's
   thin bars are dim in the drum track's row. Quit and restart: the lane
   is still muted. A lane solo is not kept (like track solo).

**Full 20-test sheet for build #133** (2026-11-21, Roy asked for one sheet
that checks everything): `tests/hardware-sheet-133.md`. Tests 1-7 are the
PR 25 features (D3 above, with expected results), tests 8-20 recheck A-E
plus MIDI OUTPUT, BLANK, DRONE, generators, evolve, slots and autosave. Each
test has a PASS/FAIL box. Use it in place of A-E for this build, and copy
the results into the table below.

**E. Long run and power**

1. [ ] Note the battery % on both devices. Play 30 minutes at 120 BPM with
   evolution on. No drift, no stuck notes, no crash.
2. [ ] Quit, then on the Mac open `seq4h.log`. The `battery ...` lines at
   start and quit give the handheld's drain. Look for any `raw MIDI
   messages dropped` line.
3. [ ] Header `BAT nn%` matches Knulli's own battery reading.
4. [ ] Optional: a hub with power. Does the handheld charge and still see
   the FM1?

**F. Problem reports**

If something goes wrong, note what you pressed and what happened, then quit
and send `seq4h.log` (and `usb-midi-check.log` if MIDI never connects: run
**USB-MIDI-CHECK** from Ports first).

**Results so far (Roy's RG40XX-V and FM1, 2026-11-19 to 2026-11-21)**

| Item | Result |
|---|---|
| System | Knulli on Batocera 42, kernel 4.9.170 |
| USB-C role | `otg_role` = `usb_host` with nothing plugged in |
| Connection | direct USB-C to USB-C cable to the FM1 works; no hub needed |
| MIDI status | header shows MIDI: OK |
| Clock | works once the FM1's CLOCK page source is set to USB; then the FM1's BPM follows seq4h |
| Start / stop | works |
| Tempo changes | FM1 follows |
| 200+ BPM | steps even |
| Mute | works |
| Buttons | Knulli codes are 3 higher than stock; fixed (PR 8) and checked against the help on build #68 |
| FM1's own sequencer | also plays along on Start; clear it with REC > CLEAR |
| Unplug while playing | a note hangs until you plug back in, then it clears (expected) |
| Power | the handheld charges the FM1 over USB and drains its own battery; charge the FM1 first or use a powered hub |
| Audio | quiet fluttering noise on the FM1 output with sparse tracks; probably USB power or ground noise, not investigated yet |
| Styles, per-track generators, MANUAL, 32 steps, slots, chaining | work |
| Slots after quit | were lost: seq4h only saved on a clean quit; fixed by autosave (PR 14); slots name/load/copy/clear/restart all good on build #124 |
| Hold bars, offset, shuffle, spread, EVO OFFSET, BLANK, DRONE, battery % | good on build #124 (2026-11-21) |
| 30-minute run | good on build #124 (2026-11-21) |
| Piano roll (build #127) | works; needed moving between steps while editing and a length for new notes (fixed in PR 25) |
| Drum grid (build #127) | plays, but wrong sounds and the note list was hard to remap; PR 25 uses Felucca's own map (from its firmware), shows each note's Felucca sound, and says which FM1 track must be on DRUM |
| Build #133 (20-test sheet `tests/hardware-sheet-133.md`, 2026-11-21) | 19 of 20 PASS: 1-9 and 11-20. Test 10 (buttons vs help, short M) not run. Test 3 was ticked both ways; Roy confirmed PASS. D3 is done: DRUM MAP, Felucca lane sounds, presets, drum grid, roll length, stick and X+L/R stepping all good |
| Roy's feedback on #133 | NEW FROM STYLE kept one track and redid the other three (it kept MANUAL tracks with notes); he wants to choose all / one / several tracks and mix styles. Wants hip hop and drum n bass (more drum styles). AMBIENT felt busier than the first version. Wants step locks in the piano roll. All four done in PR 29 (sheet `tests/hardware-sheet-pr29.md`) |
| PR 29 build | not tested yet: `tests/hardware-sheet-pr29.md` |
| Drum lane mute/solo (D4) | not tested yet |
| Hub | not tested yet |

### Appendix: advanced checks (SSH)

Optional. These need SSH into the handheld (`ssh root@<handheld-ip>`,
Knulli's default password is `linux`). The plain checklist above covers
everything else.

- **Port list:** `cd /userdata/roms/ports/seq4h && ./seq4h --midi list`
  shows the `seq` and/or `raw` lines for the FM1.
- **One note per channel:** `./seq4h --midi FM --midi-test` plays track 1
  C4, track 2 E4, track 3 G4, track 4 C5, then all four, and exits.
- **Raw MIDI fallback:** if the sequencer is a module, `rmmod snd-seq-midi`
  and run `--midi-test` again.
- **SEL mode:** `./seq4h --channel 1` with the FM1's MIDI IN set to SEL
  plays everything on the FM1's selected track.
- **Execute bits:** if files were copied without the zip, `chmod +x` both
  scripts and `seq4h/seq4h`.
- **Timing captures:** see *Harness add-ons for the hardware checklist*
  below (`--capture` files checked with `tools/midicheck.py`).

### If the driver is missing

Build `snd-usb-audio.ko`, `snd-usbmidi-lib.ko` and `snd-hwdep.ko` (and
`snd-seq*.ko` if wanted) against the exact Knulli kernel, the way the m8c port
does: Knulli's `rg35xx-plush-sdk` aarch64 toolchain, the kernel source and
`linux-sunxi64-legacy.config`, set `CONFIG_SND_USB_AUDIO=m`, build modules.
Put the `.ko` files in `/userdata/roms/ports/seq4h/modules/`; `SEQ4H.sh` loads
them. The `vermagic` must match `uname -r` on the device. This is not
automated yet; it is the next job if check B shows the driver is absent.
## Bluetooth MIDI and power (researched 2026-11-19, not tested)

Problem: over direct USB-C the handheld powers the FM1, which also charges
the FM1's 2000 mAh battery, so the handheld drains fast. Roy has no powered
hub. USB stays the primary route; this section is about options.

What is known (from web sources, not checked on the devices):

- Stock FM-1 firmware lists Bluetooth MIDI, and USB, TRS MIDI and BLE can
  run at once (Synthtopia, Synth Anatomy reviews). It has a 3.5 mm TRS MIDI
  input (TRS type A, adapter not included) and no TRS MIDI output.
- Felucca's wiki (Features page, 1.6) names only USB and TRS MIDI in, with
  clock from internal, USB or TRS. It never mentions Bluetooth, so assume
  Felucca has no BLE MIDI until a test shows otherwise. Quick test: with
  Felucca running, scan for Bluetooth MIDI on a phone; if "FM-1" shows up,
  BLE MIDI may still be there.
- Knulli on the H700 kernel 4.9.170 has no snd-seq, and BlueZ's ALSA MIDI
  bridge (`--enable-midi`) needs ALSA sequencer, so it is very unlikely to
  work out of the box. seq4h could speak BLE MIDI itself through BlueZ D-Bus
  (GATT client on the standard MIDI service 03B80E5A-...) if the Knulli
  bluetoothd allows LE GATT clients. Unchecked, and a few days of work.
- BLE MIDI timing: about 7.5 to 15 ms connection interval, so notes land in
  bursts with a few ms of jitter. The packet timestamps can fix note timing
  but Felucca's handling of them is unknown. Clock (24 ppqn) over BLE usually
  wobbles by a few ms; fine for ambient, noticeable at high BPM.

Options, cheapest first (prices rough, UK, 2026):

1. Recommended: a bus-powered USB MIDI interface with TRS or DIN out on the
   handheld (about 10 to 25 GBP) plus a TRS-A cable or DIN to TRS-A adapter
   (about 5 GBP) into the FM1's TRS MIDI in. The FM1 then runs from its own
   charger, Felucca clock source set to TRS. The handheld still uses its
   only USB-C port, so it cannot charge, but it only powers a small
   interface instead of the FM1 and its battery. seq4h needs no change if
   the interface is class compliant (it shows up as a raw MIDI port).
2. Wireless cable: a CME WIDI Bud Pro style USB BLE dongle on the handheld
   (shows up as USB MIDI) paired to a WIDI Jack or Master on the FM1's TRS
   in (about 90 to 110 GBP for both). Same handheld-charging limit as 1.
3. Fully wireless, both on chargers: seq4h speaks BLE MIDI over BlueZ to a
   WIDI Jack or Master on the FM1's TRS in (about 45 to 70 GBP). Needs the
   Knulli BlueZ check and new code; most effort, most timing risk.
4. A USB-C OTG Y adapter with a charge input. Behaviour with the RG40XX-V's
   single port in host mode is unknown; some hosts refuse or back-feed
   power. Not recommended without testing.

## Other FM-1 firmware and 8 tracks (researched 2026-11-20, not tested)

Sources: the Jangada README (github.com/zednaked/jangada, including its
README.felucca.md), the Felucca wiki Features and Controls pages, the
fm1.jesvs.com firmware list, and news posts on FM1 Open V14 and stock V16
(matrixsynth.com, synthtopia.com, neoteo.com). None of it was tried on
hardware. seq4h opens the first USB MIDI output it finds, so a firmware that
renames its USB device (Jangada shows up as "Jangada") is still found.

### Will seq4h work with other firmware?

| Firmware | Parts seq4h can play | Channels | Follows USB clock + Start/Stop | Verdict |
|---|---|---|---|---|
| Felucca (current) | 4 tracks, 8 voices shared | MENU > MIDI > MIDI IN: CH1-4 (also CH5-8, CH9-12, CH13-16, SEL, or TRACK with T1-T4 CHANNEL set to 1-16/OMNI/OFF) | Yes, on Roy's FM1 (HOME > MIXER > CLOCK, source USB) | Works (tested) |
| Jangada (Felucca fork) | 4 tracks; track 4 is DRUM or SYNTH | Not documented beyond "track 4 SYNTH = channel 4". Felucca's older README, which Jangada carries, says parts on 1-3 and drums on 10 | Yes: GLO > GLOBAL CLK USB "follows the computer's clock (tempo, start, stop)" | Very likely works with no code change |
| FM1 Open V14 (M-VAVE's open firmware) | 8-track internal sequencer; "NORMAL, PLAY and OFF track roles" | "Configurable MIDI input channels"; defaults not published | Not documented | Unknown; needs a hardware test |
| Stock V16 (M-VAVE) | 8 parts, 20 voices shared | "Separate MIDI channels" per part; which ones not published | Not documented | Unknown; the only one with 8 real parts |

What to do for each:

- **Jangada:** set GLO > GLOBAL CLK USB. Tracks 1-3 should play on channels
  1-3. If track 4 is silent, either switch Jangada's track 4 to SYNTH
  (channel 4), or keep it as DRUM and set seq4h's track 4 MIDI CHANNEL to 10
  on the TRACK page. If Jangada has Felucca's MIDI IN menu, set it to CH1-4.
- **FM1 Open V14 and stock V16:** try it, then use the TRACK page MIDI CHANNEL row to
  match whatever channels the firmware listens on. If the firmware ignores
  USB clock, leave it on its own clock and set its BPM to match: notes still
  play on time because seq4h does the note timing, only the FM1's own
  arpeggiator and synced delay would drift. (seq4h's clock can only be turned
  off with `--no-clock` or a `clock 0` line in seq4h.pat; there is no on-screen
  switch yet.)
- No new code is needed to try any of these: the per-track MIDI CHANNEL setting already
  covers different channel maps. If Roy switches firmware often, a small
  "FM1 firmware" preset in the M menu (Felucca 1/2/3/4, Jangada 1/2/3/10,
  Same channel for all) could set all four MIDI CHANNEL values at once. Optional.

### Is 8 tracks a good idea?

What it would take in seq4h:

- **Screen:** the grid is 4 rows of 64 pixels (y 70 to 326) with the step
  editor line under it. Eight full-size rows need 512 pixels, which does not
  fit in 480. Options: eight half-height rows (about 28 pixels, readable but
  cramped, and the step colours and lock marks get small), or keep four rows
  and show tracks 1-4 or 5-8 (a "bank" switch).
- **Navigation:** L2/R2 already step through tracks, so with 8 tracks they
  would step 1 to 8 and the grid would flip bank when you pass track 4. The
  mute/solo label column works the same.
- **CPU and timing:** no problem. The timing thread does a few hundred tiny
  operations per tick; doubling the tracks is still far below 1% of one H700
  core. USB MIDI has plenty of room for 8 tracks of notes plus clock.
- **Save file:** each line already carries its track number and older files
  load fine (missing tracks get defaults). A file saved with 8 tracks loaded
  into an old build would just drop tracks 5-8.
- **Work:** moderate. Mostly the screen, the generators and styles (each
  style sets up 4 tracks today), the SLOTS page and the tests.

What the synth can use:

- **Felucca and Jangada have 4 parts and 8 voices in total.** Tracks 5-8 would
  have nothing new to play: they could only double up on parts 1-4 (two
  patterns into the same sound, sharing the same 8 voices), or drive a second
  FM-1 set to MIDI IN CH5-8 on a hub.
- **Stock V16** is the only firmware found with 8 real parts (20 voices).
  FM1 Open V14 has 8 tracks in its own sequencer, but whether all 8 can be
  played from outside is not documented.

Options:

1. **Stay at 4 tracks (recommended for now).** It matches Felucca and Jangada
   exactly, keeps the big readable grid, and costs nothing.
2. **8 tracks as two banks of 4** (grid shows 1-4 or 5-8; L2/R2 run through
   all 8). Best layout if it is ever needed. Worth doing only once there is a
   target with more than 4 parts: stock V16, or a second FM-1.
3. **8 half-height rows on one screen.** Shows everything at once but every
   cell is half the size on a 4-inch screen. Not recommended.

Recommendation: keep 4 tracks while Felucca or Jangada is the firmware. If
Roy moves to stock V16 or adds a second FM-1, build option 2. A cheaper win
in the meantime is the optional firmware channel preset above.

## Piano roll, drum grid, note lengths and offsets (evaluated 2026-11-20; all four built)

Roy asked (2026-11-20) whether seq4h could open a piano roll from the grid for
fine editing, have a drum grid for a track marked as the drum track (with a
way to match drum notes to the right sounds), show how long notes are held,
and let notes sit off the grid. The current grid stays as the main view.

### How it works today

- Each track plays **one note per step** (plus optional chord notes stacked by
  the track's CHORD setting). A step has note, velocity, gate (5-100% of a
  step), hold (0-31 extra steps, a tie), probability and lock.
- The engine runs at 96 ticks per quarter note, so one 1/16 step is 24 ticks
  (about 5 ms per tick at 120 BPM). Notes start exactly on the step, apart
  from the global swing, which delays every other 1/16 by up to 12 ticks.
- The grid draws each step in its own box. A held note shows a small "+3"
  label, but the box is the same size as a one-step note.
- On the GRID page, Select on its own does nothing yet (Select+Start quits),
  and the stick press (code 12) is unused.

### 1. Note length on the grid (built 2026-11-20, see Decisions)

Draw a held note as a bar that runs through the following boxes until the
note ends (or the next note cuts it off), in a dimmer shade of the track
colour. Keep the "+3" label on the first box. This is a drawing change only:
no new data and no save-file change. About half a day with tests and
screenshots. The piano roll would draw lengths the same way.

### 2. Offsets ("nudge") (built 2026-11-20, see Decisions; HUMANIZE became SHUFFLE, SPREAD and EVO OFFSET)

Add an OFFSET row on the STEP page: -11 to +11 ticks, shown as a percentage
of a step (about -45% to +45%), with 0 meaning on the grid. Hold Y for big
jumps as on other rows.

- Engine: a note plays at its step time plus swing plus offset. A negative
  offset means the note is sent before the step starts, so the engine checks
  the next step one tick ahead. Gate and hold still count from where the note
  actually starts. MIDI clock is unchanged, so the FM1 stays in time.
- Grid: the coloured block inside the box shifts left or right a little, so
  you can see a step is early or late.
- Save file: a new optional `o T STEP TICKS` line (and the same in slots and
  the anchor), written only for steps with an offset. Older builds skip it;
  older files load with every offset 0. The format stays `seq4h 1`.
- Nice extra for ambient: a HUMANIZE amount on the GENERATE page that gives
  rolled notes small random offsets, and an evolution kind that drifts them.
- About a day with tests.

### 3. Piano roll view (larger) (built 2026-11-21, see Decisions)

A full-screen view of one track, opened from the GRID with Select (on release,
so Select+Start still quits) and closed with B or Select.

- Layout (640x480): steps across (16 at a time, 35 pixels each as on the
  grid, scrolling for 32), pitches down the left side. Show only the notes in
  the current scale, two octaves at a time (14 rows of about 24 pixels), with
  the note names in the left column and the root note rows highlighted.
  Up/down past the edge scrolls the octave. Notes are bars as long as gate +
  hold, offsets drawn as a small shift, velocity as brightness.
- Controls: d-pad moves the cursor (step and pitch). A puts the step's note on
  that pitch (or turns the step off if the note is already there). Hold Y +
  left/right makes the note longer or shorter (hold first, then gate). Hold X
  + left/right nudges the offset. Hold X + up/down changes velocity. L2/R2
  switch track, so you stay in the roll.
- Because a track plays one note per step, the roll shows at most one note in
  each column (plus chord notes in a faint colour, which follow the CHORD
  setting and are not edited here). That keeps it simple and matches how
  seq4h and Felucca's parts work.
- No save-file change. About two days with tests and screenshots.

### 4. Drum track and drum grid (largest) (built 2026-11-21, see Decisions; presets still wait for D3)

**Is there a standard?** Yes: the General MIDI (GM) drum map. Felucca's DRUM
engine is "an 8-lane kit ... on the General MIDI key map" (Felucca wiki,
Engines), and Jangada's DRUM track uses a GM kit too. The common GM notes are:

| Sound | GM note |
|---|---|
| Kick | 36 (C1) |
| Rim / side stick | 37 |
| Snare | 38 |
| Clap | 39 |
| Closed hi-hat | 42 |
| Low tom | 45 |
| Open hi-hat | 46 |
| Crash | 49 |

Felucca does not publish which 8 GM notes its lanes use, so seq4h needs a
calibration screen (below) anyway.

**What has to change:** a drum grid needs several hits on the same step (kick
and hat together), but a seq4h track can only play one note per step. So a
drum track needs its own data: 8 lanes, each with its own on/off, velocity
and probability per step, and one MIDI note per lane.

- **TRACK page:** a new TYPE row, NOTES or DRUMS. Any track can be the drum
  track; the ambient style would default track 4 to DRUMS, matching Felucca's
  and Jangada's track 4. Switching keeps both kinds of data, so nothing is
  lost by flipping back.
- **Drum grid:** opened from the GRID with Select on a drum track (the same
  key as the piano roll). 8 lane rows of about 36 pixels with the lane names
  on the left (KICK, SNARE ...), 16 steps across. A toggles a hit, hold X +
  up/down changes its velocity, Y locks. The main GRID still shows the drum
  track as one row, with a box lit wherever any lane hits.
- **Calibrating the notes (M > DRUM MAP):** a list of the 8 lanes, each with a
  name and a MIDI note. Left/right changes the note, A plays it once so you can
  hear which sound it is, Y renames the lane. Presets: GM (above), FELUCCA and
  JANGADA once Roy has checked on hardware which notes their lanes respond to.
  The map is saved in the pattern file (and could also be a small
  `seq4h.drums` file shared by every pattern).
- **Generators:** the drum generators (sparse ambient drums, Techno, etc.)
  write to lanes instead of picking notes, which is cleaner than today. They
  have to be updated for this. Evolution works per lane.
- **Save file:** new optional lines for the track type, lane notes, names and
  lane hits. Older builds skip them.
- About four to five days with tests, and a hardware session to fill in the
  Felucca and Jangada presets.

### Recommended order

1. Note length bars on the grid (quick, no format change).
2. Offsets, with HUMANIZE as an optional extra.
3. Piano roll for note tracks.
4. Drum track type, drum grid and DRUM MAP calibration.

Each step is useful on its own, and 1 and 2 also improve the piano roll and
drum grid when they arrive.

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
- **Persist** (`tests/harness.sh persist`): stores nine slots through replayed
  keys, kills seq4h with SIGKILL, checks the autosave has all nine, then
  relaunches, quits with Esc and checks the file is byte-identical.
- **`tools/midicheck.py`** checks a capture: Start first and Stop last, notes
  only on channels 1-4 (tracks 1-4), every note-on released, one note per
  channel at a time, all-notes-off at Stop, clock at 24 PPQN and the right
  tempo, every onset on the 1/16 grid (with swing), and with `--demo` that the
  demo pattern plays as written. `tests/test_midicheck.py` proves it catches
  broken input (wrong channel, hung note, late note, slow clock and so on).
  It also reads `aseqdump` output (`--aseqdump`), without timing.
- **Screenshots**: about two dozen UI states (all four pages) driven by `--keys` with
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

These need SSH (see the appendix above). They turn "it sounded fine" into
numbers. `tools/midicheck.py` runs on any PC with Python 3; copy capture files
off the handheld with `scp root@<handheld-ip>:/tmp/h.cap .`.

- **Without the FM1: does the H700 keep time?**
  `./seq4h --midi capture:/tmp/h.cap --demo --headless 20`, then on a PC
  `tools/midicheck.py h.cap --demo --duration 20 --tolerance-ms 2 --max-ms 8`.
  All PASS means the timing thread holds up on the handheld. Record the
  "note timing error" and "clock error" lines.
- **While playing (checklist B): record what reached the FM1.** Add `--capture /tmp/fm1.cap` to
  the seq4h command (it tees, so the FM1 still gets everything), then run
  `tools/midicheck.py fm1.cap --bpm N` (N = the tempo you played at; add
  `--demo` if you didn't edit the pattern). This separates "seq4h sent the
  wrong thing" from "the FM1 did something unexpected with it".
- **During the long run (checklist E):** the same with `--capture` on the 30-minute run (a few MB).


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
- `src/drum.c`, `drum.h`: drum tracks: editing, DRUM MAP (FELUCCA and GM presets, Felucca sound table), audition, drum
  kit generator, drum evolution, morph and transforms
- `tests/test_drum.c`: drum track tests
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
- `src/battery.c`, `battery.h`: battery level from sysfs; `tests/test_battery.c`
- `src/usbinfo.c`, `usbinfo.h`: USB devices from sysfs for M > MIDI and the log; `tests/test_usbinfo.c`
- `knulli/seq4h.input`: RG40XX-V button map
- `tests/harness.sh`, `tests/test_midicheck.py`, `tests/fixtures/`: harness
- `tests/hardware-sheet-133.md`: 20 on-device tests (PASS/FAIL) for build #133
- `knulli/SEQ4H.sh`: Ports launcher script
- `knulli/SHOW-BUTTONS.sh`: Ports entry that starts seq4h with button codes shown
- `knulli/USB-MIDI-CHECK.sh`: USB/MIDI diagnostic for the device
- `knulli/build.sh`, `knulli/Dockerfile`: aarch64 cross-build and packaging
