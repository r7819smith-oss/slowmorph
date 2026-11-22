/* M > LIVE pages (included by ui.c, which has the drawing helpers and the
 * Ui struct): RECORD. The engine side is live.c. */

/* ---- Start button and header ---------------------------------------- */

/* Start: play, or stop (now, or at the bar line with STOP AT BAR END). */
static void live_start_button(Seq *s, Ui *u) {
    if (!s->playing) {
        seq_start(s);
        if (s->live.count_left) say(u, "COUNT-IN: START AGAIN STOPS IT");
        return;
    }
    if (live_stop_request(s)) say(u, "STOPS AT THE BAR LINE (START AGAIN: NOW)");
}

/* The play state in the header: PLAYING, STOPPED, IN 7 (count-in beats to go)
 * or ENDING (stops at the bar line); and the bar position under it. */
static void draw_live_header(SDL_Renderer *r, const Seq *s) {
    char buf[48];
    int bars, beats, bar, beat;
    if (live_counting(s, &bars, &beats)) {
        snprintf(buf, sizeof buf, "IN %d", bars * 4 + beats);   /* beats to go */
        text(r, 440, 18, 2, YELLOW, buf);
        text(r, 440, 35, 1, YELLOW, "COUNT-IN");
        return;
    }
    text(r, 440, 18, 2, s->live.stop_pending ? YELLOW : s->playing ? (Col){90, 220, 120} : DIM,
         s->live.stop_pending ? "ENDING" : s->playing ? "PLAYING" : "STOPPED");
    live_position(s, &bar, &beat);
    if (!bar) return;
    int qb, qbeat;
    if (live_switch_in(s, &qb, &qbeat)) {   /* a scene or song waits: count it down here, on every page */
        int n = qb * 4 + qbeat;
        if (s->live.song_q >= 0) snprintf(buf, sizeof buf, "SONG %d IN %d%s", s->live.song_q + 1, n > 4 ? (n + 3) / 4 : n, n > 4 ? " BARS" : "");
        else snprintf(buf, sizeof buf, "SCENE %d IN %d%s", s->live.scene_q + 1, n > 4 ? (n + 3) / 4 : n, n > 4 ? " BARS" : "");
        text(r, 440, 35, 1, YELLOW, buf);
        return;
    }
    if (s->live.length) snprintf(buf, sizeof buf, "BAR %d.%d / %d", bar, beat, s->live.length);
    else snprintf(buf, sizeof buf, "BAR %d.%d", bar, beat);
    text(r, 440, 35, 1, s->live.length && bar >= s->live.length ? YELLOW : DIM, buf);
}

/* ---- RECORD page ------------------------------------------------------ */


enum { RR_COUNT, RR_CLICK, RR_LENGTH, RR_STOP, RR_SPP, RR_FROM_MARK, RR_MARK, RR_RESTORE, REC_ROWS };
static const int COUNT_BARS[4] = {0, 1, 2, 4};

/* "4:16" for n bars at the session's tempo. */
static void bars_time(const Seq *s, int n, char *out, int len) {
    long secs = (long)n * 4 * 60 / (s->bpm > 0 ? s->bpm : 120);
    snprintf(out, (size_t)len, "%d:%02d", (int)(secs / 60 % 1000), (int)(secs % 60));
}

static void rec_row_text(const Seq *s, int row, char *label, char *val) {
    static const char *L[REC_ROWS] = {"COUNT-IN", "CLICK", "SONG LENGTH", "STOP", "SONG POSITION",
                                      "START FROM MARK", "SET MARK NOW", "PUT MARK BACK"};
    const Live *l = &s->live;
    snprintf(label, 24, "%s", L[row]);
    switch (row) {
    case RR_COUNT: if (l->count_in) snprintf(val, 24, "%d BAR%s", l->count_in, l->count_in > 1 ? "S" : ""); else snprintf(val, 24, "OFF"); break;
    case RR_CLICK: snprintf(val, 24, "%s", l->click ? "ON" : "OFF"); break;
    case RR_LENGTH:
        if (l->length) { char tm[12]; bars_time(s, l->length, tm, sizeof tm); snprintf(val, 24, "%d BARS %.7s", l->length, tm); }
        else snprintf(val, 24, "OFF (ENDLESS)");
        break;
    case RR_STOP: snprintf(val, 24, "%s", l->stop_mode == STOP_AT_BAR ? "AT BAR END" : "AT ONCE"); break;
    case RR_SPP: snprintf(val, 24, "%s", l->spp ? "ON" : "OFF"); break;
    case RR_FROM_MARK: snprintf(val, 24, "%s", l->from_mark ? (l->has_mark ? "ON" : "ON (NO MARK)") : "OFF"); break;
    case RR_MARK: snprintf(val, 24, "%s", l->has_mark ? "A: MARK AGAIN" : "A: MARK"); break;
    default: snprintf(val, 24, "%s", l->has_mark ? "A: PUT BACK" : "NO MARK YET"); break;
    }
}

