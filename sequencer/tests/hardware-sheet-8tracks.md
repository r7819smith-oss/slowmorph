# Genvolve test sheet: tracks 5 to 8 (second page of tracks) (10 tests)

Install the build linked in the PR (see NOTES.md, *Test checklist > How to
install a new build*). Your saved pattern, slots, songs, DRUM MAP and
modulation carry over unchanged. T5-T8 start **empty**, on MIDI channels
**5-8**, so with only the FM1 connected nothing sounds different.

Everything is done on the handheld (and the FM1, plus the Blackbox 2 or the
Digitone for tests 7 and 8). No SSH. Tick **PASS** or **FAIL** and write what
you saw.

**Before you start:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual. Your normal pattern on T1-T4.

---

### 1. Your old pattern is all there
1. Start GENVOLVE.

**Expected:** the same pattern plays as before the update (same BPM,
tracks, slots, mutes). The top right of the GRID shows **T1-4** in yellow
and **/ T5-8** dim, with no coloured dots after it.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 2. Scroll down to page 2
1. On the GRID, put the cursor on T4 and press **Down**.
2. Press **Up**.
3. Press **R2** four times, then **L2** four times.

**Expected:** 1 shows **T5 T6 T7 T8** (pink, teal, lime, purple), all empty,
labels **C5 C6 C7 C8** (their MIDI channels), and **T5-8** is now yellow
top right. 2 goes back to T4 on the first page. 3 steps T1 to T5 and back.
**Up** from T1 goes round to T8, **Down** from T8 to T1.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 3. Put notes on T5
1. On T5, press **A** on a few steps. Go back up to T1-T4.

**Expected:** the steps light up in pink. Back on page 1 a **pink dot**
shows after **T5-8** top right (T5 has notes). The FM1 plays nothing new
(channel 5 is not one of its MIDI IN CH1-4).

[ ] PASS  [ ] FAIL   Note: ______________________________

### 4. Track pages, mute and solo on T5-T8
1. On T5, open **STEP**, **TRACK** and **GENERATE** (L1/R1). Header shows **T5**.
2. Back on the GRID, go left of step 1 on T6: **A** mutes it, **B** solos it.
   Undo both.

**Expected:** all pages work on T5-T8 as on T1-T4 (TRACK shows MIDI CHANNEL
6 for T6). Solo of T6 silences T1-T4 too; un-solo brings them back.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 5. NEW FROM STYLE leaves empty T5-T8 alone
1. Clear T5's steps again (or skip if you'd rather keep them, and note it).
2. **M**: rows **T1 ... T8**. With T5-T8 empty, T5-T8 read **KEEP AS IT IS**.
3. Change **MUSIC STYLE** left/right, then **NEW FROM STYLE**, **A**, **A**.

**Expected:** T1-T4 get the new style (and its tempo and key, since T5-T8
were empty). T5-T8 stay empty.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 6. Give T5-T8 their own parts
1. **M**, go down to **T5**, left/right until it reads e.g. **TECHNO KICK**;
   T6 **TECHNO HATS**, T7 **AMBIENT MOTIF**, T8 **AMBIENT HIGH**. Leave T1-T4
   on **KEEP AS IT IS**.
2. **NEW FROM STYLE**, **A**, **A**.

**Expected:** only T5-T8 change (message `NEW: T5 T6 T7 T8. KEPT: T1 T2 T3
T4`), tempo and key stay. T1-T4 play on as before. **MUSIC STYLE** changes
now also set T5-T8's rows (they have notes).

[ ] PASS  [ ] FAIL   Note: ______________________________

### 7. A second instrument on channels 5-8
Connect the Blackbox 2 or the Digitone so it gets the same MIDI as the FM1
(through the splitter/hub, or the DIN cable). On the **Blackbox 2** set its
tracks to MIDI IN channels 5-8 (or set the T5-T8 channels to what its pads
already use, on each track's TRACK page). On the **Digitone**, set TRACK 1-4
channels to 5-8 in SETTINGS > MIDI CONFIG > CHANNELS.

**Expected:** the FM1 plays T1-T4, the second instrument plays T5-T8, both
in time (the handheld is the clock for both). Note which instrument you used.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 8. Drum track on T5 for the Blackbox
1. On T5, TRACK page, **TYPE: DRUMS**. M > DEVICE + CLOCK, BLACKBOX 2 only if
   the FM1 is not connected (it changes the drum map for all drum tracks).
2. Open the drum grid on T5 (Select), add kick and hat hits.

**Expected:** the drum grid works on T5; the right instrument plays the hits.
M > DRUM MAP says **PLAYS ON T5, MIDI CHANNEL 5**.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 9. Saves and songs keep T5-T8
1. M > SONG SAVE, **X** on an empty song. Quit (Select + Start) and start
   GENVOLVE again.
2. Load the song.

**Expected:** after the restart T5-T8 are as you left them; the song page
lists **T1-T4 and T5-T8** in two columns, and loading it brings T5-T8 back.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 10. Modulation on T5-T8 (only if you use it)
1. M > MODULATION, **R1** (CC DEST), **SEND TO**: left/right.

**Expected:** the choices are **T1 ... T8**, then **T1-T4**, **T5-T8**,
**T1-T8**, then **CH 1-16**. Any destination you had set before the update
still points where it did.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

Write the results into NOTES.md, *Results so far* (row *Tracks 5-8 (D10)*),
or just send them in the thread.
