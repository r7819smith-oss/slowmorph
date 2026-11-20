/* Tests for the ambient additions: the DRONE, DRIFT and BELL generators,
 * chords stacked on a track's notes, velocity swells, rests, and slow
 * evolution (16 to 64 bars). Pure engine code: no SDL, no MIDI, no hardware. */
#include "../src/engine.h"
#include "../src/gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* D major pentatonic, the ambient style's scale. */
static void dmaj_pent(Seq *s) { seq_set_scale(s, 8, 2, 0); }

static int pc(const Seq *s, int note) { return ((note - s->root) % 12 + 12) % 12; }

static void fresh(Seq *s, int t, int type, int len) {
    s->track[t].len = (uint8_t)len;
    for (int i = 0; i < SEQ_STEPS; i++) s->track[t].step[i].on = 0;
    gen_set_type(s, t, type);
}

static void test_drone(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    s.rng = 12345;
    dmaj_pent(&s);
    SeqTrack *tr = &s.track[0];
    tr->gen.base = 38; tr->gen.span = 2;   /* D2 */
    fresh(&s, 0, GEN_DRONE, 32);
    CHECK(strcmp(gen_name(GEN_DRONE), "DRONE") == 0 && strcmp(gen_short_name(GEN_DRONE), "DRONE") == 0);
    gen_apply(&s, 0);
    /* a fresh track gets one note on the root, held for the whole pattern */
    CHECK(gen_hits(&s, 0) == 1);
    CHECK(tr->step[0].on && pc(&s, tr->step[0].note) == 0);
    CHECK(tr->step[0].hold == 31 && tr->step[0].gate == 100);

    /* MOVE 0: rolling never moves the drone */
    tr->gen.p[1] = 0;
    for (int r = 0; r < 50; r++) { gen_roll(&s, 0); CHECK(tr->step[0].note == 38); }

    /* MOVE 100%, HOME 0: every roll moves to another stable tone within a fifth */
    tr->gen.p[1] = 10; tr->gen.p[2] = 0;
    int moved = 0;
    for (int r = 0; r < 50; r++) {
        int before = tr->step[0].note;
        gen_roll(&s, 0);
        int now = tr->step[0].note, p = pc(&s, now);
        CHECK(seq_in_scale(&s, now));
        CHECK(abs(now - before) <= 7);
        CHECK(p == 0 || p == 7 || p == 9 || p == 5 || p == 3 || p == 8);
        CHECK(now >= 38 && now <= 38 + 24);
        moved += now != before;
    }
    CHECK(moved == 50);

    /* HOME 100%: off the root, the next move goes home */
    tr->gen.p[2] = 10;
    if (pc(&s, tr->step[0].note) == 0) { tr->gen.p[2] = 0; gen_roll(&s, 0); tr->gen.p[2] = 10; }
    CHECK(pc(&s, tr->step[0].note) != 0);
    gen_roll(&s, 0);
    CHECK(tr->step[0].note == 38);

    /* NOTES 2: two notes half a pattern apart, the second a fifth above, holds meet */
    tr->gen.p[0] = 2; tr->gen.p[1] = 0;
    gen_apply(&s, 0);
    CHECK(gen_hits(&s, 0) == 2);
    CHECK(tr->step[0].on && tr->step[16].on);
    CHECK(tr->step[16].note == 45);   /* A2 over D2 */
    CHECK(tr->step[0].hold == 15 && tr->step[16].hold == 15);

    /* locked steps are left alone */
    tr->step[8] = (SeqStep){1, 50, 99, 30, 100, 1, 0, 0};
    gen_apply(&s, 0);
    CHECK(tr->step[8].on && tr->step[8].note == 50 && tr->step[8].vel == 99 && tr->step[8].lock);
}

