# seq4h test sheet: build #133 (20 tests)

Build #133 is `main` after PR 25 (2026-11-21):
<https://github.com/r7819smith-oss/slowmorph/actions/runs/44852598386>,
artifact **seq4h-knulli-aarch64**. Install it as in NOTES.md, *Test
checklist > How to install a new build*.

Everything here is done on the handheld and the FM1. No SSH. Tick
**PASS** or **FAIL** and write what you saw in the note line. Tests 1-7 are
the new PR 25 features, which have not been tried on hardware yet, so do
those first. Tests 8-20 recheck everything else.

**Before you start**

- FM1 charged, its own pattern cleared (**REC > CLEAR**), **MENU > MIDI >
  MIDI IN** = **CH1-4**, CLOCK page source = **USB**.
- FM1 connected to the handheld with the USB-C to USB-C cable.
- Pick one FM1 track for drums and set it to Felucca's **DRUM** engine.
  FM1 track n listens on MIDI channel n. The sheet calls this number **c**.
- Start **SEQ4H** from Ports.

---

## New in PR 25 (do these first)

### 1. Drum track and DRUM MAP setup
1. **TRACK** page (L1/R1): pick a seq4h track (L2/R2). Set **TYPE** to
   **DRUMS (8 LANES)** and **MIDI CHANNEL** to **c**.
2. Press **M**, go down to **DRUM MAP**, press **A**.

**Expected:** the right side reads "PLAYS ON Tn, MIDI CHANNEL c" and "FM1
TRACK c MUST USE THE DRUM ENGINE". The last row, **PRESET**, reads
**FELUCCA (IN USE)**, also if you kept a save from build #127.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 2. Each drum lane plays the right sound
1. Still on DRUM MAP, press **A** on each lane in turn.

**Expected:** top to bottom you hear KICK 36, SNARE 38, HAT CL 42, HAT OP
46, CLAP 39, RIM 37, TOM 45, BELL 56, each matching its name.

[ ] PASS  [ ] FAIL   Lanes that sound wrong, and what you hear: ________

### 3. Choosing other drum sounds and presets
1. On a lane, hold **Y** and press **right** a few times.
2. Let go of Y and press **left/right** once.
3. Press **X**, rename the lane, press **A** to keep it.
4. Go to **PRESET**, choose **GM**, press **A** twice. Then choose
   **FELUCCA** and press **A** twice again.

**Expected:** 1: each press jumps to the next Felucca sound and plays it.
2: the note moves one semitone, same sound pitched (shown as +2, -4 ...).
3: the new name shows. 4: with GM the last lane becomes CRASH 49 (a
cymbal); with FELUCCA it is BELL 56 again and the row reads FELUCCA (IN
USE).

[ ] PASS  [ ] FAIL   Note: ______________________________

### 4. Drum grid, DRUM KIT roll and playing
1. **B** to the grid, cursor on the drum track, press **Select** on its own.
2. Go to **GENERATE**, press **A** on the TRACK row to roll a kit. Back to
   the drum grid (B, then Select).
3. Press **Start**.
4. In the drum grid: **A** adds and removes a hit, hold **X** +
   **up/down** changes its loudness, **Y** locks a lane, then roll again.

**Expected:** the drum grid opens on Select. Kick, hats and the rest play
together on FM1 track c. Edits are heard on the next pass. The locked lane
doesn't change when you roll again.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 5. Piano roll: length of new notes
1. On the grid, put the cursor on a **note** track, press **Select** on its
   own. The piano roll opens.
2. On an empty spot, hold **Y** and press **right** until the yellow
   **NEW NOTES: LEN** reads **2.00**.
3. Press **A** on three empty spots. Press Start to listen.
4. Press **Select** to go back to the grid.

**Expected:** each new note is drawn 2 steps long and plays 2 steps long.
Select goes back to the grid, and the bars show there too.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 6. Moving between steps with the analog stick
1. On the grid press **X** to open the STEP page. Go down to **NOTE**.
2. Push the **analog stick** left and right.
3. Also try it on the grid, the piano roll and the drum grid.

**Expected:** the step number goes down and up (wrapping at the track
end), and the NOTE row stays picked. On the other pages the cursor moves
one step.

If the stick does nothing: M > **BUTTON CODES**, A, move the stick, and
write what the top right shows: ______________________

[ ] PASS  [ ] FAIL

### 7. Moving between steps with X + left/right, R1 from the roll
1. On the STEP page, hold **X** and press **left/right** on the d-pad, on
   any row.
2. Open the piano roll (test 5), press **R1**, then **B**.
3. Press **Select + Start** together.

**Expected:** 1: the step changes like the stick did. 2: R1 opens STEP for
the step under the cursor and B returns to the piano roll, not the grid.
3: seq4h quits (start it again for the next tests).

[ ] PASS  [ ] FAIL   Note: ______________________________

---

## Everything else

### 8. MIDI status, clock, tempo and battery
1. Look at the header. Before pressing Start, look at the FM1's BPM.
2. Press **Start**, then **Start** again.
3. **TRACK** page: change **BPM** (hold Y for steps of 10), including one
   go at 200 or more.
4. Compare the header **BAT nn%** with Knulli's battery reading.

**Expected:** **MIDI: OK** in green with the FM1's name. The FM1 shows
seq4h's BPM before Start. All four tracks play on FM1 tracks 1-4; Stop
leaves no note hanging. The FM1 follows each BPM change within a second;
200+ stays even. BAT matches Knulli's number.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 9. Unplug and replug (stuck notes)
1. While playing, unplug the FM1. Wait 5 seconds. Plug it back in.

**Expected:** header shows **MIDI: WAITING** in yellow, then **MIDI: OK**
within about 2 seconds of plugging in. Any note left hanging stops when
you plug back in, and playing carries on in time (no jump back to step 1).

