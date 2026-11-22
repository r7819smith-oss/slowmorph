# Genvolve

*Genvolve was called seq4h until November 2026. The source, the desktop
binary (`./seq4h`) and the saved files (`seq4h.pat`, `seq4h.midi`,
`seq4h-song1.pat`..., `seq4h.input`) keep the old name, so nothing saved is
lost; on Knulli the app is installed as `GENVOLVE.sh` and `genvolve/`. See
"Moving from seq4h" below.*

An eight-track step sequencer for the Anbernic RG40XX-V running Knulli. It sends
MIDI to an M-VAVE FM1 running Felucca firmware (or any other MIDI device). It is inspired by [SEQ-4](https://github.com/devrvega/SEQ-4) but shares
no code with it.

**State:** the engine, UI and timing run and are tested on a desktop Linux
machine. The ALSA MIDI output compiles but has **not been run against real
MIDI hardware**, and nothing has been run on the RG40XX-V yet.

## Features

- 8 tracks x 16 steps (1/16 notes), each with its own MIDI channel and length (1-16, so tracks can be polymetric). T1-T4 play the FM1 (channels 1-4); T5-T8 start on channels 5-8, for a second instrument such as the Blackbox 2 or the Digitone
- Per step: on/off, note, velocity, gate length, probability, and a **lock**
- **MONO or POLY** per track: a POLY track plays up to 4 notes on a step (a
  chord) and lets notes ring on over the next ones
- A global **scale and root** (12 scales). Editing a note steps by scale degree.
- **Pattern generators** and **evolution** (below)
- Swing, mute and solo per track (from the grid) and per drum lane (from the drum grid), BPM 30-300
- Clock master: MIDI clock (also while stopped), start and stop output; each can be turned off per output on M > DEVICE + CLOCK (or both with `--no-clock`)
- Device presets (M > DEVICE + CLOCK): FELUCCA, JANGADA, DIGITONE and BLACKBOX 2 set the channels, drum map, clock and start/stop in one go
- Modulation (M > MODULATION): 4 LFOs (sine, triangle, ramp up/down, square, sample and hold, smooth random; synced to the BPM or free running from 0.1 s to 10 minutes) driving up to 16 MIDI CC destinations, each with its own channel or track, CC number and LOW-HIGH range. A MIDI IN monitor shows what the device sends back, and LEARN takes an incoming CC
- Everything (all 8 slots per track, chains, generators, evolution, mutes,
  scale, tempo) is kept in `seq4h.pat` (plain text): saved two seconds after
  each edit, once a minute while playing, and at quit, so it survives the
  system's exit hotkey or a flat battery too
- 96 ticks per quarter note from a dedicated timing thread; measured 0.1 ms
  deviation on desktop Linux

The sequencer sends notes, clock, start/stop and, if you switch them on,
modulation CCs. Effects and sounds are the FM1's.

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
| FOLLOW | Takes another track's rhythm and accents, e.g. a bass line that follows the kick. Rhythm can be the same, the gaps between the source's hits, an echo one step later, or every other hit. Pitch is the root plus an interval, the source's note plus an interval (harmony, folded into this track's range), or this track's own notes. | source track, rhythm, interval (scale degrees), pitch |
| ARP | Arpeggiates a chord built from the scale on the track's lowest note. | chord (triad, seventh, sus2, sus4, add9, power), order (up, down, up-down, random, inward), octaves, rate |
| TURING | A looping shift register read as pitch and rhythm. `CHANGE` is the chance each bit flips as it loops round on a roll: 0 locks the loop, small values drift slowly, 50% is random. Pair it with Evolve's regen for a melody that slowly mutates. | change, density, range, loop length |
| MOTIF | Invents a short phrase and develops it across the bar: each repeat may be transposed, inverted, reversed or both. | motif length, develop (chance a repeat changes), density, leap (largest interval) |
| DRONE | One to four long notes that fill the pattern (more than one alternates with the fifth above). Each roll the drone may move (`MOVE`) to another stable tone within a fifth (root, fourth, fifth, sixth, third), or go back to the root (`HOME`). Pair it with `CHORD` for a pad, and Evolve regen every 16-32 bars so it shifts now and then. | notes, move chance, home chance |
| DRIFT | A few long notes spread over the pattern, each at most `STEP` scale degrees from the last, starting from the track's current first note, so the line keeps wandering slowly from roll to roll. `HOLD` is how much of the gap to the next note each one is held for. | notes, hold, step (largest move) |
| BELL | Sparse, soft notes, never two in a row, on weighted stable tones (root, fifth, third) `HEIGHT` octaves above the lowest note. Each plays only `CHANCE` of the time and is up to `SOFTEN` quieter. | notes, chance, height, soften |
| CHORDS | A chord progression written as real chords (up to 4 notes per step, editable in the piano roll). One chord every `CHANGE` steps (4, 8, 16 or 32); each chord's root moves round the scale (by a fourth, fifth, third or step) with chance `MOVE`, and each chord takes the inversion nearest the last one, so the voices move little. `RHYTHM`: HOLD (held to the next chord), PULSE (every beat), STABS (short, off the beat) or SWAY (twice per chord). Choosing it sets the track to POLY. Other generators write single notes. | change, chord (triad, seventh, sus2, sus4, add9, power), move chance, rhythm |

