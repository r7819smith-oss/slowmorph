#include "engine.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gen.h"

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

uint32_t seq_rand(Seq *s) {
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}

static void emit3(Seq *s, uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m[3] = {a, b, c};
    if (s->send) s->send(s->ctx, m, 3);
}

static void emit1(Seq *s, uint8_t a) {
    if (s->send) s->send(s->ctx, &a, 1);
}

static void release(Seq *s, SeqTrack *t) {
    if (t->sounding >= 0) {
        emit3(s, 0x80 | t->channel, (uint8_t)t->sounding, 0);
        t->sounding = -1;
    }
}

void seq_init(Seq *s, SeqSend send, void *ctx) {
    static const uint8_t base[SEQ_TRACKS] = {36, 48, 60, 72};
    memset(s, 0, sizeof *s);
    s->bpm = 120;
    s->scale = SCALE_MIN_PENT;
    s->clock_out = 1;
    s->style = STYLE_AMBIENT;
    s->rng = 0x2545F491u;
    s->send = send;
    s->ctx = ctx;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        tr->len = SEQ_DEFAULT_LEN;
        tr->channel = (uint8_t)t;
        tr->sounding = -1;
        tr->last_note = -1;
        tr->gen = (GenSettings){GEN_NONE, {0, 0, 0, 0}, (uint8_t)(base[t] - 12), 2, 0x9E37u + (uint32_t)t * 0x2F1Bu};
        tr->evo = (EvolveSettings){0, 20, EVO_PITCH | EVO_RHYTHM, 0, 0, 0};
        tr->chain_bars = 4;
        for (int i = 0; i < SEQ_STEPS; i++)
            tr->step[i] = (SeqStep){0, base[t], 100, 50, 100, 0, 0};
    }
}

void seq_load_demo(Seq *s) {
    static const int kick[] = {0, 4, 8, 12};
    static const int hat[] = {2, 6, 10, 14};
    static const int bass[] = {0, 3, 6, 10, 12};
    static const int bassn[] = {36, 36, 39, 34, 36};
    static const int lead[] = {0, 4, 7, 11, 14};
    static const int leadn[] = {67, 70, 72, 70, 74};
    for (unsigned k = 0; k < 4; k++) s->track[0].step[kick[k]].on = 1;
    for (unsigned k = 0; k < 4; k++) {
        SeqStep *st = &s->track[1].step[hat[k]];
        st->on = 1; st->note = 54; st->vel = 80; st->gate = 25;
    }
    for (unsigned k = 0; k < 5; k++) {
        SeqStep *st = &s->track[2].step[bass[k]];
        st->on = 1; st->note = (uint8_t)bassn[k]; st->gate = 70;
    }
    for (unsigned k = 0; k < 5; k++) {
        SeqStep *st = &s->track[3].step[lead[k]];
        st->on = 1; st->note = (uint8_t)leadn[k]; st->vel = 90; st->prob = 80;
    }
}

void seq_start(Seq *s) {
    if (s->playing) return;
    s->tick = 0;
    s->idle_tick = 0;
    s->playing = 1;
    if (s->clock_out) emit1(s, 0xFA);
}

void seq_stop(Seq *s) {
    if (!s->playing) return;
    s->playing = 0;
    s->resync = 0;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        release(s, &s->track[t]);
        emit3(s, 0xB0 | s->track[t].channel, 123, 0);   /* all notes off */
    }
    if (s->clock_out) emit1(s, 0xFC);
}

void seq_panic(Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        if (tr->last_note >= 0 && tr->last_note != tr->sounding)
            emit3(s, 0x80 | tr->channel, (uint8_t)tr->last_note, 0);
        release(s, tr);
    }
    for (int t = 0; t < SEQ_TRACKS; t++) {
        uint8_t ch = s->track[t].channel;
        int dup = 0;
        for (int u = 0; u < t; u++) dup |= s->track[u].channel == ch;
        if (dup) continue;
        emit3(s, 0xB0 | ch, 120, 0);   /* all sound off */
        emit3(s, 0xB0 | ch, 123, 0);   /* all notes off */
    }
}

