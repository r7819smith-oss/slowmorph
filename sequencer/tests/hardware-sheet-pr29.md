# seq4h test sheet: PR 29 (8 tests)

PR 29 is Roy's feedback on build #133: NEW FROM STYLE track by track, four
new styles with their own drum kits, a sparser ambient drum kit, and step
locks in the piano roll. Install the build linked in the PR (artifact
**seq4h-knulli-aarch64**) as in NOTES.md, *Test checklist > How to install
a new build*. Your saved pattern, slots and DRUM MAP carry over.

Everything is done on the handheld and the FM1. No SSH. Tick **PASS** or
**FAIL** and write what you saw.

**Before you start:** same as the #133 sheet. FM1 on **MIDI IN: CH1-4**,
clock source **USB**, its own pattern cleared. If one of your seq4h tracks
is a drum track (TYPE = DRUMS), the FM1 track with the same number must be
on Felucca's **DRUM** engine. For tests 3-5 it helps to have **FM1 track 4
on DRUM**: the new styles put their drums on T4.

---

### 1. The new menu rows
1. Press **M**. Under **MUSIC STYLE** there are four new rows, **T1** to **T4**.
2. On **MUSIC STYLE** press right/left to pick **HIP HOP**.
3. Go down to **T1** and press **right** a few times, then hold **Y** and press **right**.

**Expected:** 2: all four T rows change to HIP HOP (BASS, KEYS, LEAD, DRUMS;
a drum track shows **HIP HOP KIT**). 3: right steps through the parts (HIP
HOP KEYS, LEAD, DRUMS, then DRUM N BASS SUB ...); hold Y + right jumps a
whole style. The right side explains the rows.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 2. Replace just one track
1. Note the BPM. Set **T1**, **T2** and **T3** to **KEEP AS IT IS** (left from
   AMBIENT, or hold Y + left until it reads KEEP).
2. Set **T4** to **HIP HOP DRUMS** (or HIP HOP KIT on a drum track).
3. Go to **NEW FROM STYLE**, it reads **A: THE T ROWS**. Press **A**, then **A** again.

**Expected:** the first A says "PRESS A AGAIN TO REPLACE 1 TRACK". After
the second, the grid shows "NEW: T4. KEPT: T1 T2 T3". T1-T3 are exactly as
they were; T4 has a boom bap beat (kick on the one, snare on 2 and 4). The
BPM, swing and scale did **not** change.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 3. Mix styles across tracks
1. **M**, set the rows to: T1 **DRONE BASS**, T2 **AMBIENT ARP** (or TRIP HOP
   PAD), T3 **AMBIENT MOTIF**, T4 **TECHNO** kit or **DRUM N BASS DRUMS**.
   (Hold Y + left/right to get to a style, then left/right for the part.)
2. **NEW FROM STYLE**, A twice. Press **Start**.

**Expected:** every track is new, each in its own style: a held drone, an
arpeggio, a motif, and the drums. Tempo and scale come from the MUSIC STYLE
row (you changed every track). Nothing hangs when you stop.

[ ] PASS  [ ] FAIL   What it sounded like: ______________________________

### 4. The four new styles
For each of **HIP HOP**, **DRUM N BASS**, **HOUSE** and **TRIP HOP**: on MUSIC
STYLE pick it (all four T rows follow), NEW FROM STYLE, A twice, play a few bars.

**Expected:**
- HIP HOP: 88 BPM, swung. Bass on the chord roots, held seventh chords on T2, a sparse lead, boom bap drums.
- DRUM N BASS: 172 BPM. A sub bass holding long notes, a slow pad that swells, a short stab, two-step drums (kick on 1 and the "and" of 3, snare on 2 and 4).
- HOUSE: 124 BPM. An offbeat bass, chord stabs, a light arpeggio, four on the floor with a clap on 2 and 4 and open hats on the offbeats.
- TRIP HOP: 80 BPM. A slow bass, a dark pad, sparse bells, slow half-time drums (snare on 3).

[ ] PASS  [ ] FAIL   Which you liked / didn't: ______________________________

### 5. Drum kits follow the track's style
1. Make a drum track (TRACK page, TYPE = DRUMS) and give it **HOUSE KIT** with NEW FROM STYLE (other rows KEEP).
2. Go to **GENERATE** on that track and press **A** to roll a few times.

**Expected:** every roll stays a house beat (four on the floor), not
ambient. The drum grid (Select on the grid) shows it.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 6. AMBIENT is sparse again
1. **M**, MUSIC STYLE **AMBIENT**, NEW FROM STYLE, A twice (all four rows AMBIENT). Your drum track stays a drum track.
2. Play for a few minutes.

**Expected:** like the first ambient set you liked: slow, sparse, tracks
drifting against each other. The drum track plays only about two soft hits
a bar, many left to chance, so some bars have almost nothing. (The note
tracks are unchanged since build #68; the drum kit was the busy part. If
it still feels busy, try other FM1 sounds and say which track it is.)

[ ] PASS  [ ] FAIL   Note: ______________________________

### 7. Lock steps in the piano roll
1. On the grid, cursor on a note track, press **Select**: the piano roll opens.
2. Move to a step with a note, press **Y** and let go. Move to an empty step, press **Y** again.
3. Go to **GENERATE** and roll that track (A). Back to the piano roll (B, Select).
4. Hold **Y** + right on a note: its length still changes, and it does not lock.
5. Press **Y** on a locked step to unlock it.

**Expected:** 2: the top line says "STEP n LOCKED"; the locked note gets a
yellow corner and each locked step's column a dim yellow frame. 3: locked
steps did not change (the empty one stayed empty). 4: Y + direction is
length, not a lock. 5: "STEP n UNLOCKED". The grid shows the same locks.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 8. Restart keeps it all
1. Quit with **Select + Start** and start SEQ4H again.
2. On GENERATE roll the drum track: it is still the same style's kit. **M**: the T rows show each track's style and part.

**Expected:** patterns, piano roll locks, and each track's style are kept.

[ ] PASS  [ ] FAIL   Note: ______________________________
