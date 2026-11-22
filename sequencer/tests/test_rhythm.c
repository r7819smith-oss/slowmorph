/* Tests for polymeter and rhythm tools: STEP RATE per track, REALIGN,
 * Euclidean drum lanes and the EUCLID transform, plus saving them.
 * Pure engine code: no SDL, no MIDI, no hardware. */
#include "../src/engine.h"
#include "../src/gen.h"
#include "../src/drum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* Every note-on: the tick it was sent on, its channel, note and velocity. */
typedef struct { uint32_t tick; int ch, note, vel; } On;
typedef struct {
    const Seq *s;
    On on[4096];
    int n, offs, down[16][128];
} Cap;

static void cap_send(void *ctx, const uint8_t *m, int len) {
    Cap *c = ctx;
    if (len != 3) return;
    int ch = m[0] & 15, st = m[0] & 0xF0;
    if (st == 0x90 && m[2]) {
        if (c->n < 4096) c->on[c->n++] = (On){c->s->tick, ch, m[1], m[2]};
        c->down[ch][m[1]] = 1;
    } else if (st == 0x80 || (st == 0x90 && !m[2])) { c->down[ch][m[1]] = 0; c->offs++; }
    else if (st == 0xB0 && (m[1] == 123 || m[1] == 120)) memset(c->down[ch], 0, sizeof c->down[ch]);
}

static int ons_on(const Cap *c, int ch) {
    int n = 0;
    for (int k = 0; k < c->n; k++) n += c->on[k].ch == ch;
    return n;
}

static void ticks(Seq *s, int n) { for (int i = 0; i < n; i++) seq_tick(s); }

/* All tracks empty, track 0 on channel 0 with every one of its steps given. */
static void setup(Seq *s, Cap *c) {
    memset(c, 0, sizeof *c);
    c->s = s;
    seq_init(s, cap_send, c);
    s->clock_out = s->transport_out = 0;
    for (int t = 0; t < SEQ_TRACKS; t++)
        for (int i = 0; i < SEQ_STEPS; i++) s->track[t].step[i].on = 0;
}

static void test_rates(void) {
    Seq s; Cap c;
    CHECK(seq_rate_ticks(SEQ_RATE_16) == SEQ_TICKS_PER_STEP);
    CHECK(seq_rate_ticks(SEQ_RATE_8) == 48);
    CHECK(seq_rate_ticks(SEQ_RATE_16T) == 16);
    CHECK(seq_rate_ticks(SEQ_RATE_BAR) == SEQ_PPQN * 4);
    CHECK(strcmp(seq_rate_name(SEQ_RATE_8T), "1/8T") == 0);

    /* every step on, 4 steps, at 1/8: a note every 48 ticks */
    setup(&s, &c);
    SeqTrack *tr = &s.track[0];
    tr->len = 4;
    for (int i = 0; i < 4; i++) { tr->step[i].on = 1; tr->step[i].note = (uint8_t)(60 + i); }
    CHECK(tr->rate == SEQ_RATE_16);
    seq_track_rate(&s, 0, 2);
    CHECK(tr->rate == SEQ_RATE_8);
    seq_start(&s);
    ticks(&s, 48 * 8);
    CHECK(ons_on(&c, 0) == 8);
    for (int k = 0; k < c.n; k++) {
        CHECK(c.on[k].tick == (uint32_t)(48 * k));
        CHECK(c.on[k].note == 60 + k % 4);
    }
    /* gate follows the rate: 50% of 48 ticks */
    CHECK(seq_playhead(&s, 0) == 3);

    /* 1/16 triplets: three steps a beat, not swung */
    setup(&s, &c);
    tr = &s.track[0];
    tr->len = 3;
    for (int i = 0; i < 3; i++) tr->step[i].on = 1;
    tr->rate = SEQ_RATE_16T;
    s.swing = 8;
    seq_start(&s);
    ticks(&s, SEQ_PPQN * 2);
    CHECK(ons_on(&c, 0) == 12);
    for (int k = 0; k < c.n; k++) CHECK(c.on[k].tick == (uint32_t)(16 * k));

    /* swing scales with 1/8 steps: odd 1/8s late by twice the swing */
    setup(&s, &c);
    tr = &s.track[0];
    tr->len = 2;
    tr->step[0].on = tr->step[1].on = 1;
    tr->rate = SEQ_RATE_8;
    s.swing = 4;
    seq_start(&s);
    ticks(&s, 200);
    CHECK(c.n >= 3);
    CHECK(c.on[0].tick == 0 && c.on[1].tick == 48 + 8 && c.on[2].tick == 96);

    /* 1/32 with a big offset: still plays, inside its own step */
    setup(&s, &c);
    tr = &s.track[0];
    tr->len = 2;
    tr->step[0].on = tr->step[1].on = 1;
    tr->step[1].nudge = SEQ_MAX_NUDGE;
    tr->rate = SEQ_RATE_32;
    seq_start(&s);
    ticks(&s, 12 * 8);
    CHECK(ons_on(&c, 0) == 8);
    for (int k = 0; k < c.n; k++) CHECK(c.on[k].tick % 12 == (k & 1 ? 11u : 0u));

    /* a held note at 1/4 rings for its hold in quarter notes */
    setup(&s, &c);
    tr = &s.track[0];
    tr->len = 4;
    tr->step[0].on = 1; tr->step[0].hold = 1; tr->step[0].gate = 50;
    tr->rate = SEQ_RATE_4;
    seq_start(&s);
    ticks(&s, 96 + 47);
    CHECK(c.down[0][tr->step[0].note] == 1);
    ticks(&s, 2);
    CHECK(c.down[0][tr->step[0].note] == 0);
    seq_stop(&s);
}