void seq_reconnected(Seq *s) {
    seq_panic(s);
    if (s->playing && s->clock_out) s->resync = 1;
}

void seq_tick(Seq *s) {
    if (!s->playing) {
        /* Keep sending clock while stopped, so the FM1 locks to the tempo
         * before Start (and shows it). Start then restarts the count. */
        if (s->clock_out && s->idle_tick++ % (SEQ_PPQN / 24) == 0) emit1(s, 0xF8);
        return;
    }
    uint32_t now = s->tick;
    if (s->resync && now % (SEQ_PPQN * 4) == 0) {   /* bar line: the receiver's beat 1 */
        s->resync = 0;
        if (s->clock_out) { emit1(s, 0xFC); emit1(s, 0xFA); }
    }
    if (s->clock_out && now % (SEQ_PPQN / 24) == 0) emit1(s, 0xF8);

    if (now && now % (SEQ_PPQN * 4) == 0) {          /* a bar boundary */
        uint32_t bar = now / (SEQ_PPQN * 4);
        for (int t = 0; t < SEQ_TRACKS; t++) seq_chain_bar(s, t, bar);
        for (int t = 0; t < SEQ_TRACKS; t++) {
            uint8_t e = s->track[t].evo.every;
            if (e && bar % e == 0) gen_evolve_bar(s, t);
        }
    }

    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        if (tr->sounding >= 0 && (int32_t)(now - tr->off_tick) >= 0) release(s, tr);
        if (!seq_track_audible(s, t)) continue;

        uint32_t gstep = now / SEQ_TICKS_PER_STEP;
        uint32_t idx = gstep % tr->len;
        uint32_t phase = now % SEQ_TICKS_PER_STEP;
        /* Swing follows the shared 1/16 grid, not the track's own step number,
         * so an odd-length track stays in the same groove as the others. */
        uint32_t when = (gstep & 1) ? (uint32_t)s->swing : 0;
        if (phase != when) continue;

        const SeqStep *st = &tr->step[idx];
        if (!st->on) continue;
        if (st->prob < 100 && (int)(seq_rand(s) % 100) >= st->prob) continue;

        release(s, tr);   /* retrigger */
        emit3(s, 0x90 | tr->channel, st->note, st->vel);
        tr->sounding = st->note;
        tr->last_note = st->note;
        uint32_t gl = (uint32_t)st->gate * SEQ_TICKS_PER_STEP / 100 + (uint32_t)st->hold * SEQ_TICKS_PER_STEP;
        tr->off_tick = now + (gl ? gl : 1);
    }
    s->tick++;
}

int seq_playhead(const Seq *s, int t) {
    if (!s->playing) return -1;
    uint32_t last = s->tick ? s->tick - 1 : 0;
    return (int)((last / SEQ_TICKS_PER_STEP) % s->track[t].len);
}

void seq_step_toggle(Seq *s, int t, int i) { s->track[t].step[i].on ^= 1; }

void seq_step_adjust(Seq *s, int t, int i, int dn, int dv, int dg, int dp) {
    SeqStep *st = &s->track[t].step[i];
    if (dn) { st->note = (uint8_t)gen_step_note(s, st->note, dn); st->on = 1; }
    st->vel = (uint8_t)clampi(st->vel + dv, 1, 127);
    st->gate = (uint8_t)clampi(st->gate + dg, 5, 100);
    st->prob = (uint8_t)clampi(st->prob + dp, 0, 100);
}

void seq_step_hold(Seq *s, int t, int i, int d) {
    SeqStep *st = &s->track[t].step[i];
    st->hold = (uint8_t)clampi(st->hold + d, 0, SEQ_MAX_HOLD);
}

int seq_any_solo(const Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++) if (s->track[t].solo) return 1;
    return 0;
}

int seq_track_audible(const Seq *s, int t) {
    const SeqTrack *tr = &s->track[t];
    return !tr->mute && (tr->solo || !seq_any_solo(s));
}

