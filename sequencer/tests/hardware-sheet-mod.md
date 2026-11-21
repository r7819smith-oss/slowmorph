# Test sheet: M > MODULATION (LFOs, CC destinations, MIDI IN, learn) (10 tests)

Install the build linked in the PR (see NOTES.md, *Test checklist > How to
install a new build*). Your saved pattern, slots, songs and DRUM MAP carry
over; modulation starts with every destination **off**, so nothing changes
until you switch one on.

Everything is done on the handheld and the FM1. No SSH. Tick **PASS** or
**FAIL** and write what you saw. Tests 6 to 9 check things that come from
Felucca's source code only and have **never been tried**. A FAIL there is
useful: it tells us what the firmware really does.

**Before you start:** FM1 on Felucca, **MIDI IN: CH1-4**, clock source
**USB**, connected as usual (USB-C through the splitter). M > DEVICE + CLOCK
shows **FELUCCA**. Have a pattern playing on T1 with a sound whose filter
you can hear (not a drum track).

---

### 1. Find the pages
1. Press **M**. A new row **MODULATION** reads **0 CCS ON**. Press **A**.
2. Press **R1** twice, then **R1** again.

**Expected:** 1 opens **LFO** (LFO 1, SINE, SYNC TO BPM, 8 BARS) with a
moving dot on the wave and four live bars. 2: **CC DEST**, then **MIDI IN**,
then back to **LFO**. **B** goes back to the menu, **M** closes it.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 2. A slow filter sweep that never fully closes
1. On **CC DEST**, destination 1 is already set: **T1 (CH 1)**, **CC 74 CUTOFF**, **LOW 40**, **HIGH 90**.
2. On the **DEST** row press **A** (it reads **1 ON**). Press **Start** to play.
3. Listen for 2 or 3 bars, then go to **LFO**, **CYCLE** and set it to **1 BAR**.

**Expected:** T1's filter on the FM1 opens and closes slowly (8 bars), never
fully shut and never fully open. The bar on the right shows the range and
**NOW nn** moving between 40 and 90. At 1 BAR it moves faster and still in time.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 3. Range: how far it moves
1. With destination 1 still on, set **HIGH** to **60** (hold **Y** for steps of 10).
2. Go to **MOVE RANGE** and press **right** a few times.
3. Set **LOW** and **HIGH** both to **70**.

**Expected:** 1: a smaller sweep. 2: the same width of sweep, just brighter
overall (MID goes up, LOW and HIGH move together). 3: the filter sits still
at one setting.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 4. Wandering pan and pause
1. Destination **2** (press **R2** on CC DEST, or right on the DEST row): **T2**, **CC 10 PAN**, LFO 2 (**SMOOTH RAND**, free, **30 S**). Turn it **ON**.
2. On **LFO**, pick **LFO 2** (**R2**), set **CYCLE** to **3 S** so you hear it, then set **STATE** to **PAUSED**, then back to **RUNNING**.
3. On **LFO**, set **ALL MOD** to **OFF**, then **ON**.

**Expected:** 1-2: T2 drifts left and right without a pattern; PAUSED stops it
where it is; RUNNING carries on. 3: OFF stops every sweep (the sounds stay
where they were); ON starts them again.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 5. Start, stop and tempo
1. With destination 1 on (LFO 1, SYNC, 1 BAR), press **Start** to stop. Watch the dot.
2. On **LFO**, set **WHEN STOPPED** to **PAUSES**. Stop and start again.
3. While playing, change the BPM (Select + stick up/down).

**Expected:** 1: by default the LFO keeps moving while stopped (like the clock).
2: it now pauses while stopped, and on Start it begins again from the start
of its cycle. 3: a synced LFO speeds up and slows down with the tempo; a
FREE one (LFO 2) does not.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 6. Does the FM1 send anything back? (unverified)
1. Go to **MIDI IN** (it starts listening as it opens).
2. Play a few FM1 keys. Then press the FM1's **MENU**, set **SELECT** to
   **MODW**, leave the menu and turn the **SELECT** encoder.
3. Turn a few sound knobs on the FM1.

**Expected (from Felucca's source):** the top line names the FM1's input
port in green. 2: **NOTE ON / NOTE OFF** lines for the keys, **PRESSURE**
if you press harder, and **CC 1 = nn** for the SELECT encoder. 3: **nothing**
(Felucca does not send its knobs). Write down what you see in each case. If
the top line is yellow, copy its text here.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 7. Learn an incoming CC (needs test 6 to show CC 1)
1. On **CC DEST**, pick destination **3**, go to **LEARN CC** and press **A** (it reads **LISTENING..**).
2. Turn the FM1's **SELECT** encoder (still in MODW mode).

**Expected:** the message line says **DEST 3 LEARNED CC 1 ON CH n**, the CC row
reads **1 MOD WHEEL** and SEND TO is the track with that channel. A key press
alone is not learned (it says it heard a note). **B** cancels a learn.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 8. Teach Felucca a CC with SEND ONCE (unverified)
1. On **CC DEST**, pick destination **4**, set **CC** to **20**, **SEND TO T1**, leave it **OFF**.
2. On **LFO**, set **ALL MOD** to **OFF** (otherwise Felucca learns whatever CC comes first).
3. On the FM1, hold **GLO** and press **D4** (MIDI LEARN). Turn the FM1 knob you want (for example a delay or an engine knob on track 1).
4. On the handheld, **CC DEST > SEND ONCE**, press **A**. Then **GLO + D4** on the FM1 again to finish.
5. Turn destination 4 **ON** and **ALL MOD** back **ON**.

**Expected:** 3-4: the FM1's footer shows the parameter picked and then CC 20
learned. 5: that knob's parameter now moves with LFO 4.

[ ] PASS  [ ] FAIL   Note: ______________________________

### 9. All four tracks at once
1. Destination **5**: **SEND TO T1-T4**, **CC 91 REVERB**, LOW 20, HIGH 80, ON.

**Expected:** reverb moves on all four FM1 tracks together. The CC DEST page
lists the channels (CC 91 ON CH 1,2,3,4).

[ ] PASS  [ ] FAIL   Note: ______________________________

### 10. Saved, and in songs
1. Quit (M > SAVE AND QUIT) and start again.
2. Save a song (M > SONG SAVE, X), change a destination, then load the song again.

**Expected:** 1: every LFO and destination setting is back. 2: loading the song
brings back its modulation settings too. Notes and timing are no different
with modulation on (listen for timing wobble while several destinations run).

[ ] PASS  [ ] FAIL   Note: ______________________________

---

Afterwards, send `seq4h.log` if anything failed: it has a line each time the
MIDI input opens or closes (`MIDI input open: ...`) and each learned CC.