static void rec_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    Live *l = &s->live;
    if (row == RR_COUNT) {
        int k = 0;
        while (k < 3 && COUNT_BARS[k] != l->count_in) k++;
        l->count_in = (uint8_t)COUNT_BARS[clampi(k + d, 0, 3)];
    } else if (row == RR_CLICK) l->click ^= 1;
    else if (row == RR_LENGTH) {
        int n = l->length + d * (big ? 16 : 1);
        if (l->length == 0 && d > 0) n = big ? 16 : 1;
        l->length = (uint16_t)clampi(n, 0, LIVE_MAX_LENGTH);
    }
    else if (row == RR_STOP) l->stop_mode = l->stop_mode == STOP_AT_BAR ? STOP_NOW : STOP_AT_BAR;
    else if (row == RR_SPP) l->spp ^= 1;
    else if (row == RR_FROM_MARK) {
        l->from_mark ^= 1;
        if (l->from_mark && !l->has_mark) say(u, "SET A MARK FIRST (ROW BELOW)");
    }
}

static void rec_row_action(Seq *s, Ui *u, int row) {
    if (row == RR_MARK) {
        live_mark(s);
        say(u, "MARKED: EVERY TRACK'S PATTERN AS IT IS NOW");
    } else if (row == RR_RESTORE) {
        if (live_restore_mark(s) < 0) say(u, "NO MARK YET: A ON SET MARK NOW");
        else say(u, "PATTERNS PUT BACK TO THE MARK");
    } else rec_row_adjust(s, u, row, 1, 0);
}

static void rec_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    int *row = &u->row[P_RECORD];
    if (dy) *row = wrap(*row + dy, REC_ROWS);
    else if (dx) rec_row_adjust(s, u, *row, dx, u->held[B_Y]);
    else if (b == B_A) rec_row_action(s, u, *row);
    else if (b == B_B) u->page = P_MENU;
}