/* After a mute or solo change: note-off for anything that just went silent. */
static void release_silenced(Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++)
        if (!seq_track_audible(s, t)) release(s, &s->track[t]);
}

void seq_track_mute_toggle(Seq *s, int t) {
    s->track[t].mute ^= 1;
    release_silenced(s);
}

void seq_track_solo_toggle(Seq *s, int t) {
    s->track[t].solo ^= 1;
    release_silenced(s);
}

void seq_track_len(Seq *s, int t, int d) {
    SeqTrack *tr = &s->track[t];
    tr->len = (uint8_t)clampi(tr->len + d, 1, SEQ_STEPS);
}

void seq_track_channel(Seq *s, int t, int d) {
    SeqTrack *tr = &s->track[t];
    release(s, tr);
    tr->channel = (uint8_t)clampi(tr->channel + d, 0, 15);
}

void seq_adjust_bpm(Seq *s, int d) { s->bpm = clampi(s->bpm + d, 30, 300); }
void seq_adjust_swing(Seq *s, int d) { s->swing = clampi(s->swing + d, 0, SEQ_MAX_SWING); }

void seq_set_scale(Seq *s, int scale, int root, int snap) {
    s->scale = clampi(scale, 0, scale_count() - 1);
    s->root = ((root % 12) + 12) % 12;
    if (!snap) return;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        for (int i = 0; i < SEQ_STEPS; i++) {
            tr->step[i].note = (uint8_t)gen_snap_note(s, tr->step[i].note);
            tr->anchor[i].note = (uint8_t)gen_snap_note(s, tr->anchor[i].note);
        }
    }
}

void seq_slot_store(Seq *s, int t, int k) {
    if (k < 0 || k >= SEQ_SLOTS) return;
    SeqTrack *tr = &s->track[t];
    memcpy(tr->slot[k].step, tr->step, sizeof tr->step);
    tr->slot[k].len = tr->len;
    tr->slot[k].used = 1;
    tr->cur_slot = (uint8_t)k;
}

int seq_slot_recall(Seq *s, int t, int k) {
    if (k < 0 || k >= SEQ_SLOTS || !s->track[t].slot[k].used) return -1;
    SeqTrack *tr = &s->track[t];
    memcpy(tr->step, tr->slot[k].step, sizeof tr->step);
    tr->len = tr->slot[k].len;
    tr->cur_slot = (uint8_t)k;
    return 0;
}

void seq_slot_clear(Seq *s, int t, int k) {
    if (k < 0 || k >= SEQ_SLOTS) return;
    memset(&s->track[t].slot[k], 0, sizeof s->track[t].slot[k]);
}

int seq_slot_copy(Seq *s, int t, int from, int to) {
    if (from < 0 || from >= SEQ_SLOTS || to < 0 || to >= SEQ_SLOTS || !s->track[t].slot[from].used) return -1;
    if (from != to) s->track[t].slot[to] = s->track[t].slot[from];
    return 0;
}

static void slot_default_name(int k, char *out) { snprintf(out, SEQ_SLOT_NAME + 1, "SLOT %d", (k & 7) + 1); }

void seq_slot_set_name(Seq *s, int t, int k, const char *name) {
    if (k < 0 || k >= SEQ_SLOTS) return;
    char buf[SEQ_SLOT_NAME + 1], def[SEQ_SLOT_NAME + 1];
    int n = 0;
    for (; name && name[n] && n < SEQ_SLOT_NAME; n++) {
        char c = name[n];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        buf[n] = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ? c : ' ';
    }
    while (n > 0 && buf[n - 1] == ' ') n--;
    buf[n] = 0;
    slot_default_name(k, def);
    if (!strcmp(buf, def)) buf[0] = 0;
    memcpy(s->track[t].slot[k].name, buf, sizeof buf);
}

void seq_slot_name(const Seq *s, int t, int k, char *out) {
    if (k >= 0 && k < SEQ_SLOTS && s->track[t].slot[k].name[0]) snprintf(out, SEQ_SLOT_NAME + 1, "%s", s->track[t].slot[k].name);
    else slot_default_name(k, out);
}