**Evolve** changes a track every 1, 2, 4, 8, 16, 32 or 64 bars while it plays.
`AMOUNT` is the chance each step changes; below 10% it moves in 1% steps, for
very slow drifts. Choose what may change: pitch (one scale degree
up or down), rhythm (toggle a step), velocity, rotate (shift the whole pattern
one step), and regen (re-run the generator, so CELLULAR advances a generation
each time). Together with per-step probability, this gives patterns that keep
their character but never settle.

**Each track picks its own approach.** Set `GENERATOR` on the GENERATE page
per track (L2/R2 or the TRACK row change track). `MANUAL` means nothing is
generated: write the pattern yourself on the grid and STEP page. A manual track
only changes if you turn its `EVOLVE` on. To start a track from blank, choose
`TRANSFORM` CLEAR and press A. The grid shows each track's approach under its
name (bright when it is evolving).

More on the GENERATE page (press **A** on a row to do its action):

- **Anchor and pull:** save a track's pattern as its anchor; each bar, every
  step that has drifted returns to the anchor with chance `PULL`, so evolution
  wanders and comes home.
- **Probability drift:** evolution can nudge step probabilities (kept within
  10-100%).
- **Density limits:** evolution keeps the number of hits between a minimum and
  a maximum, so a track can't empty out or fill up.
- **Transforms** for hand editing: reverse, invert (around the first note),
  transpose by scale degrees, shift, thin (offbeats and quiet notes go first),
  fill (new notes borrowed from neighbours), humanize velocity, clear, spread
  (every note a different offset, none on the grid), on grid (offsets back
  to 0) and write chord (the `CHORD` row's chord becomes each note's own
  chord notes, which you can then edit; with CHORD OFF it makes every step a
  single note again). All of them
  leave locked steps in place; rotate also now works around locked steps.
- **Chord, swell and rests** (at the bottom of the list) colour how a track
  plays without changing its steps. `CHORD` stacks notes from the scale on
  every note the track plays: FIFTH, TRIAD, SUS2, SUS4, SEVENTH or OPEN (fifth
  plus the third an octave up), a little softer than the step's own note, all
  on the track's channel. On a MONO track the next note cuts the chord off,
  as before; on a POLY track it rings for the note's length, and only steps
  without chord notes of their own get it. `SWELL` slowly raises and lowers velocity over 2 to
  32 bars, dipping by `SWELL DEPTH`; each track's swell is a quarter turn
  behind the one before. `RESTS` is the chance each bar that the track sits
  out for 1 to 4 bars (held notes ring on), and it always plays at least a bar
  between rests.
- **Scale snap:** with `SNAP NOTES` on, changing scale or root moves every
  note into the new scale. A on that row snaps the notes now.
- **Slots and chains:** each track has 8 pattern slots. `SAVE TO SLOT` stores
  the live pattern; A on `SLOT` loads it back. **M > PATTERN SAVE** lists them by
  name, with load, save, rename, copy and clear (see *Slots page* below). `CHAIN` plays slots 1 to N in
  turn, `BARS PER SLOT` each, and `CHANGE BY` either jumps to the next slot or
  morphs into it a few steps per bar.
