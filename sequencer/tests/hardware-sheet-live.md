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
1. Press **M**, go down to **LIVE + RECORD**, press **A** (PERFORM opens), then **R1**.

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

---

## Performance page and scenes

### P1. The page
1. **M** > **LIVE + RECORD** > **A**.

**Expected:** PERFORM: rows T1-T8 (unused T5-T8 dim, EMPTY), ALL, SCENE,
SONG, SWITCH AT. While playing, a green square lights next to tracks that
are sounding. **R1** goes to RECORD, **R1** again back.

[ ] PASS  [ ] FAIL   Note: ______________________________

### P2. Mute, solo, all
1. Play. On T1 press **A** (mute), then **A** again. On T2 press **B** (solo), then **B** again.
2. On ALL press **A**, then **A** again. Solo two tracks, then **B** on ALL.

**Expected:** 1: T1 drops out and comes back; only T2 plays while soloed.
2: everything mutes, then everything plays; B on ALL clears both solos.
Same mutes show on the GRID's track boxes.

[ ] PASS  [ ] FAIL   Note: ______________________________

### P3. Density
1. Play. On a melodic track press **left** five times (DENSITY -50), listen; then **right** ten times (+50).
2. Do the same on the drum track.

**Expected:** -50: noticeably fewer notes, the first beat mostly kept. +50:
extra softer notes in the gaps, using the track's own notes. Drums: fewer
hits / soft extra hats and snares, the kick never gets extras. The GRID
pattern itself does not change. Back at 0 it plays exactly as written.

[ ] PASS  [ ] FAIL   Note: ______________________________

### P3b. Fills
1. Play. On the drum track's row **hold X** for a bar, then let go.
2. On a melodic track hold **X**; then on **ALL** hold **X**.

**Expected:** 1: hats on every 16th and a snare roll rising through the
second half of each bar while held; the kick stays as it was; letting go
puts the beat straight back. 2: the track plays every step, the last beat
of each bar an octave higher; ALL does it on every track at once. **FILL**
shows next to the track while held.

[ ] PASS  [ ] FAIL   Note: ______________________________

### P4. Evolve from the page
1. On a track with EVOLVE OFF, hold **Y** and press **right** twice (or push the stick up).

**Expected:** EVOLVE reads 20% (stick: 5% a push) and the pattern starts
changing every 4 bars (GENERATE shows EVERY 4 BARS).

[ ] PASS  [ ] FAIL   Note: ______________________________

### P5. Save a scene and switch at the bar
1. On **SCENE 1** press **X** (saves every track into slot 1).
2. Change a few steps on the GRID, or roll a track. Back on PERFORM, **SCENE 2**, **X**.
3. Play. Pick **SCENE 1**, press **A** in the middle of a bar.

**Expected:** the row and the header show **SCENE 1 IN 4 ... IN 1**; on the
next bar line every track switches to its scene-1 pattern together. **B**
before the bar line cancels.

[ ] PASS  [ ] FAIL   Note: ______________________________

### P6. Switch every 4 bars
1. **SWITCH AT** 4 BARS. Play, queue SCENE 2 in bar 2.

**Expected:** the countdown shows **IN 3 BARS**, then **IN 2 BARS**, then
beats, and the switch happens on the bar-5 line (counted from Start).

[ ] PASS  [ ] FAIL   Note: ______________________________

### P7. Queue a song
1. Save two different songs on M > SONG SAVE (say 1 and 2).
2. With song 1 playing, PERFORM > **SONG 2** > **A**.

**Expected:** **SONG 2 IN ...** counts down, then song 2 comes in on the
bar line without a stop, no stuck notes, the FM1 keeps its clock. The
header briefly says NOW PLAYING SONG 2. RECORD settings are unchanged.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

## Arranger

### A1. Build an arrangement
1. Make three scenes on PERFORM (SCENE 1, 2, 3: change the patterns between, X on each).
2. **R1** to ARRANGE. **A** on ARRANGE (it turns ON and adds section 1).
3. On section 1: **right** until 4 BARS. **A** (adds a copy). On section 2 hold **Y** + **right** (SCENE 2), **X** (DRUM FILL).
4. **+ ADD SECTION**, **A** (section 3, SCENE 3). **AT THE END**: STOP.

**Expected:** the side timeline shows three coloured blocks sized by bars,
with a white edge on the section that has a fill; the total bars and time
are right (12 bars at 120 = 0:24).

[ ] PASS  [ ] FAIL   Note: ______________________________

### A2. Play it
1. Press **Start** and listen to the whole thing.

**Expected:** scene 1 for 4 bars, scene 2 for 4 bars with a drum fill in its
4th bar, scene 3 for 4 bars, then it stops by itself on the bar line. The
white playhead moves along the timeline, NOW: SECTION n, BAR n OF n.

[ ] PASS  [ ] FAIL   Note: ______________________________

### A3. Loop, remove, and saved with the song
1. AT THE END: LOOP. Start: after section 3 it goes back to section 1.
2. On section 2 press **Select**: it is removed.
3. Save the song (M > SONG SAVE, X), load another song, then load this one again.

**Expected:** 1 loops seamlessly. 2 leaves sections 1 and 3. 3: the
arrangement comes back with the song.

[ ] PASS  [ ] FAIL   Note: ______________________________

### A4. Record a whole arrangement
1. RECORD: COUNT-IN 1 BAR, ARRANGE AT THE END: STOP. DAW / Blackbox armed and following clock.
2. Press **Start**.

**Expected:** one take from bar 1 to the last section's last bar, with MIDI
Start after the count-in and Stop at the end.

[ ] PASS  [ ] FAIL   Note: ______________________________