void seq_chain_set(Seq *s, int t, const uint8_t *seq, int n, int bars, int morph) {
    SeqTrack *tr = &s->track[t];
    n = clampi(n, 0, SEQ_SLOTS);
    for (int i = 0; i < n; i++) tr->chain[i] = (uint8_t)clampi(seq[i], 0, SEQ_SLOTS - 1);
    tr->chain_len = (uint8_t)n;
    tr->chain_pos = 0;
    tr->chain_bars = (uint8_t)(bars <= 1 ? 1 : bars <= 2 ? 2 : bars <= 4 ? 4 : bars <= 8 ? 8 : 16);
    tr->chain_morph = (uint8_t)clampi(morph, 0, 100);
    if (n) seq_slot_recall(s, t, tr->chain[0]);
}

void seq_chain_bar(Seq *s, int t, uint32_t bar) {
    SeqTrack *tr = &s->track[t];
    if (!tr->chain_len) return;
    if (bar % tr->chain_bars == 0) {
        tr->chain_pos = (uint8_t)((tr->chain_pos + 1) % tr->chain_len);
        int k = tr->chain[tr->chain_pos];
        if (!tr->chain_morph) { seq_slot_recall(s, t, k); return; }
        if (tr->slot[k].used) { tr->len = tr->slot[k].len; tr->cur_slot = (uint8_t)k; }
    }
    if (tr->chain_morph) {
        const SeqSlot *sl = &tr->slot[tr->chain[tr->chain_pos]];
        if (sl->used) gen_morph(s, t, sl->step, tr->chain_morph);
    }
}

void seq_note_name(int note, char out[8]) {
    static const char *n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    note = clampi(note, 0, 127);
    snprintf(out, 8, "%s%d", n[note % 12], note / 12 - 1);
}

void seq_save(const Seq *s, FILE *f) {
    fprintf(f, "seq4h 1\nbpm %d\nswing %d\nclock %d\nscale %d %d\nstyle %d\n", s->bpm, s->swing, s->clock_out, s->scale, s->root, s->style);
    for (int t = 0; t < SEQ_TRACKS; t++) {
        const SeqTrack *tr = &s->track[t];
        fprintf(f, "t %d %d %d %d\n", t, tr->channel, tr->len, tr->mute);
        fprintf(f, "g %d %d %d %d %d %d %d %d\n", t, tr->gen.type, tr->gen.p[0], tr->gen.p[1], tr->gen.p[2],
                tr->gen.p[3], tr->gen.base, tr->gen.span);
        fprintf(f, "e %d %d %d %d\n", t, tr->evo.every, tr->evo.amount, tr->evo.kinds);
        fprintf(f, "ev %d %d %d %d\n", t, tr->evo.pull, tr->evo.min_hits, tr->evo.max_hits);
        fprintf(f, "gs %d %u\n", t, (unsigned)tr->gen.state);
        for (int k = 0; k < SEQ_SLOTS; k++) {
            const SeqSlot *sl = &tr->slot[k];
            if (sl->name[0]) {   /* "sn TRACK SLOT NAME", spaces written as '_'; older versions skip this line */
                char nm[SEQ_SLOT_NAME + 1];
                snprintf(nm, sizeof nm, "%s", sl->name);
                for (char *c = nm; *c; c++) if (*c == ' ') *c = '_';
                fprintf(f, "sn %d %d %s\n", t, k, nm);
            }
            if (!sl->used) continue;
            fprintf(f, "sl %d %d %d\n", t, k, sl->len);
            for (int i = 0; i < SEQ_STEPS; i++) {
                const SeqStep *st = &sl->step[i];
                fprintf(f, "ss %d %d %d %d %d %d %d %d %d\n", t, k, i, st->on, st->note, st->vel, st->gate, st->prob, st->lock);
                if (st->hold) fprintf(f, "sh %d %d %d %d\n", t, k, i, st->hold);
            }
        }
        fprintf(f, "ch %d %d %d %d %d %d %d", t, tr->cur_slot, tr->chain_len, tr->chain_pos, tr->chain_bars, tr->chain_morph, SEQ_SLOTS);
        for (int k = 0; k < SEQ_SLOTS; k++) fprintf(f, " %d", tr->chain[k]);
        fputc('\n', f);
        if (tr->has_anchor)
            for (int i = 0; i < SEQ_STEPS; i++) {
                const SeqStep *st = &tr->anchor[i];
                fprintf(f, "a %d %d %d %d %d %d %d\n", t, i, st->on, st->note, st->vel, st->gate, st->prob);
                if (st->hold) fprintf(f, "ah %d %d %d\n", t, i, st->hold);
            }
        for (int i = 0; i < SEQ_STEPS; i++) {
            const SeqStep *st = &tr->step[i];
            fprintf(f, "s %d %d %d %d %d %d %d %d\n", t, i, st->on, st->note, st->vel, st->gate, st->prob, st->lock);
            if (st->hold) fprintf(f, "h %d %d %d\n", t, i, st->hold);
        }
    }
}