- **Held notes:** `HOLD` on the STEP page ties a note over up to 31 more steps
  (until the track's next note), for drones and pads. The grid draws a held
  note as a bar across the steps it lasts for.
- **Offsets:** `OFFSET` on the STEP page plays a note up to 46% of a step
  early or late, so it doesn't have to land on the grid. The note's block
  moves left or right in its box on the grid. Rolling a generator puts
  unlocked notes back on the grid.
- **Shuffle:** `SHUFFLE` on the TRACK page gives every note a fresh random
  offset (up to the amount, around its own OFFSET) each time it plays, so the
  track never plays exactly the same twice; the pattern itself doesn't change.
  `NEVER ON GRID` keeps every note on the track at least 2/24 of a step off
  the grid. For a fixed loose groove instead, use the `SPREAD` transform, and
  turn on `EVO OFFSET` to let those offsets drift a little every few bars.
- **MONO and POLY tracks:** `VOICES` on the TRACK page (the last row).
  MONO (the default, as before) plays one note at a time: the next note cuts
  the last one off, and a step's chord notes are kept but not sent. POLY
  plays every note of a step at once (up to 4, all at the step's velocity)
  and lets each note ring for its own length (gate plus hold) over the notes
  after it, up to 8 at once per track (the one due to end first makes room).
  Give a step a chord with `CHORD` on the STEP page (FIFTH, TRIAD, SUS2,
  SUS4, SEVENTH, OPEN), by hand in the piano roll, with the CHORDS generator
  or the write chord transform. Chord notes are kept as scale degrees from
  the step's note, so they move with it (NOTE, transpose, evolution) and stay
  in the scale. Felucca plays chords only when its own track is on POLY too:
  on the FM1, EDIT > VOICE, first knob `VCE` = POLY (mono presets load as
  LEG). All four FM1 tracks share 8 voices.
- **Patterns up to 32 steps:** set `LENGTH` on the TRACK page. The grid shows
  16 steps at a time; move the cursor past step 16 to see steps 17-32.

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
| Left stick left / right [, / .] | previous / next step (grid, STEP page, piano roll, drum grid) |
| Left stick up / down [`]` / `[`] | velocity: the grid step (on a drum step every hit on it, on a track label every step of the track), the piano roll note, the drum grid hit (on a lane name every hit of the lane). On STEP, TRACK, GENERATE and ALL STEPS it changes the selected row's value, like right / left |
| Stick press [R] | quick roll of the current track (as A on GENERATE) |
| hold Select + A / X / Y | roll / undo / lock or unlock the whole track |
| hold Select + stick up / down | tempo up / down by 1 BPM (keeps going while held) |
| B [X] | back to the grid (on a grid track label: solo) |
| M [M] | open / close the main menu |
| Start [Space] | play / stop |
| Select + Start [Tab + Space, or Esc] | quit (the pattern is saved) |

**GRID page**: four tracks at a time. T1-T4 is the first page of tracks;
moving down from T4 (D-pad, or R2) shows T5-T8, and up from T5 goes back.
Top right, `T1-4 / T5-8` shows which page is on screen, with a coloured dot
for each track on the other page that has notes. T5-T8 start empty, on MIDI
channels 5-8, so an FM1-only setup is not affected by them.

| Input | Action |
|---|---|
| D-pad [arrows] | move the cursor (up/down = track, left/right = step) |
| A [Z] | step on / off |
| X [C] | edit this step (opens the STEP page) |
| Y [V] | lock / unlock this step (generators, evolution and transforms leave a locked step alone; on a drum track it locks every lane's hit on that step) |
| Select [Tab] | open the **piano roll** (note track) or the **drum grid** (drum track); press and let go on its own, so Select + Start still quits |
| Left from step 1 | move the cursor onto the track label (T1-T8) |

On a drum track A and X open the drum grid at that step instead, since a
drum step has 8 lanes. The grid draws a drum step as thin bars, one per lane
that hits (kick at the top), and the label reads DRUMS.

With the cursor on a **track label**:

| Input | Action |
|---|---|
| A [Z] | mute / unmute the track |
| B [X] | solo / unsolo the track (several tracks can be soloed) |
| X [C] | roll (generate) this track, as A on GENERATE does. Hold X for half a second instead to undo the last roll |
| Y [V] | lock / unlock the whole track |
| Select [Tab] | open **ALL STEPS** for this track (below) |
| Up / down | next / previous track |
| Right | back to step 1 |

A **locked track** shows an orange **L** badge. Roll, evolution, the
GENERATE page transforms and M > NEW FROM STYLE (BLANK too) leave it exactly
as it is; you can still edit it by hand, load slots into it and chain it.
The lock is saved with the pattern.

**ALL STEPS** (Select on a track label) changes one setting on every step
of the track at once. Rows: VELOCITY, GATE, HOLD, OFFSET, PROBABILITY and
STEP LOCKS (a drum track has VELOCITY, PROBABILITY and STEP LOCKS, and they
change every hit of every lane). Each row shows the range over the notes
that play, e.g. `60 TO 110`. Left/right moves every step's value up or down
together (hold Y for big steps; values stop at their limits). A gives every
step the average. On STEP LOCKS, A locks every step, or clears all step
locks if any step is locked. ALL STEPS is a hand edit, so locked steps
change too. B or Select goes back to the track label.

Muted tracks turn grey and show an **M** badge; soloed tracks show a yellow
**S** badge, and while anything is soloed the other tracks are dimmed and
silent. Mute is saved with the pattern; solo is not, so Genvolve always starts
with every unmuted track playing. The TRACK page's MUTE row is the same
setting.

**Piano roll** (Select on the GRID, note tracks): one track full screen,
steps across (16 at a time, as on the grid) and the scale's notes down the
left, 14 rows at a time with the root's rows lighter. Each note is a bar as
long as it sounds (gate plus hold, cut off by the next note on a MONO track),
moved by its offset, brighter when louder; a note outside the scale has a
white outline and chord notes from the CHORD row are drawn faintly. On a
MONO track each column holds one note. On a POLY track a column can hold a
chord of up to 4 notes: A adds the cursor's note to the step or takes it out
(taking out the step's own note hands that role to its lowest chord note),
and hold Y + up/down moves the whole chord. The grid marks a POLY step's
chord notes with small white dots. B or Select goes back to the grid;
L2/R2 changes track (a drum track opens its drum grid).

| Input | Action |
|---|---|
| D-pad | move the cursor: left/right step, up/down pitch (scrolls at the edge) |
| A [Z] | MONO: put the step's note on this pitch, or take it off if it is already there. POLY: add this pitch to the step's chord, or take it out (4 notes at most). Heard once while stopped |
| hold Y [V] + left / right | shorter / longer: a quarter step at a time up to one step, then whole steps (that is HOLD). This also sets the length new notes get |
| hold Y + up / down | move the step's note (and its chord) up / down a scale degree (the cursor follows) |
| Y on its own (press and let go) | lock / unlock the step under the cursor, as Y on the grid: a locked note has a yellow corner and a locked step's column a dim yellow frame. Rolls and evolution leave it alone |
| hold X [C] + left / right | offset: 1 tick (1/24 step) earlier / later |
| hold X + up / down | velocity up / down by 8 |
| R1 or the STEP tab | edit the step on the STEP page; B there comes back to the piano roll |

A on an empty step writes a note as long as the last length you set with
Y + left/right (it starts at one step); the info line shows it as
**NEW NOTES: LEN**. So set the length once, then write a run of notes.

**Drum tracks and the drum grid.** On the TRACK page set **TYPE** to DRUMS
and the track plays 8 lanes instead of one note per step: KICK, SNARE,
HAT CL, HAT OP, CLAP, RIM, TOM and BELL, each with its own hits, velocity
and chance, so a kick and a hat can play on the same step. Each lane sends
its own MIDI note on the track's channel (the **DRUM MAP**, below) as a short
one-shot. Switching TYPE back keeps both patterns, so nothing is lost.
Swing, live shuffle, NEVER ON GRID, swell, rests, mute/solo, slots, chains
(with morph), undo and the anchor all work on drum tracks; drum steps have no
OFFSET of their own. On the GENERATE page a drum track's generator is the
**DRUM KIT**: A rolls a kit pattern in the track's style (the style it was
last made from by NEW FROM STYLE; ambient, drone and blank: very sparse and
soft, about two hits a bar, nearly all left to chance; techno: four on the
floor, claps on 2 and 4, offbeat hats with ghosts; melodic: a plain backbeat
with 8th hats; hip hop: boom bap, kick on the one and around the "and" of 2
and 3, snare on 2 and 4; drum n bass: two-step, kick on 1 and the "and" of 3,
snare on 2 and 4 with ghost snares; house: four on the floor, clap on 2 and
4, open hat on every offbeat; trip hop: half-time, snare on 3, lazy kick),
and **BUSY** (0-100%, 50 = as written) makes it sparser or busier.
Evolution works per lane: RHYTHM moves a hit to a free step next to it,
VELOCITY and PROB nudge hits, ROTATE turns a lane by a step, REGEN re-rolls,
and the anchor pull brings the lanes home (PITCH and OFFSET do nothing on
drums). The transforms REVERSE, SHIFT, THIN, FILL, HUMANIZE and CLEAR work on
the lanes. NEW FROM STYLE keeps a drum track a drum track and rolls it a kit
pattern in the new style. The STEP page shows a drum step as one row per
lane: left/right changes the hit's velocity (below 1 removes it), A puts a
hit on or off.

The **drum grid** (Select on the GRID, drum tracks): the 8 lanes, with each
lane's name and note on the left, 16 steps at a time. A lane can be
**locked** (yellow marks): rolls, evolution and transforms leave it alone.

| Input | Action |
|---|---|
| D-pad | move the cursor: up/down lane, left/right step |
| A [Z] | hit on / off (a new hit: velocity 100, chance 100%; heard once while stopped) |
| hold X [C] + up / down | velocity up / down by 10 |
| hold X + left / right | chance down / up by 10% |
| Y [V] | lock / unlock the lane |
| Left from step 1 | onto the lane name: A mutes the lane, B solos it, X plays its note, Y locks, up/down picks another lane, right goes back |
| B or Select | back to the grid (on the lane name, B is solo, so use Select or move right first) |

Lane mute and solo work like the track mute and solo on the grid, but
inside one drum track: a muted lane (grey **M**) is silent, and while any
lane is soloed (yellow **S**) only soloed lanes play. Silent lanes are drawn
dim, on the drum grid and in the drum track's row on the main grid. Several
lanes can be soloed. A soloed lane that is also muted stays silent. The
track's own mute and solo still apply on top. Lane mutes are saved with the
pattern, lane solos are not (as on the grid).

**STEP, TRACK and GENERATE pages**: a list of values for the current track.
Up/down picks a row, left/right changes it, and holding **Y** makes the
change bigger.

| Page | Rows | Also |
|---|---|---|
| STEP | step number (hold X + left/right, or the stick, moves to the previous / next step from any row), on/off, note (scale degree; hold Y = octave), velocity, gate, hold, offset, probability, lock | A = step on/off |
| TRACK | track, type (NOTES or DRUMS), MIDI channel, length, mute, shuffle, never on grid, BPM, swing, MIDI port (shown only) | |
| GENERATE | track, generator and its parameters, lowest note, range, evolve settings (amount, kinds, min/max hits), anchor and pull, transform, slots and chain, scale, root, snap notes, chord, swell, swell depth, rests | A = roll, or the selected row's action; X = undo the last roll, transform or slot load |

**Main menu** (M): settings for the whole session. Up/down picks a row,
left/right changes it, A selects; M or B closes it and returns to the page
you were on. Rows:

- *Back to editing*
- *Music style*: AMBIENT (default), TECHNO, MELODIC, BLANK, DRONE, HIP HOP,
  DRUM N BASS, HOUSE or TRIP HOP. Changing it sets the T1-T4 rows below to
  that style, and the T5-T8 rows of tracks that already have notes (empty
  T5-T8 stay KEEP, so they are never filled by surprise).
- *T1 ... T8*: what each track gets from *New from style* (T5-T8 can take any
  style's parts, e.g. TECHNO KICK on T5 for the Blackbox): KEEP AS IT IS,
  a style's part (each style has four: AMBIENT LOW, ARP, MOTIF, HIGH; TECHNO
  KICK, HATS, BASS, STAB; MELODIC BASS, ARP, CALL, ANSWER; DRONE BASS, PAD,
  DRIFT, BELLS; HIP HOP BASS, KEYS, LEAD, DRUMS; DRUM N BASS SUB, PAD, STAB,
  DRUMS; HOUSE BASS, CHORDS, LEAD, DRUMS; TRIP HOP BASS, PAD, BELLS, DRUMS),
  or BLANK (an empty MANUAL track). Left/right steps through the parts, hold
  Y + left/right jumps to the next style. A drum track shows "STYLE KIT": it
  gets that style's drum kit whatever the part. So you can have techno drums
  on one track, a drone on another and an ambient motif on a third. A track
  remembers its style: it picks that style's generator defaults on the
  GENERATE page, and a drum track rolls that style's kit.
- *New from style*: press A twice to replace the tracks the T rows say
  (KEEP rows are left exactly as they are). When every track is replaced,
  the music style sets the tempo, swing, scale and root; when any track is
  kept they all stay, so the new parts fit. A DRUMS part on a note track
  plays the kit's main hits (kick first, then snare, clap, hats ...) as the
  DRUM MAP's notes, for an FM1 track on the DRUM engine. The
  ambient set runs at 72 BPM in D major pentatonic, with track lengths 16, 13,
  11 and 23 so the tracks drift against each other, long held notes, and
  evolution that wanders and is pulled back to each track's anchor.
  The techno set runs at 128 BPM in A phrygian: T1 a four-on-the-floor kick,
  T2 offbeat hats (locked) with quiet ghost 16ths that come and go, T3 a
  rolling bass in the gaps between kicks with offbeat accents and varied
  note lengths, T4 a short stab riff that changes a little every few bars.
  The melodic set runs at 100 BPM in D dorian, all tracks 32 steps over a
  short chord progression: T1 bass on the chord roots, T2 a chord-tone
  arpeggio, T3 a phrase ("call") in steps 1-8 and 17-24 and T4 its answer in
  9-16 and 25-32. The resting halves of T3 and T4 are locked off, so rolling
  or evolving keeps the call and response; unlock them to fill them in.
  With BLANK, *New from style* gives four empty MANUAL tracks to build from
  scratch: 16 steps, every step off, evolution off, no chain, all unmuted.
  Tempo, swing, scale, root, MIDI channels and your stored slots stay as they
  are.
  The DRONE set is a second ambient set: A dorian at 66 BPM, a held bass with
  its fifth (32 steps) and a triad pad (28 steps) that move to another stable
  tone now and then, a slowly drifting line (20 steps) and sparse soft bells
  (27 steps) that rest now and then, with slow swells and evolution every 4
  to 32 bars.
  The newer sets each have a drum part on T4: HIP HOP at 88 BPM in G minor
  with swing (bass on the chord roots, seventh-chord keys held on each chord
  change, a sparse motif lead, boom bap); DRUM N BASS at 172 BPM in A minor
  (a sub that holds two notes, a swelling triad pad, a short looping stab,
  two-step drums); HOUSE at 124 BPM in F dorian (an offbeat bass, syncopated
  seventh-chord stabs, a light arpeggio, four on the floor); TRIP HOP at 80
  BPM in D minor (a slow drifting bass, a dark swelling seventh pad, sparse
  bells that rest, half-time drums).
- *Pattern save*: A opens the **Pattern save page** (below), the 8 stored
  patterns of the current track (called SLOTS before).
- *Song save*: A opens the **Song save page** (below), 8 saved whole
  sessions.
- *Drum map*: A opens the **Drum map page** (below).
- *Button codes*: show each button's raw code (see *Button mapping*).
- *Save and quit*

**Pattern save page** (M > PATTERN SAVE, A): the 8 slots of the current track, each with
its name and length, or EMPTY. `<` marks the slot last loaded or saved. The
right side shows the selected slot's pattern. L2/R2 changes track.

| Input | Action |
|---|---|
| Up / down | pick a slot |
| A [Z] | load the slot into the track (X on GENERATE undoes it) |
| X [C] | save the track's pattern into the slot (a full slot asks for X again) |
| Y [V] | rename the slot |
| Select [Tab] | copy or clear: COPY TO (left/right picks the slot), CLEAR SLOT, BACK; A does it, and A again confirms replacing a full slot or clearing |
| B [X] | back to the main menu (closes the copy/clear panel first) |
| M [M] | close the menu |

**Song save page** (M > SONG SAVE, A): 8 songs. A song is everything: all
eight tracks with their stored patterns, names and chains, BPM, swing, scale,
root, style, MIDI channels, mutes, track types and the drum map. Each song is
its own file next to `seq4h.pat` (`seq4h-song1.pat` to `seq4h-song8.pat`, the
same format plus a `name` line), so songs can be copied on the Mac; a song
file copied over `seq4h.pat` loads as the session. Rows show the name and
BPM, or EMPTY; `<` marks the song last loaded or saved. The right side shows
what the selected song holds.

| Input | Action |
|---|---|
| Up / down | pick a song |
| A [Z] | load the song; A again confirms, since it replaces all eight tracks. While playing, it keeps playing in time with the new song |
| X [C] | save the whole session as this song (a full song asks for X again) |
| Y [V] | rename the song (as slot names, start as SONG 1 to SONG 8) |
| Select [Tab] | COPY TO (left/right picks the song), CLEAR SONG, UNDO LOAD (back to the session before the last load), BACK; A does it, A again confirms replacing or clearing |
| B [X] | back to the main menu |
| M [M] | close the menu |

**Drum map page** (M > DRUM MAP, A): the MIDI note each drum lane plays,
one map shared by every drum track and saved in `seq4h.pat` (lines `dm LANE
NOTE NAME`, after a `dmv 2` line). It starts as the **FELUCCA** preset, the
notes Felucca's DRUM engine is built around (from its firmware source):
KICK 36, SNARE 38, HAT CL 42, HAT OP 46, CLAP 39, RIM 37, TOM 45, BELL 56
(Genvolve names notes with middle C = C4 = 60, so 36 shows as C2). The **GM**
preset is the same with a crash (49) instead of the bell. Each row shows
the lane's note and what Felucca plays for it, for example `41 TOM -7` (a
tom, 7 semitones down): Felucca plays every note from 35 to 81 as one of
its 11 sounds (kick, rim, snare, clap, closed and open hat, tom, cymbal,
bell, clave, conga), pitched up or down. Files saved by the first drum
build with the old untouched GM map are moved to FELUCCA when loaded.

Notes play on the current track if it is a drum track, otherwise on the
first drum track, otherwise on the current track; the right side says which
track and MIDI channel, and reminds you that the FM1 track on that channel
must use its DRUM engine (with Felucca's MIDI IN on CH1-4, channel n plays
FM1 track n). If the sounds are wrong, that is the first thing to check.

| Input | Action |
|---|---|
| Up / down | pick a lane (the last row is PRESET) |
| Left / right | lane note down / up by 1, and play it |
| hold Y [V] + left / right | jump to the next Felucca sound down / up (kick, rim, snare ... at its own pitch), and play it |
| A [Z] | play the lane's note; on PRESET left/right picks FELUCCA, GM or BLACKBOX and A twice applies it to all 8 lanes |
| X [C] | rename the lane (as slot names; an empty name gives the Felucca name back) |
| B [X] | back to the main menu |

**Modulation** (M > MODULATION, A): three pages, **LFO**, **CC DEST** and
**MIDI IN**; L1/R1 moves between them, L2/R2 picks the previous / next LFO
or destination, hold Y for big steps, B back to the menu. Each destination
moves one CC **between LOW and HIGH** and never outside them: HIGH - LOW is
how far it moves (the depth), MOVE RANGE shifts both and keeps the width,
LOW = HIGH holds one value, INVERT turns it upside down. A CC is only sent
when its value changes, at most every 20 ms, and at most 4 CCs go out on one
engine tick; with ALL MOD or a destination off nothing more is sent, so the
parameter stays where it was.

| LFO row | What it does |
|---|---|
| LFO | which of the 4 |
| STATE | RUNNING or PAUSED (stops where it is; its CCs stay put) |
| WAVE | SINE, TRIANGLE, RAMP UP, RAMP DOWN, SQUARE, S+H RANDOM (a new random value each cycle), SMOOTH RAND (glides to a new random value each cycle). Every wave starts at LOW |
| TIMING | SYNC TO BPM (1/16 to 64 bars; restarts on Start, so it lines up with bar 1) or FREE (0.1 s to 10 min; ignores the BPM and Start) |
| CYCLE | how long one cycle takes |
| START PHASE | where in its cycle a synced LFO starts, 0-95% |
| WHEN STOPPED | KEEPS MOVING (default, like the clock) or PAUSES while the sequencer is stopped |
| ALL MOD | ON, or OFF: no modulation CC is sent at all |

| CC DEST row | What it does |
|---|---|
| DEST / STATE | which of the 16, and ON / OFF (A toggles) |
| LFO | which LFO drives it (several destinations can share one) |
| SEND TO | T1-T8 (that track's channel, and it follows the track), T1-T4, T5-T8 or T1-T8 (the channels of those tracks, once each), or a fixed CH 1-16 |
| CC | 0-119; the name shown is Felucca's map with the FELUCCA device, else General MIDI. 120-127 are never sent; bank select, data entry and RPN/NRPN say AVOID |
| LOW / HIGH / MOVE RANGE | the range, see above |
| INVERT | HIGH when the LFO is low |
| LEARN CC | A, then move a control on the device: the first incoming CC 0-119 sets CC and channel (B cancels). Notes are not learned, nor a CC this page sends itself (an echo) |
| SEND ONCE | A sends the CC now, even when off: for an instrument's own MIDI learn |

**MIDI IN** lists what arrives from the output's device (notes, CCs,
pressure, program, pitch bend, start/stop; clock is only counted). Input is
only open while this page listens or a LEARN waits, and nothing received is
ever sent out again. Learning an incoming CC is not the same as finding the
CC of a parameter you turned on the FM1: Felucca does not send its knobs
(see NOTES.md, *Modulation and MIDI input*). For Felucca, set a free CC here
and teach it with SEND ONCE while Felucca's own LEARN (GLO + D4) waits.

Velocity is a note value, not a CC, so it is not a destination; SWELL on
the GENERATE page already changes velocity slowly.

Pattern file lines (older builds skip them): `mo ON`, `lf L WAVE SYNC RATE
PHASE PAUSED STOPPAUSE`, `md D ON LFO TO CC LOW HIGH INVERT`. Songs carry them.

**Device page** (M > DEVICE + CLOCK, A): one preset per instrument, and
MIDI clock and Start/Stop for the output in use.

| Row | What it does |
|---|---|
| DEVICE | left/right picks FELUCCA, JANGADA, DIGITONE or BLACKBOX 2; A twice puts the eight tracks' MIDI channels (T5-T8 on 5-8 in every preset), the DRUM MAP, clock and Start/Stop on it. `*` = the settings match it now. Notes, track types and everything else stay. The right side lists what to set on the device itself |
| MIDI CLOCK | SEND (Genvolve is the clock master, also while stopped) or OFF (the device keeps its own tempo) |
| START/STOP | SEND or OFF. Off: Start on the handheld does not start the device's own sequencer |
| OUTPUT | the port these settings belong to (pick it on M > MIDI OUTPUT) |

| Preset | Channels | Drum map | Clock | Start/Stop |
|---|---|---|---|---|
| FELUCCA | 1 2 3 4 | FELUCCA | on | on |
| JANGADA | 1 2 3 4 | GM | on | on |
| DIGITONE | 1 2 3 4 | GM | on | off |
| BLACKBOX 2 | 1 2 3 4 | BLACKBOX (a 909 kit: KICK 36, SNARE 47, CL HAT 39, OP HAT 44, CLAP 41, RIM 46, TOM 51, CRASH 42) | on | on |

Clock, Start/Stop and the device are kept **per output** in `seq4h.midi`
(next to `seq4h.pat`): line 1 is the chosen port as before, then one line
per output, `out CLOCK START/STOP DEVICE PORT`, for example
`out 1 0 DIGITONE Elektron Digitone:Elektron Digitone MIDI 1`. Choosing that
port again on M > MIDI OUTPUT (or starting with it) brings its settings back,
and a port that has none but looks like one of the devices gets a hint to
set it here. The pattern file also keeps `clock` and `transport` lines, used
when the output has no settings of its own.

**Renaming**: names are up to 8 characters (A-Z, 0-9, space and `-`) and
start as SLOT 1 to SLOT 8. Up/down changes the letter under the cursor,
left/right moves along, X makes it a space, A keeps the name and B cancels.
Names are saved in `seq4h.pat` (lines `sn TRACK SLOT NAME`, spaces written as
`_`); a copy takes the name with it and clearing a slot resets its name.
Older versions of Genvolve (and seq4h) load the file and ignore the names.

On the first run (no saved `seq4h.pat` and no `--demo`), seq4h starts from the
ambient set.

The STEP page edits the step under the grid cursor, and its STEP row moves
that cursor, so you can walk through a pattern without going back to the
grid. A preview of the current track sits on the right of each list page.

## Button mapping

Every button can be remapped with a plain text file, `seq4h.input`, read from
the working directory at start (or `--input FILE`). Lines you leave out keep
their default, so the file only needs the changes. `--dump-input` prints the
full mapping in use, which is a good starting point.

### RG40XX-V under Knulli

Under Knulli the RG40XX-V's built-in pad is a joystick called `Deeplay-keys`.
These are the raw SDL joystick codes it sends, taken from the mapping other
Knulli ports use for this pad, and matching every button in the first test on a Knulli
RG40XX-V (with the older stock-firmware table, L1 started playback and
Start moved between tracks). Press **M > BUTTON CODES** to check them on
your unit (below).

| Button | Code | Genvolve default |
|---|---|---|
| Volume - / + | joy 1 / joy 2 | not used |
| A | joy 3 | a |
| B | joy 4 | b |
| Y | joy 5 | y |
| X | joy 6 | x |
| L1 | joy 7 | l |
| R1 | joy 8 | r |
| Select | joy 9 | select |
| Start | joy 10 | start |
| M / Menu | joy 11 | menu |
| Stick press | joy 12 | roll (quick roll of the current track) |
| L2 | joy 13 | l2 (previous track) |
| R2 | joy 14 | r2 (next track) |
| D-pad | hat 0 (up/right/down/left) | up/right/down/left |
| Left stick | axes 0 (x) and 1 (y) | x: prev / next (previous / next step); y: vup / vdown (velocity or value); check the axis numbers on BUTTON CODES |

L2 and R2 are plain on/off buttons, not analog trigger axes. Note Y comes
before X. Anbernic's stock firmware numbers the same buttons 3 lower
(A = joy 0 ... Start = joy 7, L2/R2 = joy 10/11); the comments in
`knulli/seq4h.input` hold that table too.

`knulli/seq4h.input` holds this map and sets `mode joystick`. Copy it next
to the binary, replacing any older copy (an old file overrides the built-in
defaults). Joystick mode matters: some H700 pads report an ID that SDL
mistakes for a different controller, and SDL's game-controller layer would
then scramble the buttons. Raw joystick mode reads the codes above directly.

### Checking the codes on your device

No terminal needed. Either:

- open the main menu with **M**, go to **BUTTON CODES** and press **A**, or
- start **SHOW-BUTTONS** from the Ports menu (useful if M itself does not
  work), which starts Genvolve with the codes already showing.

The top right of the screen then shows each press as its raw code and the
Genvolve button it does, for example `JOY 10 = START` or `JOY 12 = NONE`. The
buttons keep working while the codes show. The same lines go to `seq4h.log`,
along with each pad's name, GUID and its numbers of buttons, hats and axes.
If a button shows the wrong action, add a line for its code to
`seq4h.input`, e.g. `joy 12 undo`. Turn the codes off again from the menu.
On a computer, `./seq4h --show-input` does the same.

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

`BUTTON` is one of `up down left right a b x y l r l2 r2 select start menu`, a
shortcut action `page` (next page), `roll`, `undo`, `lock` (this step),
`quit`, `prev` / `next` (step) or `vup` / `vdown` (stick up / down), or
`none` to unmap. Two settings also go in the
file:

- `mode auto` (default): pads SDL recognises use the `pad` lines, other
  joysticks use `joy`/`hat`/`axis`. `mode joystick` always uses the raw
  lines; `mode controller` only opens recognised pads.
- `threshold N` (default 16000, out of 32767): how far an axis must move to
  count as a press.

Examples (Knulli codes): swap A and B with `joy 3 b` and `joy 4 a`; make the
stick press undo the last roll instead of rolling with `joy 12 undo`.

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
  That is Genvolve's default (track 1 on channel 1 ... track 4 on channel 4), so
  the four sequencer tracks drive the four Felucca tracks with no setup.
- Other `MIDI IN` modes: **CH5-8**, **CH9-12**, **CH13-16** (another block of
  four, for example for two FM-1s on one cable), **SEL** (every channel plays
  the *selected* track) and **TRACK** (each track on its own channel, 1-16, OMNI
  or OFF, set in `MENU > MIDI > T1-T4 CHANNEL`).
- Felucca responds to notes, pitch bend, sustain and panic, and to MIDI clock
  from internal, USB or TRS. It also responds to track CCs (5 glide, 7 level,
  10 pan, 71 resonance, 72/73/75 release/attack/decay, 74 brightness,
  91/93/94 reverb/chorus/delay sends) and Program Change (loads a track's
  sound by its preset number). Genvolve does not send CCs or Program Change yet.
- Each Felucca track has a voice mode (EDIT > VOICE, `VCE`: POLY, MONO, LEG,
  UNI). For Genvolve's POLY tracks set it to **POLY**, or the FM1 plays only one
  note of each chord. In POLY most engines play up to 8 notes per track (FM6
  6, VOICE 4, GRAIN and PHYS 3), but all four tracks share 8 voices; when
  they run out the oldest released note goes first, and a track's lowest
  held note (its bass) is kept. DRUM always plays its lanes together. (From
  the Felucca firmware source, `core.h` and `voice.c`.)

Setup:

1. Connect the FM1's USB-C to the handheld's USB-C (its only port, also used
   for charging). The handheld must act as USB host. Other H700 projects
   report this works, but it is **unverified** for Genvolve, as is whether it
   powers the FM1 and whether Knulli's kernel has the USB MIDI driver. Run
   `USB-MIDI-CHECK` from Ports to find out; see the checklist in
   [NOTES.md](NOTES.md).
2. On the FM1, set `MENU > MIDI > MIDI IN` to **CH1-4** (see the wiki for the
   current default).
3. Set the FM1's **clock source to USB** so it follows Genvolve's tempo: it is
   on the CLOCK page (tap HOME to cycle HOME, MIXER, CLOCK). Without it the
   FM1 keeps its own clock and only the notes line up.
4. Run `./seq4h --midi list` and check the FM1 appears, then
   `./seq4h --midi FM --midi-test` to hear one note on each track, then
   `./seq4h --midi FM` (or part of the name the list shows).

If the FM1 is unplugged or plugged in after start, Genvolve shows
`MIDI: WAITING FOR ...` and connects when it appears. Without the ALSA
sequencer in the kernel, Genvolve uses raw MIDI devices (`hw:C,D,S`) instead.

The header also shows the handheld's battery, e.g. `BAT 78%` (`+` while
charging, amber below 20%, red below 10%). The level is written to `seq4h.log`
at start, every hour, when the charger is plugged in or out, and at quit.

For the other `MIDI IN` modes:

- **CH5-8 / CH9-12 / CH13-16:** set each track's channel in Genvolve on the
  TRACK page.
- **SEL:** run `./seq4h --channel N` so all four tracks use one channel. The
  FM1 then plays them all on the track you've selected on the device.
- **TRACK:** set the matching channels in Genvolve the same way.

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

- `dist/genvolve`, the aarch64 binary
- `dist/genvolve-knulli-aarch64.zip`, holding `GENVOLVE.sh`, `SHOW-BUTTONS.sh`, `USB-MIDI-CHECK.sh`,
  `genvolve/genvolve` and `genvolve/seq4h.input`. Unzip it into `/userdata/roms/ports/` and refresh the
  game list. The zip keeps the execute bits.

The output is byte-for-byte reproducible. GitHub Actions runs the same script
on every change (workflow *Genvolve aarch64 build*, artifact
`genvolve-knulli-aarch64`). It has been run on a Knulli RG40XX-V driving an FM1.

### Moving from seq4h

Builds before the rename installed `SEQ4H.sh` and a `seq4h/` folder. To move
over (no SSH needed, the SD card on a computer is enough):

1. Copy `GENVOLVE.sh`, `SHOW-BUTTONS.sh`, `USB-MIDI-CHECK.sh` and the
   `genvolve` folder from the zip into `roms/ports/`, replacing the old
   `SHOW-BUTTONS.sh` and `USB-MIDI-CHECK.sh`.
2. Delete the old `SEQ4H.sh`, so the Ports menu shows only GENVOLVE. Keep the
   old `seq4h` folder for now.
3. Start GENVOLVE once. It copies `seq4h.pat`, `seq4h.midi` and
   `seq4h-song1.pat`... from `seq4h/` into `genvolve/` (it never overwrites a
   file already in `genvolve/`, and leaves `seq4h/` untouched), and logs each
   copy in `genvolve/genvolve.log`.
4. Once your patterns, songs and MIDI port look right, the `seq4h` folder can
   be deleted (or kept as a backup).

## Layout

- `src/engine.c` sequencer core (pure C, unit tested in `tests/`)
- `src/gen.c` scales, generators and evolution (also unit tested)
- `src/player.c` timing thread
- `src/midi_alsa.c` ALSA sequencer output; `src/midi_null.c` stub/logging output
- `src/ui.c` SDL2 interface, built-in font, no extra libraries
- `src/input.c` button mapping: defaults and the `seq4h.input` file (unit tested)

See [NOTES.md](NOTES.md) for design decisions, what is untested, and next steps.