static void test_drift(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    s.rng = 777;
    dmaj_pent(&s);
    SeqTrack *tr = &s.track[2];
    tr->gen.base = 62; tr->gen.span = 2;
    fresh(&s, 2, GEN_DRIFT, 20);
    uint8_t pool[64];
    int n = gen_pool(&s, 2, pool);
    for (int r = 0; r < 200; r++) {
        tr->gen.p[0] = (uint8_t)(1 + r % 8);
        tr->gen.p[1] = (uint8_t)(r % 11);
        tr->gen.p[2] = (uint8_t)(1 + r % 3);
        gen_roll(&s, 2);
        CHECK(gen_hits(&s, 2) == tr->gen.p[0]);
        int prev_deg = -1, prev_i = -1;
        for (int i = 0; i < tr->len; i++) {
            const SeqStep *st = &tr->step[i];
            if (!st->on) continue;
            CHECK(st->note >= pool[0] && st->note <= pool[n - 1] && seq_in_scale(&s, st->note));
            int deg = gen_note_degree(&s, st->note);
            if (prev_deg >= 0) CHECK(abs(deg - prev_deg) <= tr->gen.p[2]);
            /* a held note ends by the time the next one starts */
            if (prev_i >= 0) CHECK(prev_i + tr->step[prev_i].hold + 1 <= i);
            prev_deg = deg;
            prev_i = i;
        }
    }
    /* HOLD 0 leaves notes short */
    tr->gen.p[1] = 0;
    gen_roll(&s, 2);
    for (int i = 0; i < tr->len; i++) if (tr->step[i].on) CHECK(tr->step[i].hold == 0);
}

static void test_bell(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    s.rng = 4242;
    dmaj_pent(&s);
    SeqTrack *tr = &s.track[3];
    tr->gen.base = 74; tr->gen.span = 2;
    fresh(&s, 3, GEN_BELL, 27);
    for (int i = 0; i < SEQ_STEPS; i++) tr->step[i].vel = 60;
    for (int r = 0; r < 200; r++) {
        tr->gen.p[0] = (uint8_t)(1 + r % 8);
        tr->gen.p[1] = (uint8_t)(1 + r % 10);
        tr->gen.p[2] = (uint8_t)(r % 2);
        tr->gen.p[3] = (uint8_t)(r % 11);
        for (int i = 0; i < SEQ_STEPS; i++) tr->step[i].vel = 60;
        gen_roll(&s, 3);
        int h = gen_hits(&s, 3);
        CHECK(h >= 1 && h <= tr->gen.p[0]);
        for (int i = 0; i < tr->len; i++) {
            const SeqStep *st = &tr->step[i];
            if (!st->on) continue;
            CHECK(!tr->step[(i + 1) % tr->len].on);              /* never two in a row */
            CHECK(st->prob == tr->gen.p[1] * 10);
            CHECK(st->note >= 74 + 12 * tr->gen.p[2] && seq_in_scale(&s, st->note));
            CHECK(st->vel <= 60 && st->vel >= 60 - tr->gen.p[3] * 4);
        }
    }
}

static void test_chord_notes(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    dmaj_pent(&s);
    uint8_t c[SEQ_CHORD_MAX];
    CHECK(gen_chord_notes(&s, 50, CHORD_OFF, c) == 0);
    CHECK(gen_chord_notes(&s, 50, CHORD_FIFTH, c) == 1 && c[0] == 57);                  /* D3: A3 */
    CHECK(gen_chord_notes(&s, 50, CHORD_TRIAD, c) == 2 && c[0] == 54 && c[1] == 57);    /* F#3 A3 */
    CHECK(gen_chord_notes(&s, 50, CHORD_OPEN, c) == 2 && c[0] == 57 && c[1] == 66);     /* A3 F#4 */
    /* E in D major pentatonic has no third: the nearest scale note stands in */
    int nc = gen_chord_notes(&s, 52, CHORD_TRIAD, c);
    CHECK(nc == 2 && c[1] == 59);                                                       /* B3 is E's fifth */
    /* every chord in every scale: in scale, ascending, above the note, <= 127 */
    for (int sc = 0; sc < scale_count(); sc++) {
        seq_set_scale(&s, sc, 9, 0);
        for (int ch = 0; ch < CHORD_COUNT; ch++) {
            CHECK(gen_chord_name(ch)[0]);
            for (int note = 0; note < 128; note++) {
                if (!seq_in_scale(&s, note)) continue;
                int k = gen_chord_notes(&s, note, ch, c), last = note;
                CHECK(k >= 0 && k <= SEQ_CHORD_MAX);
                if (ch == CHORD_FIFTH && note <= 100) CHECK(k == 1);
                if (ch == CHORD_SEVENTH && note <= 100) CHECK(k == 3);
                for (int j = 0; j < k; j++) {
                    CHECK(c[j] > last && c[j] <= 127 && seq_in_scale(&s, c[j]));
                    last = c[j];
                }
            }
        }
    }
}

