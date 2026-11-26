# Genvolve: one test sheet for everything since build #133

94 tests in 12 parts. Everything built after build #133 (PR 29 to PR 43) that has not been tested on the hardware yet, with the repeats taken out. All on the handheld and the FM1, no SSH.

**Build:** run 240 (PR 43, same code as main 0d12ced): https://github.com/r7819smith-oss/slowmorph/actions/runs/44986135069 (artifact **genvolve-knulli-aarch64**).

**Before you start:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source **USB**, connected as usual (USB-C through the splitter). FM1 track 4 on **DRUM** and Genvolve T4 a drum track if you can. Optional tests need the Blackbox 2, Digitone or a DAW: tap SKIP if you don't have them to hand.

The phone version (tap PASS / FAIL / SKIP, type a comment, copy the results) is the artifact page linked in the project thread.

| Part | What | Tests |
|---|---|---|
| 1 | Install and first start | 4 |
| 2 | Styles track by track | 7 |
| 3 | Track box, locks and shortcuts | 7 |
| 4 | Chords (polyphony) | 5 |
| 5 | Song save | 3 |
| 6 | Clock and Start/Stop | 3 |
| 7 | Modulation (LFOs) | 9 |
| 8 | Tracks 5 to 8 | 8 |
| 9 | Rhythm: step rate, Euclid, ratchets, harmony | 14 |
| 10 | Live: RECORD, PERFORM, ARRANGE | 20 |
| 11 | Navigation, HELP and undo | 11 |
| 12 | Everything is kept | 3 |

---

## 1. Install and first start

Once, on the Mac with the SD card (or SHARE).

#### 1.1. Install the build
1. Open the build link above, download **genvolve-knulli-aarch64**, unzip it twice.
2. Copy **GENVOLVE.sh**, **SHOW-BUTTONS.sh**, **USB-MIDI-CHECK.sh** and the **genvolve** folder into **roms/ports/**. Choose **Replace** for everything.
3. Delete **roms/ports/SEQ4H.sh**. Keep the old **seq4h** folder for now.
4. On the handheld: Start > **Game settings > Update game lists**.

**Expected:** Ports shows **GENVOLVE**, **SHOW-BUTTONS** and **USB-MIDI-CHECK**, and no SEQ4H.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 1.2. Your old work is there
1. Start **GENVOLVE**.

**Expected:** Header top left reads **GENVOLVE**. Your last pattern plays (same BPM, slots, mutes). Top right of the GRID shows **T1-4** yellow and **/ T5-8** dim.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 1.3. Settings carried over
1. **M > MIDI OUTPUT** and **M > DEVICE + CLOCK**.

**Expected:** Same port picked as before; device and clock as you left them.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 1.4. SHOW-BUTTONS
1. Quit (**Select + Start**). Start **SHOW-BUTTONS** from Ports.

**Expected:** Genvolve starts with the button codes showing.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 2. Styles track by track

Uses M > MUSIC STYLE and the T rows under it.

#### 2.1. The T rows
1. Press **M**. On **MUSIC STYLE** pick **HIP HOP**.
2. On **T1** press **right** a few times, then hold **Y** + **right**.

**Expected:** All note-track rows follow HIP HOP (drum track shows **HIP HOP KIT**). Right steps through parts; Y + right jumps a whole style. Empty T5-T8 read **KEEP AS IT IS**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 2.2. Replace just one track
1. Note the BPM. Set **T1-T3** to **KEEP AS IT IS**, **T4** to **HIP HOP DRUMS / KIT**.
2. **NEW FROM STYLE**, **A**, **A**.

**Expected:** Message **NEW: T4. KEPT: T1 T2 T3**. T1-T3 unchanged, T4 a boom bap beat. BPM, swing and scale did not change.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 2.3. Mix styles
1. Rows: T1 **DRONE BASS**, T2 **AMBIENT ARP**, T3 **AMBIENT MOTIF**, T4 **TECHNO** or **DRUM N BASS** drums.
2. **NEW FROM STYLE**, A, A, **Start**.

