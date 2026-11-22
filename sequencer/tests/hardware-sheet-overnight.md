# Genvolve overnight build: one hardware test sheet

Everything built overnight on 2026-11-22, in one place: tracks 5 to 8,
polymeter and Euclidean rhythms, ratchets and trig conditions, harmony and
key changes, live playing and recording, and quicker navigation with undo.
The four parts below are the full sheets from each piece of work
(`tests/hardware-sheet-8tracks.md`, `-rhythm.md`, `-live.md`, `-nav.md`),
in a sensible order. Do them in any order, and skip a part if you run out of
time: each part stands on its own.

**Install:** the newest green run of **Genvolve aarch64 build** on branch **main** (artifact
**genvolve-knulli-aarch64**). Install it as usual (NOTES.md, *Test
checklist, How to install a new build*). Your saved pattern, slots, songs, DRUM MAP
and modulation carry over, and every new setting starts off, so your music
sounds the same until you change something.

Everything is done on the handheld and the FM1 (plus the Blackbox 2 or the
Digitone for part A tests 7 and 8). No SSH. Tick **PASS** or **FAIL** and
write what you saw.

**Before you start:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual. Your normal pattern on T1-T4 (T4 a drum track
if you can).

**One thing that feels different everywhere:** M now acts when you let go
of it. A quick tap still opens and closes the main menu; holding it opens
the new jump panel (part D).

| Part | What | Tests | About |
|---|---|---|---|
| A | Tracks 5 to 8 | 10 | 20 min |
| B | Polymeter, Euclid, ratchets, conditions and harmony | 17 | 34 min |
| C | Live playing and recording (PERFORM, ARRANGE, RECORD) | 20 | 40 min |
| D | Quick navigation, HELP and undo history | 12 | 24 min |

If something fails, write down the test number (e.g. C-R4 or D8) and what you saw; that is all I need.

---

## Part A: Tracks 5 to 8

*From `tests/hardware-sheet-8tracks.md`.*

**Before this part:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual. Your normal pattern on T1-T4.

#### A1. Your old pattern is all there
1. Start GENVOLVE.

**Expected:** the same pattern plays as before the update (same BPM,
tracks, slots, mutes). The top right of the GRID shows **T1-4** in yellow
and **/ T5-8** dim, with no coloured dots after it.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A2. Scroll down to page 2
1. On the GRID, put the cursor on T4 and press **Down**.
2. Press **Up**.
3. Press **R2** four times, then **L2** four times.

**Expected:** 1 shows **T5 T6 T7 T8** (pink, teal, lime, purple), all empty,
labels **C5 C6 C7 C8** (their MIDI channels), and **T5-8** is now yellow
top right. 2 goes back to T4 on the first page. 3 steps T1 to T5 and back.
**Up** from T1 goes round to T8, **Down** from T8 to T1.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A3. Put notes on T5
1. On T5, press **A** on a few steps. Go back up to T1-T4.

**Expected:** the steps light up in pink. Back on page 1 a **pink dot**
shows after **T5-8** top right (T5 has notes). The FM1 plays nothing new
(channel 5 is not one of its MIDI IN CH1-4).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A4. Track pages, mute and solo on T5-T8
1. On T5, open **STEP**, **TRACK** and **GENERATE** (L1/R1). Header shows **T5**.
2. Back on the GRID, go left of step 1 on T6: **A** mutes it, **B** solos it.
   Undo both.

**Expected:** all pages work on T5-T8 as on T1-T4 (TRACK shows MIDI CHANNEL
6 for T6). Solo of T6 silences T1-T4 too; un-solo brings them back.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A5. NEW FROM STYLE leaves empty T5-T8 alone
1. Clear T5's steps again (or skip if you'd rather keep them, and note it).
2. **M**: rows **T1 ... T8**. With T5-T8 empty, T5-T8 read **KEEP AS IT IS**.
3. Change **MUSIC STYLE** left/right, then **NEW FROM STYLE**, **A**, **A**.

**Expected:** T1-T4 get the new style (and its tempo and key, since T5-T8
were empty). T5-T8 stay empty.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A6. Give T5-T8 their own parts
1. **M**, go down to **T5**, left/right until it reads e.g. **TECHNO KICK**;
   T6 **TECHNO HATS**, T7 **AMBIENT MOTIF**, T8 **AMBIENT HIGH**. Leave T1-T4
   on **KEEP AS IT IS**.