static void test_polymeter_and_realign(void) {
    Seq s; Cap c;
    setup(&s, &c);
    /* track 0: 3 steps at 1/16, step 1 only; track 1: 5 steps at 1/16, step 1 only */
    s.track[0].len = 3; s.track[0].step[0].on = 1;
    s.track[1].len = 5; s.track[1].step[0].on = 1;
    seq_start(&s);
    ticks(&s, SEQ_PPQN * 4 * 4);   /* 4 bars = 64 steps */
    CHECK(ons_on(&c, 0) == 22);    /* steps 0,3,...,63 */
    CHECK(ons_on(&c, 1) == 13);    /* steps 0,5,...,60 */
    seq_stop(&s);

    /* REALIGN every bar: both tracks start their step 1 on each bar line */
    setup(&s, &c);
    s.track[0].len = 3; s.track[0].step[0].on = 1;
    s.track[1].len = 5; s.track[1].step[0].on = 1;
    s.track[2].len = 16; s.track[2].step[0].on = 1;
    seq_adjust_realign(&s, 1);
    CHECK(s.realign == 1);
    seq_start(&s);
    ticks(&s, SEQ_PPQN * 4 * 4);
    int bar_ons[3] = {0};
    for (int k = 0; k < c.n; k++)
        if (c.on[k].tick % (SEQ_PPQN * 4) == 0) bar_ons[c.on[k].ch]++;
    CHECK(bar_ons[0] == 4 && bar_ons[1] == 4 && bar_ons[2] == 4);   /* every bar line, once each */
    CHECK(ons_on(&c, 0) == 4 * 6);   /* 0,3,6,9,12,15 then realign */
    CHECK(ons_on(&c, 1) == 4 * 4);   /* 0,5,10,15 */
    CHECK(ons_on(&c, 2) == 4);       /* a 16-step track never needs it */
    for (int k = 0; k + 1 < c.n; k++)   /* no note twice on one tick */
        for (int j = k + 1; j < c.n && c.on[j].tick == c.on[k].tick; j++) CHECK(c.on[j].ch != c.on[k].ch);
    seq_stop(&s);

    /* REALIGN NOW while playing, and the setting's steps */
    setup(&s, &c);
    s.track[0].len = 5; s.track[0].step[0].on = 1;
    seq_start(&s);
    ticks(&s, 24 * 7);
    CHECK(seq_playhead(&s, 0) == 1);
    seq_realign_now(&s);
    ticks(&s, 1);
    CHECK(seq_playhead(&s, 0) == 0);
    CHECK(c.on[c.n - 1].tick == 24 * 7);
    for (int k = 0; k < 9; k++) seq_adjust_realign(&s, 1);
    CHECK(s.realign == 64);
    seq_adjust_realign(&s, -1);
    CHECK(s.realign == 32);
    seq_stop(&s);
}