**Expected:** Every track new in its own style: drone, arpeggio, motif, drums. Nothing hangs on stop.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 2.4. The four new styles
1. For **HIP HOP**, **DRUM N BASS**, **HOUSE**, **TRIP HOP**: pick it on MUSIC STYLE, NEW FROM STYLE, A, A, play a few bars.

**Expected:** HIP HOP 88 swung, DNB 172, HOUSE 124 four on the floor, TRIP HOP 80 half-time. Write which you liked.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 2.5. Kits follow the style
1. Give the drum track **HOUSE KIT** (other rows KEEP), NEW FROM STYLE.
2. **GENERATE**, press **A** a few times.

**Expected:** Every roll stays a house beat, not ambient.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 2.6. AMBIENT is sparse again
1. MUSIC STYLE **AMBIENT**, NEW FROM STYLE, A, A. Play a few minutes.

**Expected:** Slow and sparse like the first ambient set; drums only about two soft hits a bar.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 2.7. Locks in the piano roll
1. On a note track press **Select** (piano roll). **Y** on a note, **Y** on an empty step.
2. Roll the track on GENERATE, come back.
3. Hold **Y** + **right** on a note. Then **Y** on a locked step.

**Expected:** **STEP n LOCKED**, yellow corner/frame. Locked steps survive the roll (the empty one stays empty). Y + direction changes length, no lock. **STEP n UNLOCKED**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 3. Track box, locks and shortcuts

The track box is left of step 1 on the GRID.

#### 3.1. Roll and undo from the track box
1. Cursor on **T2**'s box. Tap **X**, tap **X** again.
2. Hold **X** for half a second.

**Expected:** Header **T2 ROLLED** each tap; holding X goes back one. (A MANUAL track says NO GENERATOR.)

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 3.2. Lock a track
1. On T2's box press **Y**, then tap **X**.
2. Play a few bars with evolve on. Then **M**, NEW FROM STYLE, A, A.
3. **Y** on T2's box again.

**Expected:** Orange **L**, **T2 IS LOCKED**, T2 never evolves. M row reads **LOCKED: KEPT** and NEW FROM STYLE leaves it. Y unlocks.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 3.3. ALL STEPS page
1. On T1's box press **Select**. On **VELOCITY** press left a few times, then **A**.
2. Try **GATE** and **PROBABILITY**. **STEP LOCKS**: A, A.

**Expected:** All T1 notes get quieter; A sets every step to the average. STEP LOCKS locks all steps, then clears them. **B** goes back.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 3.4. Lock a drum step
1. On the drum track, on the GRID press **Y** on a step with hits. Roll it (X on its box).

**Expected:** Yellow square on the step (yellow bar on the drum grid). That step's hits stay after the roll.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 3.5. Stick up/down
1. On the GRID push the **stick up / down** on a note.

**Expected:** Header shows STEP n VELOCITY going up/down. If nothing: M > BUTTON CODES, push the stick up and write the AXIS number.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 3.6. Stick press
1. Press the **stick in** on a track with a generator.

**Expected:** The current track rolls.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 3.7. Select combos
1. Hold **Select**: press **A**, then **X**, then **Y**. Still holding, stick up/down.
2. Let go. Then Select alone.

**Expected:** A rolls, X undoes, Y locks (L badge), stick changes BPM by 1. Letting go does **not** open the piano roll; Select alone does.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 4. Chords (polyphony)

On the FM1 first: track 2 > EDIT > **VOICE** page > **VCE: POLY**, a pad sound.

#### 4.1. A chord by hand
1. **TRACK** page, T2, last row **VOICES**: **POLY**.
2. Piano roll on T2 (Select): add two more notes above a note with **A**. **Start**.

