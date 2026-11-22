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
