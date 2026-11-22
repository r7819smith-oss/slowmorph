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
    text(r, 20, H - 20, 1, DIM, "B BACK TO MENU  M CLOSE MENU  START PLAY/STOP");
}