/* ---- playback: count notes per channel, and what is held ---- */

typedef struct { int on[16], off[16], held[16][128], max_vel[16], min_vel[16]; int hung; } Log;

static void rec(void *ctx, const uint8_t *m, int n) {
    Log *l = ctx;
    if (n < 3) return;
    int st = m[0] & 0xF0, ch = m[0] & 0x0F;
    if (st == 0x90 && m[2]) {
        l->on[ch]++;
        l->held[ch][m[1]]++;
        if (m[2] > l->max_vel[ch]) l->max_vel[ch] = m[2];
        if (!l->min_vel[ch] || m[2] < l->min_vel[ch]) l->min_vel[ch] = m[2];
    } else if (st == 0x80) {
        l->off[ch]++;
        if (l->held[ch][m[1]] > 0) l->held[ch][m[1]]--;
    } else if (st == 0xB0 && (m[1] == 123 || m[1] == 120)) {
        memset(l->held[ch], 0, sizeof l->held[ch]);
    }
}

static int held_any(const Log *l) {
    for (int c = 0; c < 16; c++) for (int k = 0; k < 128; k++) if (l->held[c][k]) return 1;
    return 0;
}

static void run_bars(Seq *s, int bars) { for (int i = 0; i < bars * SEQ_PPQN * 4; i++) seq_tick(s); }

static void quarter_notes(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    tr->len = 16;
    for (int i = 0; i < SEQ_STEPS; i++) tr->step[i] = (SeqStep){i % 4 == 0, 50, 100, 50, 100, 0, 0, 0};
}

static void test_chord_playback(void) {
    Log l; Seq s;
    memset(&l, 0, sizeof l);
    seq_init(&s, rec, &l);
    dmaj_pent(&s);
    quarter_notes(&s, 1);
    s.track[1].chord = CHORD_TRIAD;
    s.track[1].step[0].hold = 6;   /* overlaps the next note: the retrigger releases the whole chord */
    seq_start(&s);
    run_bars(&s, 4);
    CHECK(l.on[1] == 16 * 3);       /* 4 notes a bar, 3 notes each */
    CHECK(l.held[1][50] <= 1 && l.held[1][54] <= 1 && l.held[1][57] <= 1);
    seq_stop(&s);
    CHECK(!held_any(&l));
    CHECK(l.off[1] >= l.on[1]);
    /* chord notes sit a little under the step's own note */
    CHECK(l.max_vel[1] == 100 && l.min_vel[1] == 80);

    /* panic after a reconnect also silences the chord notes of the last note */
    memset(&l, 0, sizeof l);
    seq_init(&s, rec, &l);
    dmaj_pent(&s);
    quarter_notes(&s, 0);
    s.track[0].chord = CHORD_SEVENTH;
    seq_start(&s);
    run_bars(&s, 1);
    int offs = l.off[0];
    seq_panic(&s);
    CHECK(l.off[0] - offs >= 4);
}

