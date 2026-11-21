# Genvolve test sheet: M > LIVE + RECORD (live playing and recording)

Install the build linked in the PR (see NOTES.md, *Test checklist > How to
install a new build*). Your saved pattern, slots, songs and DRUM MAP carry
over. Every new setting starts **off**, so Start and Stop behave as before
until you change one.

Everything is done on the handheld and the FM1. No SSH. Tick **PASS** or
**FAIL** and write what you saw.

**Before you start:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual. Have a pattern that plays on all four tracks,
with T4 a drum track (TRACK > TYPE > DRUMS) if you can.

---

## Recording helpers

### R1. Find the page
1. Press **M**, go down to **LIVE + RECORD** and press **A**.

**Expected:** the **RECORD** page: COUNT-IN OFF, CLICK OFF, SONG LENGTH OFF
(ENDLESS), STOP AT ONCE, SONG POSITION OFF, START FROM MARK OFF, SET MARK
NOW, PUT MARK BACK. The right side explains the selected row. **B** goes back
to the menu, **M** closes it.

[ ] PASS  [ ] FAIL   Note: ______________________________

### R2. Count-in with a click
1. Set **COUNT-IN** to **1 BAR** and **CLICK** to **ON**.
2. Press **Start**.

**Expected:** the header shows **IN 4**, **IN 3**, **IN 2**, **IN 1** (yellow,
COUNT-IN under it) while T4's closed hat clicks four times, the first one
louder. Then the pattern starts on the next beat, and the header reads
**PLAYING** with **BAR 1.1** under it. The FM1's tempo did not jump.

[ ] PASS  [ ] FAIL   Note: ______________________________

### R3. The FM1 starts with the pattern, not with the count-in
1. Stop. On the FM1, watch its play indicator or step LEDs.
2. Press **Start** again (count-in still 1 bar).

**Expected:** the FM1 stays stopped during the four clicks and starts on
bar 1 together with Genvolve. (If Felucca ignores MIDI Start/Stop, write
that down: then it simply plays what it is sent.)

[ ] PASS  [ ] FAIL   Note: ______________________________

### R4. Stop during the count-in
1. Press **Start**, then **Start** again during the clicks.

**Expected:** it stops at once, no stuck click note, header **STOPPED**.

[ ] PASS  [ ] FAIL   Note: ______________________________

### R5. Song length
1. Set **COUNT-IN** **OFF**. Set **SONG LENGTH** to **4 BARS** (right four times; hold **Y** for 16 bars a step).
2. Press **Start** and wait.

**Expected:** the header shows **BAR 1.1 / 4** counting up. At the end of
bar 4 it stops by itself, exactly on the bar line, with no stuck notes.
The row also shows the time at this BPM (4 bars at 120 = 0:08).

[ ] PASS  [ ] FAIL   Note: ______________________________

### R6. Stop at the bar end
1. Set **SONG LENGTH** back to **OFF** and **STOP** to **AT BAR END**.
2. Press **Start**, and in the middle of a bar press **Start** again.
3. Start again, press **Start** mid-bar, then **Start** once more straight away.

**Expected:** 2: the header reads **ENDING** and playback stops on the next
bar line. 3: the second press stops at once.

[ ] PASS  [ ] FAIL   Note: ______________________________

### R7. Same take every time (start mark)
1. On the GENERATE page, make sure T1 evolves (EVOLVE every 1 bar, AMOUNT 50 or more).
2. Back on **RECORD**: **A** on **SET MARK NOW** (MARK: SET on the right), and set **START FROM MARK** **ON**.
3. Press **Start**, listen for 8 bars, **Stop**. Press **Start** again and listen.

**Expected:** both takes sound the same: the pattern starts from the marked
version and evolves the same way each time. With **START FROM MARK OFF**,
Start carries on from wherever evolution got to. **PUT MARK BACK** puts
the marked patterns back at any time.

[ ] PASS  [ ] FAIL   Note: ______________________________

### R8. Recording into a DAW or the Blackbox (optional)
1. Set the DAW or Blackbox 2 to follow MIDI clock and Start/Stop, arm recording.
2. **SONG POSITION ON**, **COUNT-IN 1 BAR**, press **Start**.

**Expected:** recording starts on bar 1 together with Genvolve (not during
the count-in), and it stops when Genvolve stops. Write down if the device
got confused by SONG POSITION (then leave it OFF).

[ ] PASS  [ ] FAIL   Note: ______________________________