**Expected:** The column has three notes and the FM1 plays them together.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 4.2. Chord from the STEP page
1. **X** on another T2 note, last row **CHORD**: right to **TRIAD**, then **SEVENTH**.

**Expected:** Right side lists the notes, they sound together, white dots on the grid step.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 4.3. Notes ring on
1. On a POLY step set **HOLD** to **+4**.

**Expected:** It keeps sounding while the next notes play.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 4.4. CHORDS generator
1. GENERATE, T2, GENERATOR: left from MANUAL to **CHORDS**, **A**. Try RHYTHM **PULSE** and **STABS**.

**Expected:** Held chords changing every bar; the piano roll shows them.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 4.5. Back to MONO, no hung notes
1. While chords ring: Stop; mute T2; unplug and replug the FM1.
2. VOICES back to **MONO**.

**Expected:** Sound stops every time (replug may leave one note until it's back). MONO plays one note per step.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 5. Song save

M > SONG SAVE.

#### 5.1. Save and rename
1. **M > SONG SAVE**, on **SONG 1** press **X**. Then **Y** to rename (up/down letter, A keeps).

**Expected:** **SAVED AS SONG 1**, row shows BPM; right side shows BPM, scale, style, tracks. New name shows.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 5.2. Load while playing
1. Change something big (NEW FROM STYLE or BPM). Play.
2. SONG SAVE, SONG 1, **A**, **A**.

**Expected:** Old session back (BPM, tracks, slots, drum map), FM1 stays in time, no hung note.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 5.3. Undo load, copy, clear
1. **Select > UNDO LOAD > A**. Select > **COPY TO** SONG 2 > A. Select > **CLEAR SONG** > A, A on song 2.

**Expected:** Undo brings back the session from before the load; copy and clear work.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 6. Clock and Start/Stop

M > DEVICE + CLOCK, with FELUCCA picked.

#### 6.1. Clock off
1. While playing set **MIDI CLOCK OFF**, change the BPM. Then back to **SEND**.

**Expected:** Notes still play but the FM1's tempo stops following; on SEND it follows again.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 6.2. Start/Stop off
1. **START/STOP OFF**, press Start. Then turn it **ON** while playing.

**Expected:** The FM1's own sequencer stays stopped; when turned on it starts at the next bar.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 6.3. Per-output settings (optional, Digitone)
1. Pick the Digitone on **M > MIDI OUTPUT**, then **DIGITONE** here. Play. Switch MIDI OUTPUT back to the FM1.

**Expected:** Digitone tracks 1-4 play and follow tempo; switching back says FELUCCA settings are back.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 7. Modulation (LFOs)

A pattern on T1 with a sound whose filter you can hear. Tests 7.6-7.8 have never been tried: a FAIL there is useful.

#### 7.1. Find the pages
1. **M > MODULATION**, then **R1**, **R1**, **R1**.

**Expected:** **LFO** page (moving dot), then **CC DEST**, **MIDI IN**, back to LFO.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.2. Filter sweep
1. CC DEST 1 (T1, CC 74 CUTOFF, 40-90): **A** on DEST (**1 ON**). Start.
2. LFO page: **CYCLE 1 BAR**.

**Expected:** T1's filter opens and closes slowly, never fully shut or open; **NOW nn** moves 40-90. 1 BAR is faster and in time.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.3. Range
1. **HIGH 60**. Then **MOVE RANGE** right. Then LOW and HIGH both **70**.

**Expected:** Smaller sweep; same width but brighter; then it sits still.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.4. Pan, pause, all off
1. DEST 2: T2, CC 10 PAN, ON. LFO 2 **CYCLE 3 S**, **STATE PAUSED**, then RUNNING.
2. **ALL MOD OFF**, then ON.

**Expected:** T2 drifts left/right; PAUSED holds it; ALL MOD OFF stops every sweep, ON restarts.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.5. Stop and tempo
1. Stop: watch the dot. **WHEN STOPPED: PAUSES**, stop and start. Change BPM while playing (Select + stick).

**Expected:** Keeps moving while stopped by default; PAUSES holds and restarts from the cycle start. Synced LFO follows BPM; free LFO 2 doesn't.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.6. What the FM1 sends back
1. **MIDI IN** page. Play FM1 keys. FM1 MENU: SELECT = **MODW**, turn SELECT. Turn some sound knobs.

**Expected:** Expected: green port name; NOTE ON/OFF for keys; **CC 1** for SELECT; nothing for knobs. Write what you saw.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.7. Learn a CC
1. CC DEST 3, **LEARN CC**, **A**, turn FM1 **SELECT** (MODW).

**Expected:** **DEST 3 LEARNED CC 1 ON CH n**, CC row **1 MOD WHEEL**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.8. Teach Felucca a CC (SEND ONCE)
1. DEST 4: CC 20, SEND TO T1, OFF. **ALL MOD OFF**.
2. FM1: **GLO + D4**, turn a knob. Handheld: **SEND ONCE**, A. FM1: GLO + D4 again.
3. DEST 4 ON, ALL MOD ON.

**Expected:** FM1 footer shows CC 20 learned; that knob now moves with LFO 4.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 7.9. All four tracks
1. DEST 5: **SEND TO T1-T4**, CC 91 REVERB, 20-80, ON.

**Expected:** Reverb moves on all four FM1 tracks; page reads CC 91 ON CH 1,2,3,4.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 8. Tracks 5 to 8

#### 8.1. Page 2 of the GRID
1. Cursor on T4, press **Down**, then **Up**. **R2** four times, **L2** four times.

**Expected:** **T5-T8** (pink, teal, lime, purple), empty, labels C5-C8, **T5-8** yellow top right. Up goes back. Up from T1 wraps to T8.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 8.2. Notes on T5
1. Press **A** on a few T5 steps. Go back up.

**Expected:** Pink steps; a **pink dot** after T5-8 on page 1. The FM1 plays nothing new.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 8.3. Pages, mute and solo on T5-T8
1. On T5 open STEP, TRACK, GENERATE. On T6's box **A** then **B**, undo both.

**Expected:** Pages work (TRACK shows channel 6 for T6). Solo T6 silences T1-T4 too.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 8.4. NEW FROM STYLE leaves empty T5-T8 alone
1. Clear T5. **M**: change MUSIC STYLE, NEW FROM STYLE, A, A.

**Expected:** T1-T4 get the new style; T5-T8 stay empty.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 8.5. Parts for T5-T8
1. **M**: T5 **TECHNO KICK**, T6 **TECHNO HATS**, T7 **AMBIENT MOTIF**, T8 **AMBIENT HIGH**, T1-T4 KEEP. NEW FROM STYLE, A, A.

**Expected:** **NEW: T5 T6 T7 T8. KEPT: T1 T2 T3 T4**; tempo and key stay.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 8.6. Second instrument on ch 5-8 (optional)
1. Blackbox 2 or Digitone on the same MIDI; its tracks on channels 5-8. Play.

**Expected:** FM1 plays T1-T4, the other plays T5-T8, both in time. Write which you used.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 8.7. Drum track on T5 (optional)
1. T5 TRACK **TYPE: DRUMS**. Drum grid (Select), add kick and hat.

**Expected:** Drum grid works on T5; M > DRUM MAP says **PLAYS ON T5, MIDI CHANNEL 5**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 8.8. Modulation targets
1. M > MODULATION > CC DEST, **SEND TO** left/right.

**Expected:** T1...T8, T1-T4, T5-T8, T1-T8, then CH 1-16. Old destinations unchanged.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 9. Rhythm: step rate, Euclid, ratchets, harmony

Notes on T1 and T2 (AMBIENT is fine).

#### 9.1. STEP RATE
1. **TRACK**, T1, **STEP RATE** (second-last row). Start. Right twice (**1/8**), three more (**1/2**), back to 1/16.

**Expected:** 1/8: half speed, notes twice as long, grid box shows 1/8. 1/2: half a bar a step. FM1 stays in time.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.2. Triplets
1. T2: STEP RATE **1/16T**, **LENGTH 12**. Listen 4 bars.

**Expected:** Three to the beat against four, round once a bar.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.3. REALIGN
1. T1 LENGTH 13, T2 LENGTH 11. **REALIGN ALL: EVERY 4 BARS**.
2. Back to OFF, let them drift, **A** on the row.

**Expected:** Every 4 bars all tracks jump to step 1 on the bar line, no doubled notes. A: **ALL TRACKS BACK TO STEP 1**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.4. Euclidean drum lane
1. Drum track, GENERATE: **EUCLID LANE KICK**, **HITS 5 OF 16**, **ROTATE** right twice. Roll with A.

**Expected:** Kick: 5 even hits, ROTATE shifts them; other lanes change on roll, kick keeps its pattern. X undoes.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.5. EUCLID transform
1. Note track with bunched notes: **TRANSFORM EUCLID, BY +0**, A. Then **BY +1**, A. Try it on drums.

**Expected:** Same notes spread evenly; +1 shifts one step; locked steps never move.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.6. Ratchet
1. **X** on a T1 note, **RATCHET x3**, Start. Try x2, x4, x8.

**Expected:** 3 quick repeats in the step, in time; grid block split in 3, info ends **X3**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.7. Conditions
1. Same note: **CONDITION 1:2**, then **2:2**, then FIRST, NOT FIRST (hold Y jumps 4).

**Expected:** 1:2 plays passes 1,3,5; 2:2 passes 2,4,6; FIRST only first pass. Small cyan square on the block.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.8. PRE / NOT PRE
1. Three T1 notes: **1:2**, **PRE**, **NOT PRE**.

**Expected:** Pass 1 plays the first two, pass 2 only the third, and so on.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.9. Drum hit ratchets
1. Drum track STEP page on a kick step: **KICK RATCHET x2**, **KICK COND 2:2**. A hat **x4**. Open the drum grid.

**Expected:** Kick roll of 2 every second pass, hat roll of 4; split lines and cyan square shown.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.10. RATCHET/VARY transforms, EVO
1. TRANSFORM **RATCHET BY +3**, A; **VARY BY +4**, A. **BY 0** clears. EVOLVE with only **EVO RATCHET+COND** on.

**Expected:** Notes get ratchets/conditions; BY 0 clears; evolve adds/removes them, notes stay; locks respected.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.11. Chord progression
1. GENERATE, up to **HARMONY: I-VI-IV-V**, **BARS EACH 2**. Listen 8 bars. Try 1 and 4.

**Expected:** All tracks move to the next chord every 2 bars; header e.g. **C MIN PENT  IV**. Grid notes don't change.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.12. FOLLOWS
1. **T1 FOLLOWS**: OFF, MOVE, CHORD TONES, ROOT (BASS). A melodic track on CHORD TONES.

**Expected:** OFF as written; MOVE shifts the line; ROOT plays the chord root; CHORD TONES keeps the shape on chord notes. Drums show - (DRUMS).

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.13. DRIFT
1. HARMONY **DRIFT**, BARS EACH 4.

**Expected:** Random nearby chord every 4 bars, never the same twice.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 9.14. Key change on the bar
1. **KEY CHANGE: MOVE ALL (BAR)**. Playing: **ROOT** right twice. Stopped: change ROOT again.

**Expected:** **C TO D AT BAR**, all note tracks jump together on the bar, drums stay. Stopped it moves at once.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 10. Live: RECORD, PERFORM, ARRANGE

A pattern on all tracks, T4 drums. M > LIVE + RECORD.

#### 10.1. RECORD page
1. **M > LIVE + RECORD > A** (PERFORM), then **R1**.

**Expected:** COUNT-IN OFF, CLICK OFF, SONG LENGTH OFF, STOP AT ONCE, SONG POSITION OFF, START FROM MARK OFF, SET MARK NOW, PUT MARK BACK.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.2. Count-in with click
1. **COUNT-IN 1 BAR**, **CLICK ON**, Start.

**Expected:** Header IN 4..IN 1 with 4 hat clicks (first louder), then **PLAYING BAR 1.1**. FM1 tempo doesn't jump.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.3. FM1 starts with the pattern
1. Stop, watch the FM1, Start again.

**Expected:** FM1 stays stopped during the clicks and starts on bar 1.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.4. Stop in the count-in
1. Start, then Start again during clicks.

**Expected:** Stops at once, no stuck click, **STOPPED**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.5. Song length
1. COUNT-IN OFF, **SONG LENGTH 4 BARS**, Start.

**Expected:** **BAR 1.1 / 4** counts up, stops on the bar-4 line by itself, no stuck notes.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.6. Stop at bar end
1. SONG LENGTH OFF, **STOP AT BAR END**. Start, press Start mid-bar. Again, then Start twice quickly.

**Expected:** **ENDING**, stops on the next bar line; a second press stops at once.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.7. Start mark
1. T1 evolving. **SET MARK NOW**, **START FROM MARK ON**. Play 8 bars, stop, play again.

**Expected:** Both takes sound the same. PUT MARK BACK restores the marked patterns.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.8. Recording into a DAW / Blackbox (optional)
1. Device follows clock and Start/Stop, armed. **SONG POSITION ON**, COUNT-IN 1 BAR, Start.

**Expected:** Recording starts on bar 1, not in the count-in, and stops with Genvolve.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.9. PERFORM page
1. **R1** back to PERFORM. Play.

**Expected:** Rows T1-T8 (empty dim), ALL, SCENE, SONG, SWITCH AT; green squares on sounding tracks.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.10. Mute, solo, all
1. T1 **A**, A. T2 **B**, B. ALL **A**, A. Solo two, **B** on ALL.

**Expected:** Mutes/solos work and show on the GRID; B on ALL clears solos.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.11. Density
1. Melodic track **left** 5 times (-50), then right 10 (+50). Same on drums.

**Expected:** Fewer notes / extra soft notes; drums thin or add hats/snares, kick never gets extras. 0 = as written.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.12. Fills
1. Hold **X** on the drum row for a bar. Then a melodic track, then ALL.

**Expected:** Hat 16ths + rising snare roll while held, beat straight back on release; melodic plays every step; ALL does all.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.13. Evolve from PERFORM
1. Track with EVOLVE OFF: hold **Y** + right twice (or stick up).

**Expected:** EVOLVE 20%, pattern changes every 4 bars.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.14. Scenes at the bar
1. **SCENE 1 X**. Change things. **SCENE 2 X**. Play, SCENE 1, **A** mid-bar.

**Expected:** Countdown **IN 4..IN 1**, all tracks switch on the bar line; B before it cancels.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.15. Switch every 4 bars
1. **SWITCH AT 4 BARS**, queue SCENE 2 in bar 2.

**Expected:** IN 3 BARS, IN 2 BARS, then beats; switches on the bar-5 line.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.16. Queue a song
1. Two songs saved. Playing song 1: PERFORM **SONG 2 > A**.

**Expected:** Song 2 comes in on the bar line without a stop, no stuck notes, FM1 keeps clock.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.17. Build an arrangement
1. Scenes 1-3 saved. **R1** to ARRANGE, **A** (ON + section 1). Section 1 **4 BARS**, A (copy). Section 2: Y + right (SCENE 2), **X** (fill). **+ ADD SECTION** (SCENE 3). AT THE END: **STOP**.

**Expected:** Three coloured blocks, white edge on the fill section, 12 bars = 0:24 at 120.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.18. Play it
1. Start, listen to the whole thing.

**Expected:** Scene 1, scene 2 with a fill in its 4th bar, scene 3, then stops on the bar line.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.19. Loop and remove
1. AT THE END: **LOOP**, Start. **Select** on section 2.

**Expected:** Loops seamlessly to section 1; section 2 is removed.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 10.20. Record an arrangement (optional)
1. COUNT-IN 1 BAR, AT THE END STOP, DAW armed. Start.

**Expected:** One take from bar 1 to the end, Start after the count-in, Stop at the end.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 11. Navigation, HELP and undo

Stop playback unless a test says otherwise.

#### 11.1. Tap M
1. On the GRID tap **M**, tap again.

**Expected:** Menu opens on release, closes on the second tap.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.2. Hold M: jump panel
1. Hold **M** about half a second.

**Expected:** **JUMP TO** with 18 tiles; GRID says YOU ARE HERE; track and UNDO/REDO counts at the top.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.3. Jump and let go
1. Holding M, d-pad to **SONGS**, let go.

**Expected:** SONG SAVE opens.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.4. Back to the last page
1. Go to GENERATE, B to the GRID. Hold M, let go.

**Expected:** Panel starts on **GENERATE (LAST PAGE)**; letting go goes there. Again: back to GRID.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.5. Cancel and track change
1. Hold M, move, **B**, let go. Hold M, **R2** twice, TRACK, **A**.

**Expected:** B closes with nothing else; second opens TRACK for **T3**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.6. Live and MIDI pages
1. Hold M > **PERFORM**; hold M > **RECORD**; hold M > **MODULATION**; tap M.

**Expected:** Each opens directly; tap of M closes back to the GRID.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.7. HELP
1. In the piano roll: hold M > **HELP**. Down a few, **B**. Also M > HELP from the menu.

**Expected:** **HELP: ROLL** lists its buttons then EVERYWHERE; B returns; menu HELP shows the page you came from.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.8. Undo and redo
1. Turn on 3 steps with **A**. **Select + X** three times, then **Select + B**. Select + X again ×2.

**Expected:** Each undo removes one, newest first (**UNDO STEP ...**); redo puts one back; then **NOTHING TO UNDO**.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.9. Held value is one undo
1. STEP page, hold **right** on VELOCITY. Select + X once.

**Expected:** Velocity goes back to where it started.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.10. HISTORY page
1. Roll T2, change a T1 step, REVERSE T3. Hold M > **HISTORY**. **A** on the 2nd line below NOW, then on the top FWD line.

**Expected:** **WENT BACK 2 EDITS**, then **WENT FORWARD 2 EDITS**. Track-box hold X and GENERATE X undo share this history.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 11.11. Song load clears history
1. Make an edit, load a song, Select + X.

**Expected:** **NOTHING TO UNDO**; SONG SAVE's UNDO LOAD still works.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________


---

## 12. Everything is kept

Do this last.

#### 12.1. Restart keeps edits
1. Save a slot (PATTERN SAVE). Quit (**Select + Start**), start GENVOLVE.

**Expected:** Same patterns, T5-T8, track lock, drum step lock, piano roll locks, chords and VOICES, ratchets and conditions; M rows show each track's style.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 12.2. Restart keeps settings
1. Look at TRACK, GENERATE, DEVICE + CLOCK, MODULATION.

**Expected:** STEP RATE, REALIGN, Euclid lanes, HARMONY, BARS EACH, FOLLOWS, clock and Start/Stop, all LFOs and destinations as you left them.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________

#### 12.3. Songs bring it all back
1. **M > SONG SAVE** lists your songs. Save one, change T5-T8, a mod destination and the arrangement, then load it again. Load the slot from 12.1.

**Expected:** The song brings back T5-T8, modulation and the arrangement; the slot keeps ratchets and conditions.

[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________
