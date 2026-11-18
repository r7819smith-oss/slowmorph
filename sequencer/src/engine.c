#include "engine.h"

#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static uint32_t rnd(Seq *s) {
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
    s->clock_out = 1;
    s->rng = 0x2545F491u;
    s->send = send;
    s->ctx = ctx;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        tr->len = SEQ_STEPS;
        tr->channel = (uint8_t)t;
        tr->sounding = -1;
        for (int i = 0; i < SEQ_STEPS; i++)
            tr->step[i] = (SeqStep){0, base[t], 100, 50, 100};
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
    s->playing = 1;
    if (s->clock_out) emit1(s, 0xFA);
}

void seq_stop(Seq *s) {
    if (!s->playing) return;
    s->playing = 0;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        release(s, &s->track[t]);
        emit3(s, 0xB0 | s->track[t].channel, 123, 0);   /* all notes off */
    }
    if (s->clock_out) emit1(s, 0xFC);
}

void seq_tick(Seq *s) {
    if (!s->playing) return;
    uint32_t now = s->tick;
    if (s->clock_out && now % (SEQ_PPQN / 24) == 0) emit1(s, 0xF8);

    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        if (tr->sounding >= 0 && (int32_t)(now - tr->off_tick) >= 0) release(s, tr);
        if (tr->mute) continue;

        uint32_t idx = (now / SEQ_TICKS_PER_STEP) % tr->len;
        uint32_t phase = now % SEQ_TICKS_PER_STEP;
        uint32_t when = (idx & 1) ? (uint32_t)s->swing : 0;
        if (phase != when) continue;

        const SeqStep *st = &tr->step[idx];
        if (!st->on) continue;
        if (st->prob < 100 && (int)(rnd(s) % 100) >= st->prob) continue;

        release(s, tr);   /* retrigger */
        emit3(s, 0x90 | tr->channel, st->note, st->vel);
        tr->sounding = st->note;
        uint32_t gl = (uint32_t)st->gate * SEQ_TICKS_PER_STEP / 100;
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
    if (dn) { st->note = (uint8_t)clampi(st->note + dn, 0, 127); st->on = 1; }
    st->vel = (uint8_t)clampi(st->vel + dv, 1, 127);
    st->gate = (uint8_t)clampi(st->gate + dg, 5, 100);
    st->prob = (uint8_t)clampi(st->prob + dp, 0, 100);
}

void seq_track_mute_toggle(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    tr->mute ^= 1;
    if (tr->mute) release(s, tr);
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

void seq_note_name(int note, char out[8]) {
    static const char *n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    note = clampi(note, 0, 127);
    snprintf(out, 8, "%s%d", n[note % 12], note / 12 - 1);
}

void seq_save(const Seq *s, FILE *f) {
    fprintf(f, "seq4h 1\nbpm %d\nswing %d\nclock %d\n", s->bpm, s->swing, s->clock_out);
    for (int t = 0; t < SEQ_TRACKS; t++) {
        const SeqTrack *tr = &s->track[t];
        fprintf(f, "t %d %d %d %d\n", t, tr->channel, tr->len, tr->mute);
        for (int i = 0; i < SEQ_STEPS; i++) {
            const SeqStep *st = &tr->step[i];
            fprintf(f, "s %d %d %d %d %d %d %d\n", t, i, st->on, st->note, st->vel, st->gate, st->prob);
        }
    }
}

int seq_load(Seq *s, FILE *f) {
    char line[128];
    int a, b, c, d, e, g, h;
    if (!fgets(line, sizeof line, f) || strncmp(line, "seq4h ", 6) != 0) return -1;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "bpm %d", &a) == 1) s->bpm = clampi(a, 30, 300);
        else if (sscanf(line, "swing %d", &a) == 1) s->swing = clampi(a, 0, SEQ_MAX_SWING);
        else if (sscanf(line, "clock %d", &a) == 1) s->clock_out = a != 0;
        else if (sscanf(line, "t %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].channel = (uint8_t)clampi(b, 0, 15);
            s->track[a].len = (uint8_t)clampi(c, 1, SEQ_STEPS);
            s->track[a].mute = d != 0;
        } else if (sscanf(line, "s %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h) == 7) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_STEPS) continue;
            s->track[a].step[b] = (SeqStep){(uint8_t)(c != 0), (uint8_t)clampi(d, 0, 127),
                                            (uint8_t)clampi(e, 1, 127), (uint8_t)clampi(g, 5, 100),
                                            (uint8_t)clampi(h, 0, 100)};
        }
    }
    return 0;
}