int seq_save_file(const Seq *s, const char *path) {
    size_t n = strlen(path) + 5;
    char *tmp = malloc(n);
    if (!tmp) return -1;
    snprintf(tmp, n, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    int rc = -1;
    if (f) {
        seq_save(s, f);
        /* on the SD card before the rename, so a power cut leaves the old or the new file, never half of one */
        rc = (fflush(f) == 0 && fsync(fileno(f)) == 0) ? 0 : -1;
        if (fclose(f) != 0) rc = -1;
        if (rc == 0 && rename(tmp, path) != 0) rc = -1;
        if (rc != 0) remove(tmp);
    }
    free(tmp);
    return rc;
}

int seq_slots_used(const Seq *s) {
    int n = 0;
    for (int t = 0; t < SEQ_TRACKS; t++)
        for (int k = 0; k < SEQ_SLOTS; k++) n += s->track[t].slot[k].used;
    return n;
}

int seq_load(Seq *s, FILE *f) {
    char line[128], nm[16];
    int a, b, c, d, e, g, h, k, m, n;
    unsigned u;
    if (!fgets(line, sizeof line, f) || strncmp(line, "seq4h ", 6) != 0) return -1;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "bpm %d", &a) == 1) s->bpm = clampi(a, 30, 300);
        else if (sscanf(line, "swing %d", &a) == 1) s->swing = clampi(a, 0, SEQ_MAX_SWING);
        else if (sscanf(line, "clock %d", &a) == 1) s->clock_out = a != 0;
        else if (sscanf(line, "style %d", &a) == 1) s->style = clampi(a, 0, style_count() - 1);
        /* hold lines follow the step they belong to, so the step line has already reset hold */
        else if (sscanf(line, "h %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) s->track[a].step[b].hold = (uint8_t)clampi(c, 0, SEQ_MAX_HOLD);
        } else if (sscanf(line, "ah %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) s->track[a].anchor[b].hold = (uint8_t)clampi(c, 0, SEQ_MAX_HOLD);
        } else if (sscanf(line, "sh %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_SLOTS && c >= 0 && c < SEQ_STEPS)
                s->track[a].slot[b].step[c].hold = (uint8_t)clampi(d, 0, SEQ_MAX_HOLD);
        }
        else if (sscanf(line, "sn %d %d %15s", &a, &b, nm) == 3) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_SLOTS) continue;
            for (char *q = nm; *q; q++) if (*q == '_') *q = ' ';
            seq_slot_set_name(s, a, b, nm);
        }
        else if (sscanf(line, "sl %d %d %d", &a, &b, &c) == 3) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_SLOTS) continue;
            s->track[a].slot[b].len = (uint8_t)clampi(c, 1, SEQ_STEPS);
            s->track[a].slot[b].used = 1;
        } else if (sscanf(line, "ss %d %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h, &k, &n) == 9) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_SLOTS || c < 0 || c >= SEQ_STEPS) continue;
            s->track[a].slot[b].step[c] = (SeqStep){(uint8_t)(d != 0), (uint8_t)clampi(e, 0, 127),
                                                    (uint8_t)clampi(g, 1, 127), (uint8_t)clampi(h, 5, 100),
                                                    (uint8_t)clampi(k, 0, 100), (uint8_t)(n != 0), 0};
        } else if (sscanf(line, "ch %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h) == 7) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            SeqTrack *tr = &s->track[a];
            int ch[SEQ_SLOTS] = {0}, off = 0, pos = 0, v;
            char *q = line;
            for (int skip = 0; skip < 8 && sscanf(q, "%*s%n", &off) == 0; skip++) q += off;   /* past the 8 header fields */
            for (int i = 0; i < SEQ_SLOTS && sscanf(q, "%d%n", &v, &off) == 1; i++) { ch[i] = v; q += off; pos = i + 1; }
            tr->cur_slot = (uint8_t)clampi(b, 0, SEQ_SLOTS - 1);
            tr->chain_len = (uint8_t)clampi(c, 0, pos);
            tr->chain_pos = (uint8_t)(tr->chain_len ? clampi(d, 0, tr->chain_len - 1) : 0);
            tr->chain_bars = (uint8_t)(e <= 1 ? 1 : e <= 2 ? 2 : e <= 4 ? 4 : e <= 8 ? 8 : 16);
            tr->chain_morph = (uint8_t)clampi(g, 0, 100);
            for (int i = 0; i < SEQ_SLOTS; i++) tr->chain[i] = (uint8_t)clampi(ch[i], 0, SEQ_SLOTS - 1);
        }
        else if (sscanf(line, "scale %d %d", &a, &b) == 2) { s->scale = clampi(a, 0, scale_count() - 1); s->root = clampi(b, 0, 11); }
        else if (sscanf(line, "g %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h, &k) == 8) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            gen_load_settings(&s->track[a].gen, b, c, d, e, g, h, k);
        } else if (sscanf(line, "gs %d %u", &a, &u) == 2) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].gen.state = u ? u : 1;
        } else if (sscanf(line, "ev %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            EvolveSettings *ev = &s->track[a].evo;
            ev->pull = (uint8_t)clampi(b, 0, 100);
            ev->min_hits = (uint8_t)clampi(c, 0, SEQ_STEPS);
            ev->max_hits = (uint8_t)(d <= 0 ? 0 : clampi(d, ev->min_hits, SEQ_STEPS));
        } else if (sscanf(line, "a %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h) == 7) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_STEPS) continue;
            s->track[a].anchor[b] = (SeqStep){(uint8_t)(c != 0), (uint8_t)clampi(d, 0, 127),
                                              (uint8_t)clampi(e, 1, 127), (uint8_t)clampi(g, 5, 100),
                                              (uint8_t)clampi(h, 0, 100), 0, 0};
            s->track[a].has_anchor = 1;
        } else if (sscanf(line, "e %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            EvolveSettings *ev = &s->track[a].evo;
            ev->every = (uint8_t)clampi(b, 0, 8);
            ev->amount = (uint8_t)clampi(c, 0, 100);
            ev->kinds = (uint8_t)(d & EVO_ALL);
        } else if (sscanf(line, "t %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].channel = (uint8_t)clampi(b, 0, 15);
            s->track[a].len = (uint8_t)clampi(c, 1, SEQ_STEPS);
            s->track[a].mute = d != 0;
        } else if ((m = sscanf(line, "s %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h, &n)) >= 7) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_STEPS) continue;
            s->track[a].step[b] = (SeqStep){(uint8_t)(c != 0), (uint8_t)clampi(d, 0, 127),
                                            (uint8_t)clampi(e, 1, 127), (uint8_t)clampi(g, 5, 100),
                                            (uint8_t)clampi(h, 0, 100), (uint8_t)(m == 8 && n != 0), 0};
        }
    }
    return 0;
}