2. **NEW FROM STYLE**, **A**, **A**.

**Expected:** only T5-T8 change (message `NEW: T5 T6 T7 T8. KEPT: T1 T2 T3
T4`), tempo and key stay. T1-T4 play on as before. **MUSIC STYLE** changes
now also set T5-T8's rows (they have notes).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A7. A second instrument on channels 5-8
Connect the Blackbox 2 or the Digitone so it gets the same MIDI as the FM1
(through the splitter/hub, or the DIN cable). On the **Blackbox 2** set its
tracks to MIDI IN channels 5-8 (or set the T5-T8 channels to what its pads
already use, on each track's TRACK page). On the **Digitone**, set TRACK 1-4
channels to 5-8 in SETTINGS > MIDI CONFIG > CHANNELS.

**Expected:** the FM1 plays T1-T4, the second instrument plays T5-T8, both
in time (the handheld is the clock for both). Note which instrument you used.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A8. Drum track on T5 for the Blackbox
1. On T5, TRACK page, **TYPE: DRUMS**. M > DEVICE + CLOCK, BLACKBOX 2 only if
   the FM1 is not connected (it changes the drum map for all drum tracks).
2. Open the drum grid on T5 (Select), add kick and hat hits.

**Expected:** the drum grid works on T5; the right instrument plays the hits.
M > DRUM MAP says **PLAYS ON T5, MIDI CHANNEL 5**.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A9. Saves and songs keep T5-T8
1. M > SONG SAVE, **X** on an empty song. Quit (Select + Start) and start
   GENVOLVE again.
2. Load the song.

**Expected:** after the restart T5-T8 are as you left them; the song page
lists **T1-T4 and T5-T8** in two columns, and loading it brings T5-T8 back.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### A10. Modulation on T5-T8 (only if you use it)
1. M > MODULATION, **R1** (CC DEST), **SEND TO**: left/right.

**Expected:** the choices are **T1 ... T8**, then **T1-T4**, **T5-T8**,
**T1-T8**, then **CH 1-16**. Any destination you had set before the update
still points where it did.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

Write the results into NOTES.md, *Results so far* (row *Tracks 5-8 (D10)*),
or just send them in the thread.

---

## Part B: Polymeter, Euclid, ratchets, conditions and harmony

*From `tests/hardware-sheet-rhythm.md`.*

**Before this part:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual. Load or roll a pattern with notes on T1 and T2
(AMBIENT is fine).

### Part 1: polymeter and Euclidean rhythms

#### B1. A slower track (STEP RATE)
1. Go to **TRACK** (R1 twice from the grid), pick **T1**, and go **up** from
   **TRACK** to the second-last row, **STEP RATE**. It reads **1/16 (NORMAL)**.
2. Press **Start**. Press **right** twice: **1/8**.
3. Press **right** three more times: **1/2**. Then **left** back to **1/16**.

**Expected:** at 1/8, T1 plays at half speed against the other tracks and
its held notes last twice as long; the grid's T1 box shows **1/8** under the
generator name and the info line reads **LEN 16 1/8**. At 1/2 each T1 step
lasts half a bar. Back at 1/16 it is as before. The FM1 stays in time.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B2. Triplets against straight time
1. T2: **STEP RATE** **1/16T** (one left from 1/16), **LENGTH** **12**.
2. Listen for 4 bars.

**Expected:** T2 runs three steps to the beat over the others' four, and
comes round once a bar. Swing (if you have any) does not move T2's steps.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B3. REALIGN
1. Make T1 **LENGTH 13** and T2 **LENGTH 11** (STEP RATE back to 1/16).
   Watch the playheads drift apart on the grid.
2. **TRACK > REALIGN ALL**: press **right** until it reads **EVERY 4 BARS**.
3. Set it back to **OFF**, let the playheads drift, then press **A** on the row.

**Expected:** 2: every 4 bars, on the bar line, every track that is not on
its step 1 jumps back to it, so the tracks meet again; no doubled notes. 3:
**A** brings every track back to step 1 at once ("ALL TRACKS BACK TO STEP 1").

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B4. A Euclidean drum lane
1. Use your drum track (T4 if FM1 track 4 is on DRUM). Go to **GENERATE**.
2. Rows under **BUSY**: **EUCLID LANE** (pick **KICK** with left/right),
   **HITS** (press **right** until **5 OF 16**), **ROTATE** (press **right** twice).
3. Press **A** on **GENERATOR** (roll the kit) a few times.

**Expected:** 2: the kick changes straight away to 5 evenly spread hits
(the classic 5-in-16), and ROTATE moves them later by a step each press. 3:
the other lanes change on each roll, the kick keeps its Euclidean pattern.
**HITS** back to **OFF (KIT)**: the next roll uses the kit's own kick again.
**X** on GENERATE undoes a HITS or ROTATE change.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B5. EUCLID transform
1. On a note track with 4 or 5 notes bunched together, go to **GENERATE >
   TRANSFORM**, pick **EUCLID**, **BY +0**, press **A**.
2. Set **BY** to **+1** and press **A** again.
3. Try it on the drum track too.

**Expected:** 1: the same notes, in the same order, now spread evenly over
the track. 2: the whole pattern one step later. Locked steps never move.
3: each lane's hits are spread evenly, same count, same velocities. **X**
undoes.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B6. Saved
1. Quit (Select + Start) and start Genvolve again.

**Expected:** STEP RATE per track, REALIGN, and the Euclidean lanes are as
you left them.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

### Part 2: ratchets and trig conditions

New settings start **off** here too: no step has a ratchet or a condition
until you give it one, and EVO RATCHET+COND is off on every track.

#### B7. A ratchet on a note
1. On the grid, put the cursor on a T1 note and press **X** (STEP page).
2. Go **up** from **STEP** twice: **RATCHET**. Press **right** twice: **x3 IN THE STEP**.
3. Press **Start**. Then try **x2**, **x4** and **x8**.

**Expected:** the note plays 3 quick times inside its step, evenly spaced,
in time with the FM1. Back on the grid the block shows thin lines splitting
it into 3, and the info line ends with **X3**. x8 at 1/16 is a fast buzz.
With T1's STEP RATE at **1/4**, an x4 plays four 1/16s.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B8. Every other pass (CONDITION)
1. Same STEP page, row **CONDITION** (up once from STEP). Press **right**:
   **1:2 PASS 1 OF 2**. Press **right** again: **2:2**.
2. Listen for 4 passes of T1 with 1:2, then with 2:2.
3. Keep going right: **1:3**, **2:3** ... **8:8**, **FIRST**, **NOT FIRST**,
   **PRE**, **NOT PRE** (hold **Y** to jump 4 at a time).

**Expected:** 1:2 plays the note on passes 1, 3, 5 ...; 2:2 on passes 2, 4,
6 ...; FIRST only on the first pass after Start; NOT FIRST on every pass
but the first. The grid block has a small cyan square in its bottom left
corner. A step's chance (PROBABILITY) still applies on top.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B9. Taking turns (PRE / NOT PRE)
1. Give one T1 note **1:2**, the next note **PRE**, and the one after **NOT PRE**.

**Expected:** pass 1 plays the first two notes, pass 2 only the third, and
so on: the two later notes take turns, following the first one.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B10. Drum hits
1. On your drum track, go to the **STEP** page on a step where the kick hits.
2. Move onto the **KICK** row, then **down** past the lanes to **KICK RATCHET**
   and **KICK COND** (they follow the last lane row you were on). Set
   **x2** and **2:2**. Do the same for a hat with **x4**.
3. Open the drum grid (**Select** on the grid).

**Expected:** a kick roll of 2 on every second pass, a hat roll of 4. On
the drum grid the hits show the split lines and the cyan square, and the
info line reads e.g. **STEP 01 KICK VEL 72 CHANCE 69% X2 2:2**.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B11. RATCHET and VARY transforms, and EVO RATCHET+COND
1. GENERATE > TRANSFORM: **RATCHET**, **BY +3**, press **A**. Then **VARY**, **BY +4**, **A**.
2. **BY 0** on either clears them all (**0 (CLEAR ALL)**); **X** undoes.
3. Turn on **EVOLVE** (every 1 bar, amount 50%) with only **EVO RATCHET+COND** on.

**Expected:** 1: three notes get a ratchet (x2 to x4) and four get a
condition (mostly 1:2 or 2:2, sometimes 1:4, 3:4, 1:3 or NOT FIRST), so the
pattern changes from pass to pass. 3: now and then a ratchet or a
condition appears or goes, the notes themselves stay. Locked steps never
change. Works on drum tracks too.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B12. Saved
1. Save a slot, quit and start again, then load the slot.

**Expected:** ratchets and conditions come back on the live pattern, the
slot and the anchor, for notes and drum hits.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

### Part 3: harmony and key changes

HARMONY starts **OFF**, so nothing changes until you pick one. Every note
track is set to follow it (**MOVE**); NEW FROM STYLE sets bass parts to
**ROOT (BASS)** and drum parts to not follow.

#### B13. A chord progression
1. Play a pattern with notes on T1-T3 (AMBIENT or MELODIC).
2. Go to **GENERATE** and press **up** three times from **TRACK**: **HARMONY**.
   Press **right** three times: **I-VI-IV-V**. **BARS EACH** (one down) reads **2**.
3. Listen for 8 bars. Try **BARS EACH 1** and **4**.

**Expected:** every 2 bars the whole set moves together to the next chord
(I, VI, IV, V, then round). The header shows the chord after the scale,
e.g. **C MIN PENT  IV**, and the HARMONY row reads **NOW IV**. The
patterns on the grid don't change: only what is sent does.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B14. How each track follows
1. On the row under BARS EACH, **T1 FOLLOWS**, try each value on a bass-ish
   T1 (L2/R2 picks the track): **OFF**, **MOVE**, **CHORD TONES**, **ROOT (BASS)**.
2. Set a melodic track to **CHORD TONES**.

**Expected:** OFF plays as written. MOVE moves the whole line up the
scale with each chord. ROOT makes every note the chord's root, at or just
below where it was written (a bass that follows the changes). CHORD TONES
keeps the melody's shape but moves each note to the nearest note of the
chord, so it always fits. Drum tracks show **- (DRUMS)** and never move.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B15. DRIFT
1. **HARMONY** > **DRIFT** (the last one), **BARS EACH 4**.

**Expected:** every 4 bars the chord changes to a nearby one at random
(home on I most often, then IV, V, VI), never the same chord twice in a
row. Good for long ambient takes.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B16. A key change every track follows
1. **KEY CHANGE** (under ROOT) reads **SNAP INTO KEY**. Press **right**: **MOVE ALL (BAR)**.
2. While playing, go up to **ROOT** and press **right** twice (C to D).
3. Stop, change ROOT again, and look at the grid.

**Expected:** 2: the ROOT row reads **C TO D AT BAR** and on the next bar
line every note track jumps up a whole tone together, in time; drum tracks
and drum parts stay. 3: stopped, it moves at once; the grid's notes now
read in the new key (e.g. NOTE D2 where it was C2). It always takes the
shorter way (up to 6 semitones up or 5 down). Slots and the anchor move too.
**KEEP NOTES** and **SNAP INTO KEY** work as before.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### B17. Saved
1. Quit and start again.

**Expected:** HARMONY, BARS EACH and each track's FOLLOWS are as you left them.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

## Part C: Live playing and recording (PERFORM, ARRANGE, RECORD)

*From `tests/hardware-sheet-live.md`.*

**Before this part:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual. Have a pattern that plays on all four tracks,
with T4 a drum track (TRACK > TYPE > DRUMS) if you can.

### Recording helpers

#### C-R1. Find the page
1. Press **M**, go down to **LIVE + RECORD**, press **A** (PERFORM opens), then **R1**.

**Expected:** the **RECORD** page: COUNT-IN OFF, CLICK OFF, SONG LENGTH OFF
(ENDLESS), STOP AT ONCE, SONG POSITION OFF, START FROM MARK OFF, SET MARK
NOW, PUT MARK BACK. The right side explains the selected row. **B** goes back
to the menu, **M** closes it.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-R2. Count-in with a click
1. Set **COUNT-IN** to **1 BAR** and **CLICK** to **ON**.
2. Press **Start**.

**Expected:** the header shows **IN 4**, **IN 3**, **IN 2**, **IN 1** (yellow,
COUNT-IN under it) while T4's closed hat clicks four times, the first one
louder. Then the pattern starts on the next beat, and the header reads
**PLAYING** with **BAR 1.1** under it. The FM1's tempo did not jump.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-R3. The FM1 starts with the pattern, not with the count-in
1. Stop. On the FM1, watch its play indicator or step LEDs.
2. Press **Start** again (count-in still 1 bar).

**Expected:** the FM1 stays stopped during the four clicks and starts on
bar 1 together with Genvolve. (If Felucca ignores MIDI Start/Stop, write
that down: then it simply plays what it is sent.)

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-R4. Stop during the count-in
1. Press **Start**, then **Start** again during the clicks.

**Expected:** it stops at once, no stuck click note, header **STOPPED**.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-R5. Song length
1. Set **COUNT-IN** **OFF**. Set **SONG LENGTH** to **4 BARS** (right four times; hold **Y** for 16 bars a step).
2. Press **Start** and wait.

**Expected:** the header shows **BAR 1.1 / 4** counting up. At the end of
bar 4 it stops by itself, exactly on the bar line, with no stuck notes.
The row also shows the time at this BPM (4 bars at 120 = 0:08).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-R6. Stop at the bar end
1. Set **SONG LENGTH** back to **OFF** and **STOP** to **AT BAR END**.
2. Press **Start**, and in the middle of a bar press **Start** again.
3. Start again, press **Start** mid-bar, then **Start** once more straight away.

**Expected:** 2: the header reads **ENDING** and playback stops on the next
bar line. 3: the second press stops at once.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-R7. Same take every time (start mark)
1. On the GENERATE page, make sure T1 evolves (EVOLVE every 1 bar, AMOUNT 50 or more).
2. Back on **RECORD**: **A** on **SET MARK NOW** (MARK: SET on the right), and set **START FROM MARK** **ON**.
3. Press **Start**, listen for 8 bars, **Stop**. Press **Start** again and listen.

**Expected:** both takes sound the same: the pattern starts from the marked
version and evolves the same way each time. With **START FROM MARK OFF**,
Start carries on from wherever evolution got to. **PUT MARK BACK** puts
the marked patterns back at any time.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-R8. Recording into a DAW or the Blackbox (optional)
1. Set the DAW or Blackbox 2 to follow MIDI clock and Start/Stop, arm recording.
2. **SONG POSITION ON**, **COUNT-IN 1 BAR**, press **Start**.

**Expected:** recording starts on bar 1 together with Genvolve (not during
the count-in), and it stops when Genvolve stops. Write down if the device
got confused by SONG POSITION (then leave it OFF).

[ ] PASS  [ ] FAIL   Note: ______________________________

---

### Performance page and scenes

#### C-P1. The page
1. **M** > **LIVE + RECORD** > **A**.

**Expected:** PERFORM: rows T1-T8 (unused T5-T8 dim, EMPTY), ALL, SCENE,
SONG, SWITCH AT. While playing, a green square lights next to tracks that
are sounding. **R1** goes to RECORD, **R1** again back.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-P2. Mute, solo, all
1. Play. On T1 press **A** (mute), then **A** again. On T2 press **B** (solo), then **B** again.
2. On ALL press **A**, then **A** again. Solo two tracks, then **B** on ALL.

**Expected:** 1: T1 drops out and comes back; only T2 plays while soloed.
2: everything mutes, then everything plays; B on ALL clears both solos.
Same mutes show on the GRID's track boxes.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-P3. Density
1. Play. On a melodic track press **left** five times (DENSITY -50), listen; then **right** ten times (+50).
2. Do the same on the drum track.

**Expected:** -50: noticeably fewer notes, the first beat mostly kept. +50:
extra softer notes in the gaps, using the track's own notes. Drums: fewer
hits / soft extra hats and snares, the kick never gets extras. The GRID
pattern itself does not change. Back at 0 it plays exactly as written.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-P3b. Fills
1. Play. On the drum track's row **hold X** for a bar, then let go.
2. On a melodic track hold **X**; then on **ALL** hold **X**.

**Expected:** 1: hats on every 16th and a snare roll rising through the
second half of each bar while held; the kick stays as it was; letting go
puts the beat straight back. 2: the track plays every step, the last beat
of each bar an octave higher; ALL does it on every track at once. **FILL**
shows next to the track while held.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-P4. Evolve from the page
1. On a track with EVOLVE OFF, hold **Y** and press **right** twice (or push the stick up).

**Expected:** EVOLVE reads 20% (stick: 5% a push) and the pattern starts
changing every 4 bars (GENERATE shows EVERY 4 BARS).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-P5. Save a scene and switch at the bar
1. On **SCENE 1** press **X** (saves every track into slot 1).
2. Change a few steps on the GRID, or roll a track. Back on PERFORM, **SCENE 2**, **X**.
3. Play. Pick **SCENE 1**, press **A** in the middle of a bar.

**Expected:** the row and the header show **SCENE 1 IN 4 ... IN 1**; on the
next bar line every track switches to its scene-1 pattern together. **B**
before the bar line cancels.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-P6. Switch every 4 bars
1. **SWITCH AT** 4 BARS. Play, queue SCENE 2 in bar 2.

**Expected:** the countdown shows **IN 3 BARS**, then **IN 2 BARS**, then
beats, and the switch happens on the bar-5 line (counted from Start).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-P7. Queue a song
1. Save two different songs on M > SONG SAVE (say 1 and 2).
2. With song 1 playing, PERFORM > **SONG 2** > **A**.

**Expected:** **SONG 2 IN ...** counts down, then song 2 comes in on the
bar line without a stop, no stuck notes, the FM1 keeps its clock. The
header briefly says NOW PLAYING SONG 2. RECORD settings are unchanged.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

### Arranger

#### C-A1. Build an arrangement
1. Make three scenes on PERFORM (SCENE 1, 2, 3: change the patterns between, X on each).
2. **R1** to ARRANGE. **A** on ARRANGE (it turns ON and adds section 1).
3. On section 1: **right** until 4 BARS. **A** (adds a copy). On section 2 hold **Y** + **right** (SCENE 2), **X** (DRUM FILL).
4. **+ ADD SECTION**, **A** (section 3, SCENE 3). **AT THE END**: STOP.

**Expected:** the side timeline shows three coloured blocks sized by bars,
with a white edge on the section that has a fill; the total bars and time
are right (12 bars at 120 = 0:24).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-A2. Play it
1. Press **Start** and listen to the whole thing.

**Expected:** scene 1 for 4 bars, scene 2 for 4 bars with a drum fill in its
4th bar, scene 3 for 4 bars, then it stops by itself on the bar line. The
white playhead moves along the timeline, NOW: SECTION n, BAR n OF n.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-A3. Loop, remove, and saved with the song
1. AT THE END: LOOP. Start: after section 3 it goes back to section 1.
2. On section 2 press **Select**: it is removed.
3. Save the song (M > SONG SAVE, X), load another song, then load this one again.

**Expected:** 1 loops seamlessly. 2 leaves sections 1 and 3. 3: the
arrangement comes back with the song.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### C-A4. Record a whole arrangement
1. RECORD: COUNT-IN 1 BAR, ARRANGE AT THE END: STOP. DAW / Blackbox armed and following clock.
2. Press **Start**.

**Expected:** one take from bar 1 to the last section's last bar, with MIDI
Start after the count-in and Stop at the end.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

## Part D: Quick navigation, HELP and undo history

*From `tests/hardware-sheet-nav.md`.*

**Before this part:** a pattern that plays on T1-T4, FM1 connected as usual.
Stop playback unless a test says otherwise.

#### D1. A tap of M still opens the menu
1. On the GRID, tap **M** (press and let go straight away).
2. Tap **M** again.

**Expected:** the main menu opens on the first tap and closes on the second,
back on the GRID. It opens as you let go, not as you press.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D2. Hold M: the jump panel
1. On the GRID, hold **M** for about half a second and keep holding.

**Expected:** a panel titled **JUMP TO** covers the screen with 18 tiles in 4
columns: GRID, STEP, TRACK, GENERATE, PIANO ROLL (or DRUM GRID on a drum
track), ALL STEPS, MAIN MENU, PATTERNS, SONGS, DRUM MAP, MIDI OUT, DEVICE,
MODULATION, PERFORM, ARRANGE, RECORD, HISTORY, HELP. GRID says **YOU ARE
HERE**. The track (T1) shows at the top, with UNDO and REDO counts on the
right.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D3. Pick a page and let go
1. Still holding **M**, use the **d-pad** to move the yellow box to **SONGS**.
2. Let go of **M**.

**Expected:** SONG SAVE opens. B goes back to the main menu, M closes it
(back to the GRID).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D4. Back to the last page in one move
1. Go to the **GENERATE** page (R1 three times from the GRID), then back to the GRID with **B**.
2. Hold **M** until the panel shows, then let go without touching anything else.

**Expected:** the panel starts on **GENERATE** (marked LAST PAGE), so letting
go takes you straight back to GENERATE. Do it again and you are back on the
GRID.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D5. Cancel, A and track change in the panel
1. Hold **M**, move to any tile, press **B**, then let go of M.
2. Hold **M**, press **R2** twice, move to **TRACK**, press **A** (still holding M), then let go.

**Expected:** step 1: the panel closes on B and nothing else happens (you stay
on the page, the menu does not open). Step 2: the track at the top of the
panel goes T1, T2, T3; A opens the TRACK page for **T3** straight away.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D6. Jump into the live and MIDI pages
1. Hold **M**, pick **PERFORM**, let go. Then hold M, pick **RECORD**, let go.
2. Hold **M**, pick **MODULATION**, let go. Tap **M**.

**Expected:** PERFORM, then RECORD open directly (L1/R1 still move between
PERFORM, ARRANGE and RECORD). MODULATION opens the LFO page. The tap of M on
any of these closes it, back to the GRID.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D7. HELP for the page you are on
1. Go to the **piano roll** (Select on the GRID on a note track).
2. Hold **M**, pick **HELP**, let go.
3. Press **down** a few times, then **B**.

**Expected:** **HELP: ROLL** lists the piano roll's buttons (white) and what
they do, then an **EVERYWHERE** section (L1/R1, L2/R2, M tap and hold, Select
+ X undo, Select + B redo ...). Down scrolls. B goes back to the piano roll.
Also try **M > HELP** from the main menu: it shows the help of the page you
opened the menu from.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D8. Undo several edits, then redo
1. On the GRID turn on 3 empty steps on T1 with **A** (three different steps).
2. Hold **Select** and press **X** three times.
3. Still holding Select, press **B** once.

**Expected:** each Select + X takes back one step, newest first; the header
says e.g. **UNDO STEP S5 T1 (2 MORE)**. After three, all three steps are off
again. Select + B (redo) puts the first one back (**REDO ...**). A fourth
Select + X with nothing left says **NOTHING TO UNDO**.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D9. Holding a value is one undo
1. On the **STEP** page, hold **right** on VELOCITY until it has gone up a lot.
2. Select + X once.

**Expected:** one undo puts the velocity back to where it started (repeats of
the same change within about 1.5 seconds are one entry).

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D10. UNDO HISTORY page
1. Make a few different edits: roll T2 (stick press, or Select + A), change a
   step on T1, and on GENERATE apply a transform (e.g. REVERSE) to T3.
2. Hold **M**, pick **HISTORY**, let go.
3. Move down to the second line below **NOW** and press **A**.
4. Move up above **NOW** and press **A** on the top line.

**Expected:** step 2: a list with **NOW** at the top and the edits below it,
newest first (e.g. BACK 1 REVERSE T3, BACK 2 STEP S4 T1, BACK 3 ROLL T2).
Step 3: **WENT BACK 2 EDITS**; T3 and T1 are as before those edits; the two
lines move above NOW as **FWD** lines. Step 4: **WENT FORWARD 2 EDITS**, both
edits are back. Undo also works while playing.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D11. Old undo buttons still work
1. On the GRID's track box (left of step 1), tap **X** to roll the track, then
   hold **X** for half a second.
2. On GENERATE press **A** (roll), then **X**.

**Expected:** both undo the roll, as before. They now share the history with
Select + X.

[ ] PASS  [ ] FAIL   Note: ______________________________

#### D12. Loading a song clears the history
1. Make an edit, then load a song (M > SONG SAVE, A, A).
2. Select + X.

**Expected:** **NOTHING TO UNDO** (undo never steps into the other song).
SONG SAVE's own **UNDO LOAD** still brings the old session back.

[ ] PASS  [ ] FAIL   Note: ______________________________