[ ] PASS  [ ] FAIL   Note: ______________________________

### 10. Buttons, help and the main menu
1. M > **BUTTON CODES** > A. Press every button and compare with the help
   at the bottom of each page. Turn it off again.
2. **L1/R1** change page, **L2/R2** change track, **B** goes back to the
   grid.
3. A short press of **M** opens the main menu; **M** or **B** closes it.

**Expected:** each button does what the help says. No Knulli hotkey fires
on a short M.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 11. M > MIDI OUTPUT
1. Press **M**, go to **MIDI OUTPUT**, press **A**.
2. Pick the FM1's port with **A**. Quit and start SEQ4H again, open the
   page again.
3. Pick **AUTO (first found)** with **A**.

**Expected:** the list shows AUTO and the FM1's port, with **\*** on the
one in use. The right side lists the FM1 as a USB MIDI device and the hint
says OK. After the restart the chosen port is still marked. AUTO still
plays to the FM1.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 12. Styles: AMBIENT, TECHNO, MELODIC, DRONE
1. M > **MUSIC STYLE**: pick each in turn, then **NEW FROM STYLE**, then
   play for a few bars.

**Expected:** each gives a new set in its own feel: AMBIENT the set you
like, TECHNO 128 BPM kick and offbeat hats, MELODIC 100 BPM chord bass and
arp, DRONE 66 BPM slow drones, drift and bells. No hung notes when
switching. (Do your drum track last, or redo test 1 afterwards: NEW FROM
STYLE keeps it as a drum track and rolls a kit.)

[ ] PASS  [ ] FAIL   Note: ______________________________

### 13. BLANK style
1. Note the BPM and SCALE. M > **MUSIC STYLE** > **BLANK**, then **NEW FROM
   STYLE**.

**Expected:** all four tracks are empty and MANUAL, 16 steps, nothing
plays. BPM, scale, root and MIDI channels are unchanged. M > SLOTS still
shows your saved slots.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 14. Generators, roll and undo
1. Pick AMBIENT again (NEW FROM STYLE). On **GENERATE**, change
   **GENERATOR** on one track (try TURING, BELL, MANUAL) and press **A** to
   roll.
2. Press **X**.
3. On the grid, put a note on a MANUAL track with **A** and edit it with
   **X** (STEP page).

**Expected:** each roll writes a new pattern on that track only, and locked
steps (Y on the grid) stay. X undoes the last roll ("UNDONE"). A MANUAL
track only plays what you put in.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 15. Evolve
1. On **GENERATE** set **EVOLVE** to **EVERY 2 BARS**, **AMOUNT** 30% or
   more, and **EVO PITCH** and **EVO RHYTHM** ON. Play for 16 bars.
2. Set **EVOLVE** back to OFF.

**Expected:** the track changes a little at bar lines, never mid-bar, and
locked steps never change. With EVOLVE OFF it repeats exactly.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 16. Mute and solo from the grid
1. On the grid press **left** from step 1 to reach the track label.
2. **A** mutes, **A** again unmutes. **B** solos; solo a second track too;
   **B** again un-solos.
3. **Right** goes back to step 1.

**Expected:** the label shows **M** (track goes silent, cut at once) and
**S** (only soloed tracks play). The TRACK page MUTE row agrees.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 17. Hold bars and offset
1. Cursor on a note, **X**, set **HOLD** to +2, **B**. Add a note inside
   the bar.
2. On STEP set **OFFSET** to LATE 25% on one note and EARLY 25% on another.
   Play.

**Expected:** a bar runs across the next two steps and stops at the new
note. The late block moves right and plays just after the beat, the early
one moves left and plays just before. The FM1 tempo doesn't change.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 18. Shuffle, NEVER ON GRID, SPREAD, ON GRID
1. On a note track with notes on every step: **TRACK** page **SHUFFLE**
   UP TO 25%, **NEVER ON GRID** ON. Play.
2. **GENERATE** > **TRANSFORM** **SPREAD**, **A**. Then **TRANSFORM ON
   GRID**, **A**.

**Expected:** 1: loose, never quite repeating, never on the beat. 2:
SPREAD moves the blocks to different places on the grid; ON GRID puts them
back in line. (On a drum track SPREAD says NOT FOR DRUM TRACKS: that is
right.)

[ ] PASS  [ ] FAIL   Note: ______________________________

### 19. Slots: save, name, load, copy, clear, chain
1. M > **SLOTS**. On an empty slot press **X** (save). **Y** rename it,
   **A** keep.
2. Change the pattern (roll on GENERATE), then on SLOTS press **A** on your
   slot.
3. **Select** > **COPY TO** another slot (A twice if it is full). Then
   **Select** > **CLEAR SLOT** on the copy (A twice).
4. On **GENERATE** set **CHAIN** on with two saved slots and play.

**Expected:** the name shows on SLOTS and on the GENERATE SLOT row. Loading
brings the saved pattern back (X on GENERATE undoes it). The copy keeps the
name; clear empties it and resets the name. The chain moves between slots
at bar lines.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 20. Saving: restart, autosave and power off
1. Quit with **Select + Start**. Start SEQ4H again.
2. Make one edit, wait **5 seconds**, then switch the handheld **off**
   without quitting. Switch on, start SEQ4H.

**Expected:** after both restarts the pattern, slots and slot names, holds,
offsets, shuffle, the drum track with its hits and lane names, the DRUM MAP
and the chosen MIDI port are all still there, including the last edit.

[ ] PASS  [ ] FAIL   Note: ______________________________

---

**If anything fails:** write down what you pressed and what happened, quit,
and send `roms/ports/seq4h/seq4h.log` (open it on the Mac from the SD card
or SHARE).