static void test_swell(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    SeqTrack *tr = &s.track[0];
    CHECK(seq_swell_vel(&s, 0, 0, 100) == 100);   /* off */
    tr->swell = 4; tr->swell_depth = 50;
    uint32_t period = 4 * SEQ_PPQN * 4;
    int lo = 1000, hi = 0;
    for (uint32_t now = 0; now < period; now += 24) {
        int v = seq_swell_vel(&s, 0, now, 100);
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    CHECK(lo == 50 && hi == 100);
    CHECK(seq_swell_vel(&s, 0, 0, 100) == 50 && seq_swell_vel(&s, 0, period / 2, 100) == 100);
    CHECK(seq_swell_vel(&s, 0, period, 100) == 50);
    /* tracks are a quarter period apart */
    s.track[1].swell = 4; s.track[1].swell_depth = 50;
    CHECK(seq_swell_vel(&s, 1, 0, 100) == 75);
    /* never below 1 */
    tr->swell_depth = 100;
    CHECK(seq_swell_vel(&s, 0, 0, 1) == 1);

    /* the sent velocities follow it */
    Log l;
    memset(&l, 0, sizeof l);
    seq_init(&s, rec, &l);
    quarter_notes(&s, 0);
    s.track[0].swell = 2; s.track[0].swell_depth = 40;
    seq_start(&s);
    run_bars(&s, 4);
    CHECK(l.max_vel[0] >= 95 && l.min_vel[0] <= 65 && l.min_vel[0] >= 60);
}

static void test_rests(void) {
    Log l; Seq s;
    memset(&l, 0, sizeof l);
    seq_init(&s, rec, &l);
    quarter_notes(&s, 0);
    quarter_notes(&s, 1);
    s.track[0].rest = 100;
    s.track[0].step[12].hold = 10;   /* rings into the rest; still gets its note-off */
    seq_start(&s);
    run_bars(&s, 64);
    seq_stop(&s);
    CHECK(l.on[1] == 64 * 4);        /* other tracks play on */
    /* rest 100%: bar 1 plays, then rests of 1-4 bars each followed by one bar played */
    CHECK(l.on[0] >= 4 * 13 && l.on[0] <= 4 * 32);
    CHECK(!held_any(&l));

    /* bars played between rests are never skipped twice in a row past 4 bars */
    memset(&l, 0, sizeof l);
    seq_init(&s, rec, &l);
    quarter_notes(&s, 0);
    s.track[0].rest = 100;
    seq_start(&s);
    int silent_run = 0, longest = 0;
    for (int bar = 0; bar < 64; bar++) {
        int before = l.on[0];
        run_bars(&s, 1);
        silent_run = l.on[0] == before ? silent_run + 1 : 0;
        longest = silent_run > longest ? silent_run : longest;
    }
    CHECK(longest >= 1 && longest <= 4);

    /* rest 0 changes nothing, and seq_start clears a running rest */
    memset(&l, 0, sizeof l);
    seq_init(&s, rec, &l);
    quarter_notes(&s, 0);
    s.track[0].rest_left = 3;
    seq_start(&s);
    run_bars(&s, 8);
    CHECK(l.on[0] == 32);
}

static void test_slow_evolution(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    dmaj_pent(&s);
    SeqTrack *tr = &s.track[0];
    quarter_notes(&s, 0);
    for (int i = 0; i < SEQ_STEPS; i++) tr->step[i].note = 62;
    tr->evo = (EvolveSettings){64, 100, EVO_PITCH, 0, 0, 0};
    SeqStep before[SEQ_STEPS];
    memcpy(before, tr->step, sizeof before);
    seq_start(&s);
    run_bars(&s, 64);                       /* bars 1..63 have passed: nothing yet */
    CHECK(memcmp(before, tr->step, sizeof before) == 0);
    seq_tick(&s);                           /* the bar-64 line */
    CHECK(memcmp(before, tr->step, sizeof before) != 0);

    /* save and load keep the slow interval and the playback colour */
    tr->evo.every = 32;
    tr->chord = CHORD_OPEN; tr->swell = 16; tr->swell_depth = 35; tr->rest = 20;
    FILE *f = tmpfile();
    seq_save(&s, f);
    rewind(f);
    Seq t;
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.track[0].evo.every == 32);
    CHECK(t.track[0].chord == CHORD_OPEN && t.track[0].swell == 16 && t.track[0].swell_depth == 35 && t.track[0].rest == 20);
    CHECK(t.track[1].chord == 0 && t.track[1].swell == 0 && t.track[1].rest == 0);

    /* an old file (no pc line) loads with the colour off */
    const char *old = "seq4h 1\nbpm 72\nt 0 0 16 0\ne 0 8 15 1\n";
    f = tmpfile();
    fputs(old, f);
    rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.track[0].evo.every == 8 && t.track[0].chord == 0 && t.track[0].swell == 0);
}

