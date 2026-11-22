# Genvolve test sheet: quick navigation, HELP and undo history

Everything is done on the handheld and the FM1. No SSH. Tick **PASS** or
**FAIL** and write what you saw.

**Before you start:** a pattern that plays on T1-T4, FM1 connected as usual.
Stop playback unless a test says otherwise.

What changed: **M** now acts when you let go of it. A quick tap opens and
closes the main menu as before. Holding M shows the **jump panel**. Undo now
goes back up to 32 edits, with redo, and the main menu has two new rows,
**UNDO HISTORY** and **HELP**.

---

### 1. A tap of M still opens the menu
1. On the GRID, tap **M** (press and let go straight away).
2. Tap **M** again.

**Expected:** the main menu opens on the first tap and closes on the second,
back on the GRID. It opens as you let go, not as you press.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 2. Hold M: the jump panel
1. On the GRID, hold **M** for about half a second and keep holding.

**Expected:** a panel titled **JUMP TO** covers the screen with 18 tiles in 4
columns: GRID, STEP, TRACK, GENERATE, PIANO ROLL (or DRUM GRID on a drum
track), ALL STEPS, MAIN MENU, PATTERNS, SONGS, DRUM MAP, MIDI OUT, DEVICE,
MODULATION, PERFORM, ARRANGE, RECORD, HISTORY, HELP. GRID says **YOU ARE
HERE**. The track (T1) shows at the top, with UNDO and REDO counts on the
right.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 3. Pick a page and let go
1. Still holding **M**, use the **d-pad** to move the yellow box to **SONGS**.
2. Let go of **M**.

**Expected:** SONG SAVE opens. B goes back to the main menu, M closes it
(back to the GRID).

[ ] PASS  [ ] FAIL   Note: ______________________________

### 4. Back to the last page in one move
1. Go to the **GENERATE** page (R1 three times from the GRID), then back to the GRID with **B**.
2. Hold **M** until the panel shows, then let go without touching anything else.

**Expected:** the panel starts on **GENERATE** (marked LAST PAGE), so letting
go takes you straight back to GENERATE. Do it again and you are back on the
GRID.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 5. Cancel, A and track change in the panel
1. Hold **M**, move to any tile, press **B**, then let go of M.
2. Hold **M**, press **R2** twice, move to **TRACK**, press **A** (still holding M), then let go.

**Expected:** step 1: the panel closes on B and nothing else happens (you stay
on the page, the menu does not open). Step 2: the track at the top of the
panel goes T1, T2, T3; A opens the TRACK page for **T3** straight away.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 6. Jump into the live and MIDI pages
1. Hold **M**, pick **PERFORM**, let go. Then hold M, pick **RECORD**, let go.
2. Hold **M**, pick **MODULATION**, let go. Tap **M**.

**Expected:** PERFORM, then RECORD open directly (L1/R1 still move between
PERFORM, ARRANGE and RECORD). MODULATION opens the LFO page. The tap of M on
any of these closes it, back to the GRID.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 7. HELP for the page you are on
1. Go to the **piano roll** (Select on the GRID on a note track).
2. Hold **M**, pick **HELP**, let go.
3. Press **down** a few times, then **B**.

**Expected:** **HELP: ROLL** lists the piano roll's buttons (white) and what
they do, then an **EVERYWHERE** section (L1/R1, L2/R2, M tap and hold, Select
+ X undo, Select + B redo ...). Down scrolls. B goes back to the piano roll.
Also try **M > HELP** from the main menu: it shows the help of the page you
opened the menu from.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 8. Undo several edits, then redo
1. On the GRID turn on 3 empty steps on T1 with **A** (three different steps).
2. Hold **Select** and press **X** three times.
3. Still holding Select, press **B** once.

**Expected:** each Select + X takes back one step, newest first; the header
says e.g. **UNDO STEP S5 T1 (2 MORE)**. After three, all three steps are off
again. Select + B (redo) puts the first one back (**REDO ...**). A fourth
Select + X with nothing left says **NOTHING TO UNDO**.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 9. Holding a value is one undo
1. On the **STEP** page, hold **right** on VELOCITY until it has gone up a lot.
2. Select + X once.

**Expected:** one undo puts the velocity back to where it started (repeats of
the same change within about 1.5 seconds are one entry).

[ ] PASS  [ ] FAIL   Note: ______________________________

### 10. UNDO HISTORY page
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

### 11. Old undo buttons still work
1. On the GRID's track box (left of step 1), tap **X** to roll the track, then
   hold **X** for half a second.
2. On GENERATE press **A** (roll), then **X**.

**Expected:** both undo the roll, as before. They now share the history with
Select + X.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 12. Loading a song clears the history
1. Make an edit, then load a song (M > SONG SAVE, A, A).
2. Select + X.

**Expected:** **NOTHING TO UNDO** (undo never steps into the other song).
SONG SAVE's own **UNDO LOAD** still brings the old session back.

[ ] PASS  [ ] FAIL   Note: ______________________________