static void draw_rec_side(SDL_Renderer *r, const Seq *s, const Ui *u) {
    char buf[64];
    int bar, beat, bars, beats;
    text(r, 392, 62, 2, YELLOW, "RECORD");
    if (live_counting(s, &bars, &beats)) snprintf(buf, sizeof buf, "COUNT-IN: %d BEATS", bars * 4 + beats);
    else {
        live_position(s, &bar, &beat);
        if (!bar) snprintf(buf, sizeof buf, "STOPPED");
        else if (s->live.length) snprintf(buf, sizeof buf, "BAR %d.%d OF %d", bar, beat, s->live.length);
        else snprintf(buf, sizeof buf, "BAR %d.%d", bar, beat);
    }
    text(r, 392, 86, 2, s->playing ? WHITE : DIM, buf);
    static const char *HELP[REC_ROWS][5] = {
        {"BARS OF MIDI CLOCK BEFORE START.", "THE RECEIVER GETS MIDI START", "ONLY WHEN THE COUNT-IN ENDS, SO", "IT BEGINS ON BAR 1 WITH YOU.", ""},
        {"A SHORT NOTE ON EACH BEAT OF THE", "COUNT-IN, LOUDER ON BEAT 1: THE", "FIRST DRUM TRACK'S CLOSED HAT", "(NO DRUM TRACK: A HIGH C ON T1).", ""},
        {"PLAYBACK STOPS BY ITSELF AT THE", "END OF THIS BAR, COUNTED FROM", "START. TIME IS AT THIS BPM.", "HOLD Y: 16 BARS A STEP.", ""},
        {"AT BAR END: START WHILE PLAYING", "WAITS FOR THE BAR LINE, SO A TAKE", "ENDS CLEANLY (HEADER: ENDING).", "START AGAIN WHILE IT WAITS STOPS", "AT ONCE."},
        {"SENDS SONG POSITION 0 JUST BEFORE", "MIDI START, SO A DAW THAT WAS", "MOVED ALWAYS STARTS AT ITS BAR 1.", "OFF IF A DEVICE MISREADS IT.", ""},
        {"EVERY START PUTS THE MARK BACK", "FIRST: EVOLVING TRACKS PLAY THE", "SAME TAKE EACH TIME (THE SAME", "RANDOM CHOICES TOO).", ""},
        {"REMEMBERS EVERY TRACK'S PATTERN", "AND THE RANDOM STATE NOW.", "", "", ""},
        {"PUTS EVERY TRACK'S PATTERN BACK", "TO THE MARK, PLAYING OR NOT.", "", "", ""}};
    int row = u->row[P_RECORD];
    for (int k = 0; k < 5; k++) text(r, 392, 116 + 12 * k, 1, DIM, HELP[row][k]);
    text(r, 392, 190, 1, WHITE, "TO RECORD INTO A DAW OR BLACKBOX:");
    text(r, 392, 204, 1, DIM, "1 SET IT TO FOLLOW MIDI CLOCK AND");
    text(r, 392, 216, 1, DIM, "  START/STOP (DEVICE + CLOCK ON).");
    text(r, 392, 228, 1, DIM, "2 ARM RECORDING THERE.");
    text(r, 392, 240, 1, DIM, "3 PRESS START HERE: IT RECORDS");
    text(r, 392, 252, 1, DIM, "  FROM BAR 1 AFTER THE COUNT-IN.");
    text(r, 392, 264, 1, DIM, "4 SONG LENGTH OR STOP AT BAR ENDS");
    text(r, 392, 276, 1, DIM, "  THE TAKE ON A BAR LINE.");
    snprintf(buf, sizeof buf, "MARK: %s", s->live.has_mark ? "SET" : "NONE");
    text(r, 392, 300, 1, s->live.has_mark ? (Col){90, 220, 120} : DIM, buf);
    text(r, 20, H - 34, 1, DIM, "UP/DOWN ROW  LEFT/RIGHT CHANGE (HOLD Y = BIG)  A DO THE ROW");
    text(r, 20, H - 20, 1, DIM, "B BACK TO MENU  L1/R1 OTHER LIVE PAGES  M CLOSE MENU  START PLAY/STOP");
}

/* ---- PERFORM page ------------------------------------------------------
 * One row per track, then ALL TRACKS, SCENE, SONG and SWITCH AT.
 *   track rows: A mute, B solo, left/right DENSITY, hold Y + left/right (or
 *               the stick up/down) EVOLVE amount, hold X a FILL
 *   ALL TRACKS: the same on every track (A mutes all, or unmutes all if any is muted)
 *   SCENE: left/right picks 1-8, A switches to it at the next switch bar (now
 *          when stopped), X saves every track into it (X twice if it has
 *          patterns), B cancels a switch that waits
 *   SONG: left/right picks 1-8, A switches to it at the next switch bar, B cancels
 *   SWITCH AT: every 1, 2, 4 or 8 bars */
enum { PR_ALL = SEQ_TRACKS, PR_SCENE, PR_SONG, PR_SWITCH, PERF_ROWS };

static void perf_swap(void *seq, void *song) { song_apply(seq, song); free(song); }

static void perf_evolve(Seq *s, int t, int d) {
    EvolveSettings *e = &s->track[t].evo;
    e->amount = (uint8_t)clampi(e->amount + d, 0, 100);
    if (d > 0 && !e->every) e->every = 4;   /* evolution was off: every 4 bars */
}

static void perf_say_track(const Seq *s, Ui *u, int t) {
    const SeqTrack *tr = &s->track[t];
    snprintf(u->msg_buf, sizeof u->msg_buf, "T%d DENSITY %+d EVOLVE %d%%%s", t + 1, s->live.density[t],
             tr->evo.every ? tr->evo.amount : 0, tr->locked ? " (LOCKED)" : "");
    say(u, u->msg_buf);
}

/* Stick up/down on PERFORM: the EVOLVE amount of the row's track (or all). */
static void perf_stick(Seq *s, Ui *u, int d) {
    int row = u->row[P_PERFORM];
    if (row > PR_ALL) return;
    for (int t = 0; t < SEQ_TRACKS; t++) if (row == PR_ALL || row == t) perf_evolve(s, t, d * 5);
    if (row < PR_ALL) perf_say_track(s, u, row);
}