/* The ambient style is unchanged: no chords, swells or rests on its tracks. */
static void test_ambient_style_unchanged(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    style_apply(&s, STYLE_AMBIENT, 0);
    CHECK(s.bpm == 72);
    for (int t = 0; t < SEQ_TRACKS; t++)
        CHECK(s.track[t].chord == 0 && s.track[t].swell == 0 && s.track[t].rest == 0);
    CHECK(s.track[0].gen.type == GEN_TURING && s.track[3].gen.type == GEN_WEIGHTED);
}

/* DRONE style: the new generators, chords, swells and rests, slow evolution. */
static void test_drone_style(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    CHECK(strcmp(style_name(STYLE_DRONE), "DRONE") == 0);
    style_apply(&s, STYLE_DRONE, 0);
    CHECK(s.style == STYLE_DRONE && s.bpm == 66);
    CHECK(s.track[0].gen.type == GEN_DRONE && s.track[1].gen.type == GEN_DRONE);
    CHECK(s.track[2].gen.type == GEN_DRIFT && s.track[3].gen.type == GEN_BELL);
    CHECK(s.track[0].chord == CHORD_FIFTH && s.track[1].chord == CHORD_TRIAD);
    CHECK(s.track[0].evo.every == 32 && s.track[1].evo.every == 16);
    CHECK(s.track[0].swell && s.track[3].rest);
    for (int t = 0; t < SEQ_TRACKS; t++) CHECK(gen_hits(&s, t) >= 1);
    /* the bass and the pad start on the root (A) */
    CHECK(pc(&s, s.track[0].step[0].note) == 0 && pc(&s, s.track[1].step[0].note) == 0);
    /* going back to AMBIENT clears the colour again */
    style_apply(&s, STYLE_AMBIENT, 0);
    for (int t = 0; t < SEQ_TRACKS; t++) CHECK(s.track[t].chord == 0 && s.track[t].swell == 0 && s.track[t].rest == 0);
    /* a MANUAL track with notes is kept as it is, colour included */
    style_apply(&s, STYLE_DRONE, 0);
    gen_set_type(&s, 2, GEN_NONE);
    s.track[2].chord = CHORD_SUS4;
    style_apply(&s, STYLE_AMBIENT, 1);
    CHECK(s.track[2].gen.type == GEN_NONE && s.track[2].chord == CHORD_SUS4);

    /* hours of it: no hung notes, every track plays */
    Log l;
    memset(&l, 0, sizeof l);
    seq_init(&s, rec, &l);
    style_apply(&s, STYLE_DRONE, 0);
    seq_start(&s);
    run_bars(&s, 1000);
    seq_stop(&s);
    for (int t = 0; t < SEQ_TRACKS; t++) CHECK(l.on[t] > 0);
    CHECK(!held_any(&l));
}

int main(void) {
    test_drone();
    test_drift();
    test_bell();
    test_chord_notes();
    test_chord_playback();
    test_swell();
    test_rests();
    test_slow_evolution();
    test_ambient_style_unchanged();
    test_drone_style();
    if (fails) { fprintf(stderr, "%d ambient test(s) failed\n", fails); return 1; }
    puts("ambient tests passed");
    return 0;
}
