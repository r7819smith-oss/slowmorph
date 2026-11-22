# Genvolve test sheet: polymeter, Euclidean rhythms, ratchets, conditions and harmony

Install the build linked in the PR (see NOTES.md, *Test checklist > How to
install a new build*). Your saved pattern, slots and songs carry over and
sound the same: every new setting starts **off** (STEP RATE 1/16, REALIGN
OFF, no Euclidean lanes).

Everything is done on the handheld and the FM1. No SSH. Tick **PASS** or
**FAIL** and write what you saw.

**Before you start:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual. Load or roll a pattern with notes on T1 and T2
(AMBIENT is fine).

---

## Part 1: polymeter and Euclidean rhythms

### 1. A slower track (STEP RATE)
1. Go to **TRACK** (R1 twice from the grid), pick **T1**, and go **up** from
   **TRACK** to the second-last row, **STEP RATE**. It reads **1/16 (NORMAL)**.
2. Press **Start**. Press **right** twice: **1/8**.
3. Press **right** three more times: **1/2**. Then **left** back to **1/16**.

**Expected:** at 1/8, T1 plays at half speed against the other tracks and
its held notes last twice as long; the grid's T1 box shows **1/8** under the
generator name and the info line reads **LEN 16 1/8**. At 1/2 each T1 step
lasts half a bar. Back at 1/16 it is as before. The FM1 stays in time.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 2. Triplets against straight time
1. T2: **STEP RATE** **1/16T** (one left from 1/16), **LENGTH** **12**.
2. Listen for 4 bars.

**Expected:** T2 runs three steps to the beat over the others' four, and
comes round once a bar. Swing (if you have any) does not move T2's steps.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 3. REALIGN
1. Make T1 **LENGTH 13** and T2 **LENGTH 11** (STEP RATE back to 1/16).
   Watch the playheads drift apart on the grid.
2. **TRACK > REALIGN ALL**: press **right** until it reads **EVERY 4 BARS**.
3. Set it back to **OFF**, let the playheads drift, then press **A** on the row.

**Expected:** 2: every 4 bars, on the bar line, every track that is not on
its step 1 jumps back to it, so the tracks meet again; no doubled notes. 3:
**A** brings every track back to step 1 at once ("ALL TRACKS BACK TO STEP 1").

[ ] PASS  [ ] FAIL   Note: ______________________________

### 4. A Euclidean drum lane
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

### 5. EUCLID transform
1. On a note track with 4 or 5 notes bunched together, go to **GENERATE >
   TRANSFORM**, pick **EUCLID**, **BY +0**, press **A**.
2. Set **BY** to **+1** and press **A** again.
3. Try it on the drum track too.

**Expected:** 1: the same notes, in the same order, now spread evenly over
the track. 2: the whole pattern one step later. Locked steps never move.
3: each lane's hits are spread evenly, same count, same velocities. **X**
undoes.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 6. Saved
1. Quit (Select + Start) and start Genvolve again.

**Expected:** STEP RATE per track, REALIGN, and the Euclidean lanes are as
you left them.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

## Part 2: ratchets and trig conditions

New settings start **off** here too: no step has a ratchet or a condition
until you give it one, and EVO RATCHET+COND is off on every track.

### 7. A ratchet on a note
1. On the grid, put the cursor on a T1 note and press **X** (STEP page).
2. Go **up** from **STEP** twice: **RATCHET**. Press **right** twice: **x3 IN THE STEP**.
3. Press **Start**. Then try **x2**, **x4** and **x8**.

**Expected:** the note plays 3 quick times inside its step, evenly spaced,
in time with the FM1. Back on the grid the block shows thin lines splitting
it into 3, and the info line ends with **X3**. x8 at 1/16 is a fast buzz.
With T1's STEP RATE at **1/4**, an x4 plays four 1/16s.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 8. Every other pass (CONDITION)
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

### 9. Taking turns (PRE / NOT PRE)
1. Give one T1 note **1:2**, the next note **PRE**, and the one after **NOT PRE**.

**Expected:** pass 1 plays the first two notes, pass 2 only the third, and
so on: the two later notes take turns, following the first one.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 10. Drum hits
1. On your drum track, go to the **STEP** page on a step where the kick hits.
2. Move onto the **KICK** row, then **down** past the lanes to **KICK RATCHET**
   and **KICK COND** (they follow the last lane row you were on). Set
   **x2** and **2:2**. Do the same for a hat with **x4**.
3. Open the drum grid (**Select** on the grid).

**Expected:** a kick roll of 2 on every second pass, a hat roll of 4. On
the drum grid the hits show the split lines and the cyan square, and the
info line reads e.g. **STEP 01 KICK VEL 72 CHANCE 69% X2 2:2**.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 11. RATCHET and VARY transforms, and EVO RATCHET+COND
1. GENERATE > TRANSFORM: **RATCHET**, **BY +3**, press **A**. Then **VARY**, **BY +4**, **A**.
2. **BY 0** on either clears them all (**0 (CLEAR ALL)**); **X** undoes.
3. Turn on **EVOLVE** (every 1 bar, amount 50%) with only **EVO RATCHET+COND** on.

**Expected:** 1: three notes get a ratchet (x2 to x4) and four get a
condition (mostly 1:2 or 2:2, sometimes 1:4, 3:4, 1:3 or NOT FIRST), so the
pattern changes from pass to pass. 3: now and then a ratchet or a
condition appears or goes, the notes themselves stay. Locked steps never
change. Works on drum tracks too.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 12. Saved
1. Save a slot, quit and start again, then load the slot.

**Expected:** ratchets and conditions come back on the live pattern, the
slot and the anchor, for notes and drum hits.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

## Part 3: harmony and key changes

HARMONY starts **OFF**, so nothing changes until you pick one. Every note
track is set to follow it (**MOVE**); NEW FROM STYLE sets bass parts to
**ROOT (BASS)** and drum parts to not follow.

### 13. A chord progression
1. Play a pattern with notes on T1-T3 (AMBIENT or MELODIC).
2. Go to **GENERATE** and press **up** three times from **TRACK**: **HARMONY**.
   Press **right** three times: **I-VI-IV-V**. **BARS EACH** (one down) reads **2**.
3. Listen for 8 bars. Try **BARS EACH 1** and **4**.

**Expected:** every 2 bars the whole set moves together to the next chord
(I, VI, IV, V, then round). The header shows the chord after the scale,
e.g. **C MIN PENT  IV**, and the HARMONY row reads **NOW IV**. The
patterns on the grid don't change: only what is sent does.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 14. How each track follows
1. On the row under BARS EACH, **T1 FOLLOWS**, try each value on a bass-ish
   T1 (L2/R2 picks the track): **OFF**, **MOVE**, **CHORD TONES**, **ROOT (BASS)**.
2. Set a melodic track to **CHORD TONES**.

**Expected:** OFF plays as written. MOVE moves the whole line up the
scale with each chord. ROOT makes every note the chord's root, at or just
below where it was written (a bass that follows the changes). CHORD TONES
keeps the melody's shape but moves each note to the nearest note of the
chord, so it always fits. Drum tracks show **- (DRUMS)** and never move.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 15. DRIFT
1. **HARMONY** > **DRIFT** (the last one), **BARS EACH 4**.

**Expected:** every 4 bars the chord changes to a nearby one at random
(home on I most often, then IV, V, VI), never the same chord twice in a
row. Good for long ambient takes.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 16. A key change every track follows
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

### 17. Saved
1. Quit and start again.

**Expected:** HARMONY, BARS EACH and each track's FOLLOWS are as you left them.

[ ] PASS  [ ] FAIL   Note: ______________________________