static void perf_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    int *row = &u->row[P_PERFORM], r = *row, confirm = u->perf_confirm;
    u->perf_confirm = 0;
    if (dy) { *row = wrap(r + dy, PERF_ROWS); return; }
    if (r <= PR_ALL) {
        int all = r == PR_ALL, any_muted = 0;
        for (int t = 0; t < SEQ_TRACKS; t++) any_muted |= s->track[t].mute;
        for (int t = 0; t < SEQ_TRACKS; t++) {
            if (!all && t != r) continue;
            if (b == B_A && all) { if (s->track[t].mute == any_muted) seq_track_mute_toggle(s, t); }
            else if (b == B_A) seq_track_mute_toggle(s, t);
            else if (b == B_B && all) { if (s->track[t].solo) seq_track_solo_toggle(s, t); }
            else if (b == B_B) seq_track_solo_toggle(s, t);
            else if (b == B_X) live_fill(s, t, 1);   /* held: let go and the track is back (perf_x_release) */
            else if (dx && u->held[B_Y]) perf_evolve(s, t, dx * 10);
            else if (dx) live_density(s, t, dx * 10);
        }
        if (b == B_X) say(u, all ? "FILL: ALL TRACKS (LET GO OF X TO END)" : "FILL (LET GO OF X TO END)");
        else if (all && b == B_A) say(u, any_muted ? "ALL TRACKS ON" : "ALL TRACKS MUTED");
        else if (all && b == B_B) say(u, "SOLOS CLEARED");
        else if (!all && dx) perf_say_track(s, u, r);
        if (dx && u->held[B_Y]) u->y_used = 1;
        return;
    }
    if (r == PR_SCENE) {
        if (dx) u->perf_scene = wrap(u->perf_scene + dx, LIVE_SCENES);
        else if (b == B_A) {
            int k = u->perf_scene;
            if (!live_scene_tracks(s, k)) { say(u, "SCENE IS EMPTY: X SAVES EVERY TRACK INTO IT"); return; }
            if (live_queue_scene(s, k)) snprintf(u->msg_buf, sizeof u->msg_buf, "SCENE %d WAITS FOR THE SWITCH BAR", k + 1);
            else snprintf(u->msg_buf, sizeof u->msg_buf, "SCENE %d: %d TRACKS LOADED", k + 1, live_scene_tracks(s, k));
            say(u, u->msg_buf);
        }
        else if (b == B_X) {
            int k = u->perf_scene;
            if (live_scene_tracks(s, k) && !confirm) { u->perf_confirm = 1; say(u, "X AGAIN: REPLACE THIS SCENE (SLOT ON EVERY TRACK)"); return; }
            live_scene_store(s, k);
            snprintf(u->msg_buf, sizeof u->msg_buf, "SCENE %d SAVED ON EVERY TRACK", k + 1);
            say(u, u->msg_buf);
        }
        else if (b == B_B) { live_queue_scene(s, -1); say(u, "SCENE SWITCH CANCELLED"); }
    } else if (r == PR_SONG) {
        if (dx) u->perf_song = wrap(u->perf_song + dx, SEQ_SONGS);
        else if (b == B_A) {
            if (!u->song_dir) { say(u, "SONGS ARE OFF IN THIS MODE"); return; }
            if (!u->song[u->perf_song].used) { say(u, "SONG IS EMPTY (SAVE IT ON M > SONG SAVE)"); return; }
            u->perf_song_due = 1;   /* the file is read after the press, unlocked */
        }
        else if (b == B_B) { live_queue_song(s, -1, NULL, NULL); say(u, "SONG SWITCH CANCELLED"); }
    } else if (r == PR_SWITCH && dx) {
        static const int Q[4] = {1, 2, 4, 8};
        int k = 0;
        while (k < 3 && Q[k] != s->live.quant) k++;
        live_set_quant(s, Q[clampi(k + dx, 0, 3)]);
    }
}

/* X let go: every fill ends. */
static void perf_x_release(Ui *u) {
    player_lock(u->p);
    u->p->seq.live.fill = 0;
    player_unlock(u->p);
}