static void test_euclid_lanes(void) {
    Seq s; Cap c;
    setup(&s, &c);
    seq_track_type(&s, 3, TRACK_DRUMS);
    SeqTrack *tr = &s.track[3];
    tr->len = 16;
    tr->euc_hits[0] = 5;   /* kick: 5 of 16 */
    tr->euc_rot[0] = 0;
    gen_drum_euclid_lane(&s, 3, 0);
    int n = 0;
    for (int i = 0; i < 16; i++) n += tr->drum.vel[0][i] != 0;
    CHECK(n == 5);
    CHECK(tr->drum.vel[0][0] != 0);
    /* evenly spread: gaps of 3 or 4 */
    int last = -1;
    for (int i = 0; i < 16; i++) if (tr->drum.vel[0][i]) { if (last >= 0) CHECK(i - last == 3 || i - last == 4); last = i; }
    /* rotation moves it */
    tr->euc_rot[0] = 2;
    gen_drum_euclid_lane(&s, 3, 0);
    CHECK(tr->drum.vel[0][0] == 0 && tr->drum.vel[0][2] != 0);
    /* rolls keep a Euclidean lane Euclidean */
    for (int r = 0; r < 5; r++) {
        gen_drum_roll(&s, 3);
        n = 0;
        for (int i = 0; i < 16; i++) n += tr->drum.vel[0][i] != 0;
        CHECK(n == 5);
        CHECK(tr->drum.vel[0][2] != 0);
    }
    /* a locked lane is left alone */
    uint8_t before[SEQ_STEPS];
    memcpy(before, tr->drum.vel[0], sizeof before);
    seq_drum_lock(&s, 3, 0);
    tr->euc_rot[0] = 5;
    gen_drum_euclid_lane(&s, 3, 0);
    CHECK(memcmp(before, tr->drum.vel[0], sizeof before) == 0);
    seq_drum_lock(&s, 3, 0);
    /* step locks hold their hit */
    tr->drum.vel[0][1] = 77;
    seq_step_lock_toggle(&s, 3, 1);
    gen_drum_euclid_lane(&s, 3, 0);
    CHECK(tr->drum.vel[0][1] == 77);
    /* more hits than steps: every step */
    tr->drum_step_lock = 0;
    tr->len = 4;
    tr->euc_hits[0] = 9;
    tr->euc_rot[0] = 0;
    gen_drum_euclid_lane(&s, 3, 0);
    for (int i = 0; i < 4; i++) CHECK(tr->drum.vel[0][i] != 0);
    for (int i = 4; i < SEQ_STEPS; i++) CHECK(tr->drum.vel[0][i] == 0);
}

static void test_euclid_transform(void) {
    Seq s; Cap c;
    setup(&s, &c);
    SeqTrack *tr = &s.track[0];
    tr->len = 8;
    /* four notes bunched at the start: 60 62 64 65 */
    for (int i = 0; i < 4; i++) { tr->step[i].on = 1; tr->step[i].note = (uint8_t)(60 + i); }
    gen_transform(&s, 0, XF_EUCLID, 0);
    for (int i = 0; i < 8; i++) CHECK(tr->step[i].on == (i % 2 == 0));
    CHECK(tr->step[0].note == 60 && tr->step[2].note == 61 && tr->step[4].note == 62 && tr->step[6].note == 63);
    gen_transform(&s, 0, XF_EUCLID, 1);   /* turned one step */
    for (int i = 0; i < 8; i++) CHECK(tr->step[i].on == (i % 2 == 1));
    CHECK(tr->step[1].note == 60);
    /* locked steps stay where they are */
    tr->step[1].lock = 1;
    gen_transform(&s, 0, XF_EUCLID, 0);
    CHECK(tr->step[1].on && tr->step[1].note == 60);
    CHECK(strcmp(gen_xform_name(XF_EUCLID), "EUCLID") == 0);

    /* drums: each lane keeps its count */
    seq_track_type(&s, 1, TRACK_DRUMS);
    SeqTrack *d = &s.track[1];
    d->len = 12;
    for (int i = 0; i < 3; i++) d->drum.vel[2][i] = (uint8_t)(90 + i);
    gen_transform(&s, 1, XF_EUCLID, 0);
    CHECK(d->drum.vel[2][0] == 90 && d->drum.vel[2][4] == 91 && d->drum.vel[2][8] == 92);
    int n = 0;
    for (int i = 0; i < 12; i++) n += d->drum.vel[2][i] != 0;
    CHECK(n == 3);
}

static void test_save(void) {
    Seq s, t; Cap c, c2;
    setup(&s, &c);
    s.track[1].rate = SEQ_RATE_8T;
    s.track[3].euc_hits[2] = 7;
    s.track[3].euc_rot[2] = 3;
    s.realign = 16;
    FILE *f = tmpfile();
    seq_save(&s, f);
    rewind(f);
    setup(&t, &c2);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.track[1].rate == SEQ_RATE_8T);
    CHECK(t.track[0].rate == SEQ_RATE_16);
    CHECK(t.track[3].euc_hits[2] == 7 && t.track[3].euc_rot[2] == 3);
    CHECK(t.realign == 16);

    /* an old file (no rate lines) loads at 1/16 */
    f = tmpfile();
    fputs("seq4h 1\nbpm 90\nt 0 0 16 0\n", f);
    rewind(f);
    setup(&t, &c2);
    t.track[0].rate = SEQ_RATE_4;
    seq_init(&t, cap_send, &c2);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.track[0].rate == SEQ_RATE_16 && t.realign == 0);
}

int main(void) {
    test_rates();
    test_polymeter_and_realign();
    test_euclid_lanes();
    test_euclid_transform();
    test_save();
    if (fails) { fprintf(stderr, "%d rhythm test(s) failed\n", fails); return 1; }
    printf("rhythm tests passed\n");
    return 0;
}