/* Read the chosen song (player unlocked), then queue it. */
static void perf_song_do(Ui *u) {
    char path[512];
    int k = u->perf_song;
    u->perf_song_due = 0;
    Seq *song = malloc(sizeof *song);
    if (!song) { say(u, "OUT OF MEMORY"); return; }
    song_path(u->song_dir, k, path, sizeof path);
    if (song_read_file(path, song, NULL) != 0) { free(song); say(u, "CANNOT READ THIS SONG"); return; }
    player_lock(u->p);
    Seq *s = &u->p->seq;
    live_queue_song(s, k, song, perf_swap);
    int now = !s->playing || s->live.count_left;
    if (now) {   /* stopped: in straight away */
        live_start_song_now(s);
    }
    player_unlock(u->p);
    song_say(u, now ? "LOADED" : "WAITS FOR THE SWITCH BAR:", k);
}

/* The engine switched songs at a bar: keep the UI in step. */
static void live_ui_poll(Ui *u) {
    player_lock(u->p);
    int k = u->p->seq.live.song_now;
    u->p->seq.live.song_now = -1;
    player_unlock(u->p);
    if (k < 0) return;
    u->cur_song = k;
    u->undo_track = -1;   /* GENERATE's X must not undo into another song */
    u->on_label = u->drum_label = 0;
    song_say(u, "NOW PLAYING", k);
}

static void draw_bar(SDL_Renderer *r, int x, int y, int w, int v, int lo, int hi, Col c) {
    rect(r, x, y, w, 10, (Col){36, 38, 52});
    if (lo < 0) {   /* centred: -100..100 */
        int mid = x + w / 2, len = v * (w / 2) / hi;
        rect(r, mid - 1, y - 2, 2, 14, DIM);
        if (len > 0) rect(r, mid, y, len, 10, c); else if (len < 0) rect(r, mid + len, y, -len, 10, c);
    } else if (v > 0) rect(r, x, y, v * w / hi, 10, c);
}

/* "IN 3 BARS", then "IN 4" ... "IN 1" (beats) for the last bar. */
static void switch_text(char *buf, int n, const char *what, int bars, int beats) {
    int b = bars * 4 + beats;
    if (b > 4) snprintf(buf, (size_t)n, "NEXT: %s IN %d BARS", what, (b + 3) / 4);
    else snprintf(buf, (size_t)n, "NEXT: %s IN %d", what, b);
}

static void draw_perform(SDL_Renderer *r, const Seq *s, const Ui *u) {
    char buf[64];
    int pitch = SEQ_TRACKS > 4 ? 28 : 34, y0 = 82, sel = u->row[P_PERFORM];
    text(r, 20, 62, 1, DIM, "TRACK");
    text(r, 150, 62, 1, DIM, "MUTE SOLO");
    text(r, 290, 62, 1, DIM, "DENSITY (L/R)");
    text(r, 460, 62, 1, DIM, "EVOLVE (Y+L/R)");
    for (int row = 0; row < PERF_ROWS; row++) {
        int y = y0 + row * pitch + (row > PR_ALL ? 8 : 0);
        if (row == sel) rect(r, 14, y - 6, W - 28, pitch - 4, (Col){40, 44, 64});
        if (row <= PR_ALL) {
            int all = row == PR_ALL, muted = 0, solo = 0, dens = 0, evo = 0, n = 0, sounding = 0;
            for (int t = 0; t < SEQ_TRACKS; t++) {
                if (!all && t != row) continue;
                const SeqTrack *tr = &s->track[t];
                muted += tr->mute; solo += tr->solo; dens += s->live.density[t];
                evo += tr->evo.every ? tr->evo.amount : 0; n++;
                sounding |= tr->sounding >= 0 || tr->npv || tr->drum_down;
            }
            if (all) snprintf(buf, sizeof buf, "ALL");
            else snprintf(buf, sizeof buf, "T%d", row + 1);
            int unused = !all && !track_in_use(s, row);
            text(r, 20, y, 2, all ? WHITE : unused ? DIM : TRACK_COL[row], buf);
            if (!all) {
                const SeqTrack *tr = &s->track[row];
                if (unused) snprintf(buf, sizeof buf, "EMPTY CH%d", tr->channel + 1);
                else snprintf(buf, sizeof buf, "%s CH%d", tr->type == TRACK_DRUMS ? "DRUMS" : tr->poly ? "POLY" : "NOTES", tr->channel + 1);
                text(r, 60, y + 4, 1, DIM, buf);
                if (sounding && s->playing && seq_track_audible(s, row)) rect(r, 128, y + 2, 10, 10, (Col){90, 220, 120});
                if ((s->live.fill >> row) & 1) text(r, 100, y - 4, 1, YELLOW, "FILL");
            }
            Col mc = muted == n ? (Col){235, 90, 80} : muted ? (Col){150, 70, 60} : (Col){50, 52, 66};
            Col sc = solo == n ? YELLOW : solo ? (Col){150, 135, 50} : (Col){50, 52, 66};
            rect(r, 150, y - 3, 40, 20, mc);
            text(r, 164, y + 1, 2, muted ? WHITE : DIM, "M");
            rect(r, 200, y - 3, 40, 20, sc);
            text(r, 214, y + 1, 2, solo ? (Col){20, 20, 20} : DIM, "S");
            dens /= n; evo /= n;
            draw_bar(r, 260, y + 2, 120, dens, -100, 100, dens < 0 ? (Col){120, 140, 200} : (Col){240, 180, 60});
            snprintf(buf, sizeof buf, "%+d", dens);
            text(r, 388, y + 3, 1, dens ? WHITE : DIM, dens ? buf : "0");
            draw_bar(r, 460, y + 2, 100, evo, 0, 100, (Col){90, 200, 130});
            snprintf(buf, sizeof buf, "%d%%", evo);
            text(r, 568, y + 3, 1, evo ? WHITE : DIM, all ? buf : s->track[row].evo.every ? buf : "OFF");
            continue;
        }
        int bars, beats, waiting = live_switch_in(s, &bars, &beats);
        Col lc = row == sel ? YELLOW : WHITE;
        if (row == PR_SCENE) {
            int k = u->perf_scene, n = live_scene_tracks(s, k);
            text(r, 20, y, 2, lc, "SCENE");
            snprintf(buf, sizeof buf, "%d", k + 1);
            text(r, 150, y, 2, lc, buf);
            snprintf(buf, sizeof buf, n ? "%d TRACK%s" : "EMPTY", n, n == 1 ? "" : "S");
            text(r, 180, y + 4, 1, DIM, buf);
            char what[16];
            snprintf(what, sizeof what, "SCENE %d", s->live.scene_q + 1);
            if (s->live.scene_q >= 0 && waiting) switch_text(buf, sizeof buf, what, bars, beats);
            else if (s->live.scene >= 0) snprintf(buf, sizeof buf, "NOW: SCENE %d", s->live.scene + 1);
            else buf[0] = 0;
            text(r, 300, y + 2, 2, s->live.scene_q >= 0 ? YELLOW : DIM, buf);
        } else if (row == PR_SONG) {
            int k = u->perf_song;
            char nm[SEQ_SONG_NAME + 1];
            song_show_name(k, u->song[k].name, nm);
            text(r, 20, y, 2, lc, "SONG");
            snprintf(buf, sizeof buf, "%d %s", k + 1, u->song[k].used ? nm : "EMPTY");
            text(r, 150, y, 2, u->song[k].used ? lc : DIM, buf);
            if (s->live.song_q >= 0 && waiting) {
                char what[16];
                snprintf(what, sizeof what, "SONG %d", s->live.song_q + 1);
                switch_text(buf, sizeof buf, what, bars, beats);
                text(r, 300, y + 2, 2, YELLOW, buf);
            }
        } else {
            text(r, 20, y, 2, lc, "SWITCH AT");
            snprintf(buf, sizeof buf, "EVERY %d BAR%s", s->live.quant, s->live.quant > 1 ? "S" : "");
            text(r, 150, y, 2, lc, buf);
        }
    }
    const char *h1 = sel < PR_ALL ? "A MUTE  B SOLO  HOLD X FILL  LEFT/RIGHT DENSITY  HOLD Y+LEFT/RIGHT OR STICK EVOLVE"
                   : sel == PR_ALL ? "A MUTE ALL / ALL ON  B CLEAR SOLOS  HOLD X FILL ALL  LEFT/RIGHT DENSITY  Y+L/R EVOLVE"
                   : sel == PR_SCENE ? "LEFT/RIGHT SCENE  A SWITCH AT THE NEXT SWITCH BAR  X SAVE ALL TRACKS HERE  B CANCEL"
                   : sel == PR_SONG ? "LEFT/RIGHT SONG  A SWITCH AT THE NEXT SWITCH BAR (NOW IF STOPPED)  B CANCEL"
                   : "LEFT/RIGHT: HOW OFTEN SCENES AND SONGS MAY SWITCH";
    text(r, 20, H - 34, 1, DIM, h1);
    text(r, 20, H - 20, 1, DIM, "UP/DOWN ROW  L1/R1 ARRANGE AND RECORD PAGES  M CLOSE MENU  START PLAY/STOP");
}


/* ---- ARRANGE page ------------------------------------------------------
 * Rows: ARRANGE on/off, AT THE END loop/stop, then the sections, then
 * + ADD SECTION. On a section: left/right bars, hold Y + left/right scene,
 * X fill (OFF, DRUMS, ALL), A adds a copy below, Select removes it. */
enum { AR_ON, AR_END, AR_FIRST };
static const int ARR_BARS[] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
#define N_ARR_BARS ((int)(sizeof ARR_BARS / sizeof ARR_BARS[0]))
static const char *ARR_FILL_NAME[3] = {"", "+DRUM FILL", "+FILL ALL"};
static const Col SCENE_COL[8] = {{235, 90, 80}, {240, 180, 60}, {90, 200, 130}, {90, 150, 240},
                                 {200, 110, 220}, {80, 200, 210}, {210, 210, 90}, {170, 130, 240}};

static int arr_rows(const Seq *s) { return AR_FIRST + s->arr.n + (s->arr.n < ARR_MAX); }

static void arr_row_text(const Seq *s, int row, char *label, char *val) {
    const Arrange *a = &s->arr;
    if (row == AR_ON) { snprintf(label, 24, "ARRANGE"); snprintf(val, 24, "%s", a->on ? "ON" : "OFF"); return; }
    if (row == AR_END) { snprintf(label, 24, "AT THE END"); snprintf(val, 24, "%s", a->end == ARR_STOP ? "STOP" : "LOOP"); return; }
    int i = row - AR_FIRST;
    if (i >= a->n) { snprintf(label, 24, "+ ADD SECTION"); snprintf(val, 24, "A"); return; }
    const ArrSection *x = &a->sec[i];
    snprintf(label, 24, "%2d SCENE %d", i + 1, x->scene + 1);
    if (x->fill) snprintf(val, 24, "%d %s", x->bars, ARR_FILL_NAME[x->fill % 3]);
    else snprintf(val, 24, "%d BAR%s", x->bars, x->bars > 1 ? "S" : "");
}

static void arr_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    Arrange *a = &s->arr;
    int *row = &u->row[P_ARRANGE], n = arr_rows(s);
    if (*row >= n) *row = n - 1;
    if (dy) { *row = wrap(*row + dy, n); return; }
    int r = *row, i = r - AR_FIRST;
    if (r == AR_ON && (dx || b == B_A)) {
        a->on ^= 1;
        if (a->on && !a->n) live_arr_insert(s, 0, (ArrSection){(uint8_t)(s->live.scene > 0 ? s->live.scene : 0), 8, ARR_FILL_OFF});
        say(u, a->on ? "ARRANGE ON: START PLAYS FROM SECTION 1" : "ARRANGE OFF");
        return;
    }
    if (r == AR_END && (dx || b == B_A)) { a->end = a->end == ARR_STOP ? ARR_LOOP : ARR_STOP; return; }
    if (r < AR_FIRST) return;
    if (i >= a->n) {   /* + ADD SECTION */
        if (b != B_A) return;
        ArrSection x = a->n ? a->sec[a->n - 1] : (ArrSection){0, 8, ARR_FILL_OFF};
        if (a->n) x.scene = (uint8_t)((x.scene + 1) % LIVE_SCENES);
        live_arr_insert(s, a->n, x);
        if (a->n == ARR_MAX) *row = AR_FIRST + a->n - 1;
        return;
    }
    ArrSection *x = &a->sec[i];
    if (dx && u->held[B_Y]) { x->scene = (uint8_t)wrap(x->scene + dx, LIVE_SCENES); u->y_used = 1; }
    else if (dx) {
        int k = 0;
        while (k < N_ARR_BARS - 1 && ARR_BARS[k] < x->bars) k++;
        x->bars = (uint8_t)ARR_BARS[clampi(k + dx, 0, N_ARR_BARS - 1)];
    }
    else if (b == B_X) x->fill = (uint8_t)((x->fill + 1) % 3);
    else if (b == B_A) {
        if (a->n >= ARR_MAX) { say(u, "16 SECTIONS IS THE MOST"); return; }
        live_arr_insert(s, i + 1, *x);
        *row = r + 1;
    }
    else if (b == B_B) u->page = P_MENU;
    if (!live_scene_tracks(s, x->scene) && (dx || b == B_A)) say(u, "SCENE EMPTY: SAVE IT ON PERFORM");
}

/* Select on ARRANGE: remove the section under the cursor. */
static void arr_select(Ui *u) {
    player_lock(u->p);
    Seq *s = &u->p->seq;
    int i = u->row[P_ARRANGE] - AR_FIRST;
    if (i >= 0 && i < s->arr.n) {
        live_arr_delete(s, i);
        snprintf(u->msg_buf, sizeof u->msg_buf, "SECTION %d REMOVED", i + 1);
        say(u, u->msg_buf);
        if (u->row[P_ARRANGE] >= arr_rows(s)) u->row[P_ARRANGE] = arr_rows(s) - 1;
        if (!u->save_due) u->save_due = SDL_GetTicks() + 2000;
    }
    player_unlock(u->p);
}

static void draw_arr_side(SDL_Renderer *r, const Seq *s, const Ui *u) {
    char buf[64], tm[12];
    const Arrange *a = &s->arr;
    int total = live_arr_total(s), bar, at = live_arr_where(s, &bar);
    text(r, 392, 62, 2, YELLOW, "ARRANGE");
    bars_time(s, total, tm, sizeof tm);
    snprintf(buf, sizeof buf, "%d SECTION%s, %d BARS, %s", a->n, a->n == 1 ? "" : "S", total, tm);
    text(r, 392, 86, 1, WHITE, buf);
    if (total) {   /* the timeline: one block per section, as wide as its bars, coloured by scene */
        int x = 392, w = 232, played = 0;
        for (int i = 0; i < a->n; i++) {
            int bw = a->sec[i].bars * w / total;
            if (bw < 2) bw = 2;
            Col c = SCENE_COL[a->sec[i].scene % 8];
            if (!a->on) c = (Col){(uint8_t)(c.r / 2), (uint8_t)(c.g / 2), (uint8_t)(c.b / 2)};
            rect(r, x, 102, bw - 1, 18, c);
            if (a->sec[i].fill) rect(r, x + bw - 1 - (bw > 8 ? 4 : 1), 102, bw > 8 ? 4 : 1, 18, WHITE);
            if (u->row[P_ARRANGE] - AR_FIRST == i) frame(r, x - 1, 100, bw + 1, 22, 1, YELLOW);
            if (i < at) played += a->sec[i].bars;
            x += bw;
        }
        if (at >= 0) {
            int px = 392 + (played + bar) * w / total;
            rect(r, px, 96, 2, 30, WHITE);
        }
    }
    if (at >= 0) snprintf(buf, sizeof buf, "NOW: SECTION %d, BAR %d OF %d", at + 1, bar + 1, a->sec[at].bars);
    else snprintf(buf, sizeof buf, a->on ? "START PLAYS FROM SECTION 1" : "OFF: START PLAYS AS USUAL");
    text(r, 392, 134, 1, at >= 0 ? (Col){90, 220, 120} : DIM, buf);
    static const char *HELP[] = {
        "EACH SECTION PLAYS A SCENE (SLOT N", "OF EVERY TRACK) FOR ITS BARS, THEN", "THE NEXT SECTION'S SCENE COMES IN",
        "ON THE BAR LINE. A FILL PLAYS IN A", "SECTION'S LAST BAR.", "",
        "MAKE SCENES ON PERFORM: SCENE ROW,", "X SAVES EVERY TRACK INTO IT.", "",
        "WITH SONG LENGTH ON RECORD, OR", "AT THE END: STOP, A TAKE ENDS", "BY ITSELF. SAVED WITH THE SONG."};
    for (unsigned k = 0; k < sizeof HELP / sizeof HELP[0]; k++) text(r, 392, 158 + 12 * (int)k, 1, DIM, HELP[k]);
    text(r, 20, H - 34, 1, DIM, "SECTION: LEFT/RIGHT BARS  HOLD Y+L/R SCENE  X FILL  A ADD A COPY  SELECT REMOVE");
    text(r, 20, H - 20, 1, DIM, "UP/DOWN ROW  B BACK TO MENU  L1/R1 OTHER LIVE PAGES  M CLOSE MENU  START PLAY/STOP");
}

static int live_rows(const Seq *s, int page) { return page == P_PERFORM ? PERF_ROWS : page == P_ARRANGE ? arr_rows(s) : REC_ROWS; }
