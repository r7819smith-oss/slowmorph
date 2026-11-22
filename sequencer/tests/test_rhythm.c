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

static int all_down(const Cap *c) {
    int n = 0;
    for (int ch = 0; ch < 16; ch++) for (int k = 0; k < 128; k++) n += c->down[ch][k];
    return n;
}

static void test_cond_names(void) {
    char b[12];
    int a, bb;
    CHECK(seq_cond_make(1, 2) == COND_AB_FIRST);
    CHECK(seq_cond_make(8, 8) == COND_AB_LAST);
    for (int B = 2; B <= 8; B++)
        for (int A = 1; A <= B; A++) {
            int c = seq_cond_make(A, B);
            CHECK(seq_cond_ab(c, &a, &bb) && a == A && bb == B);
        }
    seq_cond_name(seq_cond_make(3, 4), b); CHECK(strcmp(b, "3:4") == 0);
    seq_cond_name(COND_ALWAYS, b); CHECK(strcmp(b, "ALWAYS") == 0);
    seq_cond_name(COND_NOT_PRE, b); CHECK(strcmp(b, "NOT PRE") == 0);
    CHECK(!seq_cond_ab(COND_FIRST, &a, &bb));
    CHECK(seq_cond_make(3, 2) == COND_ALWAYS);
}

static void test_ratchets(void) {
    Seq s; Cap c;
    /* x3 on a 1/16: notes on ticks 0, 8, 16, each 50% of 8 ticks long */
    setup(&s, &c);
    SeqTrack *tr = &s.track[0];
    tr->len = 4;
    tr->step[0].on = 1; tr->step[0].note = 60; tr->step[0].ratchet = 3;
    seq_start(&s);
    ticks(&s, 3);
    CHECK(c.n == 1 && c.down[0][60] == 1);
    ticks(&s, 2);
    CHECK(c.down[0][60] == 0);   /* 50% of the repeat's 8 ticks */
    ticks(&s, 20);
    CHECK(c.n == 3);
    CHECK(c.on[0].tick == 0 && c.on[1].tick == 8 && c.on[2].tick == 16);
    ticks(&s, 71);   /* rest of the bar: nothing more */
    CHECK(c.n == 3);
    CHECK(all_down(&c) == 0);
    ticks(&s, 1);    /* step 1 again */
    CHECK(c.n == 4);
    seq_stop(&s);

    /* at 1/8 the repeats spread over the longer step; the last one keeps the hold */
    setup(&s, &c);
    tr = &s.track[0];
    tr->len = 2; tr->rate = SEQ_RATE_8;
    tr->step[0].on = 1; tr->step[0].note = 62; tr->step[0].ratchet = 2; tr->step[0].hold = 1; tr->step[0].gate = 50;
    seq_start(&s);
    ticks(&s, 24 + 1);
    CHECK(c.n == 2 && c.on[1].tick == 24);
    ticks(&s, 12 + 48 - 2);
    CHECK(c.down[0][62] == 1);   /* 12 (half of 24) + 48 (hold) after tick 24 */
    ticks(&s, 2);
    CHECK(c.down[0][62] == 0);
    seq_stop(&s);

    /* chance is decided once: 0% plays no repeat either */
    setup(&s, &c);
    tr = &s.track[0];
    tr->step[0].on = 1; tr->step[0].ratchet = 4; tr->step[0].prob = 0;
    seq_start(&s);
    ticks(&s, SEQ_PPQN * 4);
    CHECK(c.n == 0);
    seq_stop(&s);

    /* POLY: each repeat plays the whole chord */
    setup(&s, &c);
    tr = &s.track[0];
    seq_track_poly(&s, 0, 1);
    tr->step[0].on = 1; tr->step[0].note = 60; tr->step[0].more[0] = 2; tr->step[0].ratchet = 2;
    seq_start(&s);
    ticks(&s, 24);
    CHECK(c.n == 4);
    ticks(&s, 24);
    CHECK(all_down(&c) == 0);
    seq_stop(&s);

    /* stop in the middle of a ratchet: nothing hangs, nothing plays later */
    setup(&s, &c);
    tr = &s.track[0];
    tr->step[0].on = 1; tr->step[0].ratchet = 8;
    seq_start(&s);
    ticks(&s, 7);
    seq_stop(&s);
    int n = c.n;
    ticks(&s, 50);
    CHECK(c.n == n && all_down(&c) == 0);
    /* a mute does the same */
    seq_start(&s);
    ticks(&s, 4);
    seq_track_mute_toggle(&s, 0);
    n = c.n;
    ticks(&s, 30);
    CHECK(c.n == n);
    seq_stop(&s);

    /* drum hits ratchet too, each lane on its own */
    setup(&s, &c);
    seq_track_type(&s, 1, TRACK_DRUMS);
    tr = &s.track[1];
    tr->drum.vel[2][0] = 90; tr->drum.rat[2][0] = 4;
    tr->drum.vel[0][0] = 100;
    seq_start(&s);
    ticks(&s, 24);
    CHECK(ons_on(&c, 1) == 5);
    for (int k = 0; k < c.n; k++) if (c.on[k].note == s.lane_note[2]) CHECK(c.on[k].tick % 6 == 0);
    ticks(&s, 24);
    CHECK(all_down(&c) == 0);
    seq_stop(&s);

    /* setters */
    setup(&s, &c);
    seq_step_ratchet(&s, 0, 3, 1);
    CHECK(s.track[0].step[3].ratchet == 2);
    seq_step_ratchet(&s, 0, 3, 20);
    CHECK(s.track[0].step[3].ratchet == SEQ_MAX_RATCHET);
    seq_step_ratchet(&s, 0, 3, -20);
    CHECK(s.track[0].step[3].ratchet == 0);
    seq_step_cond(&s, 0, 3, 1);
    CHECK(s.track[0].step[3].cond == COND_AB_FIRST);
    seq_step_cond(&s, 0, 3, -5);
    CHECK(s.track[0].step[3].cond == COND_ALWAYS);
    seq_drum_ratchet(&s, 0, 0, 0, 1);   /* no hit: nothing */
    CHECK(s.track[0].drum.rat[0][0] == 0);
}

/* Which passes (of 8) a one-step, one-step-long track plays with condition c. */
static int passes(int cond) {
    Seq s; Cap c;
    setup(&s, &c);
    SeqTrack *tr = &s.track[0];
    tr->len = 1;
    tr->step[0].on = 1;
    tr->step[0].cond = (uint8_t)cond;
    seq_start(&s);
    ticks(&s, 24 * 8);
    int bits = 0;
    for (int k = 0; k < c.n; k++) bits |= 1 << (c.on[k].tick / 24);
    return bits;
}

static void test_conditions(void) {
    CHECK(passes(COND_ALWAYS) == 0xFF);
    CHECK(passes(seq_cond_make(1, 2)) == 0x55);
    CHECK(passes(seq_cond_make(2, 2)) == 0xAA);
    CHECK(passes(seq_cond_make(1, 4)) == 0x11);
    CHECK(passes(seq_cond_make(3, 4)) == 0x44);
    CHECK(passes(seq_cond_make(1, 8)) == 0x01);
    CHECK(passes(COND_FIRST) == 0x01);
    CHECK(passes(COND_NOT_FIRST) == 0xFE);

    /* passes count the track's own pattern: a 4-step track's 1:2 step plays bars 1, 3 ... of its own */
    Seq s; Cap c;
    setup(&s, &c);
    SeqTrack *tr = &s.track[0];
    tr->len = 4;
    tr->step[2].on = 1; tr->step[2].cond = (uint8_t)seq_cond_make(2, 2);
    seq_start(&s);
    ticks(&s, 24 * 16);
    CHECK(c.n == 2 && c.on[0].tick == 24 * 6 && c.on[1].tick == 24 * 14);
    seq_stop(&s);

    /* PRE / NOT PRE follow the last A:B step: 1:2 on step 1, PRE on step 2, NOT PRE on step 3 */
    setup(&s, &c);
    tr = &s.track[0];
    tr->len = 3;
    for (int i = 0; i < 3; i++) { tr->step[i].on = 1; tr->step[i].note = (uint8_t)(60 + i); }
    tr->step[0].cond = (uint8_t)seq_cond_make(1, 2);
    tr->step[1].cond = COND_PRE;
    tr->step[2].cond = COND_NOT_PRE;
    seq_start(&s);
    ticks(&s, 24 * 12);   /* 4 passes */
    char got[16] = "";
    for (int k = 0; k < c.n && k < 15; k++) got[k] = (char)('a' + c.on[k].note - 60);
    /* pass 1: a, b (PRE: a played); pass 2: c (a didn't); pass 3: a, b; pass 4: c */
    CHECK(strcmp(got, "abcabc") == 0);
    CHECK(c.n == 6 && c.on[2].tick == 24 * 5 && c.on[5].tick == 24 * 11);
    seq_stop(&s);

    /* drum hits have their own conditions */
    setup(&s, &c);
    seq_track_type(&s, 1, TRACK_DRUMS);
    tr = &s.track[1];
    tr->len = 1;
    tr->drum.vel[0][0] = 100;
    tr->drum.vel[1][0] = 100; tr->drum.cond[1][0] = (uint8_t)seq_cond_make(2, 2);
    seq_start(&s);
    ticks(&s, 24 * 4);
    int kick = 0, snare = 0;
    for (int k = 0; k < c.n; k++) { kick += c.on[k].note == s.lane_note[0]; snare += c.on[k].note == s.lane_note[1]; }
    CHECK(kick == 4 && snare == 2);
    seq_stop(&s);
}

static void test_ratchet_tools(void) {
    Seq s; Cap c;
    setup(&s, &c);
    SeqTrack *tr = &s.track[0];
    tr->len = 16;
    for (int i = 0; i < 16; i += 2) tr->step[i].on = 1;
    tr->step[4].lock = 1;
    gen_transform(&s, 0, XF_RATCHET, 3);
    int n = 0;
    for (int i = 0; i < 16; i++) if (tr->step[i].ratchet) { n++; CHECK(tr->step[i].on && tr->step[i].ratchet >= 2 && tr->step[i].ratchet <= 4); }
    CHECK(n == 3);
    CHECK(tr->step[4].ratchet == 0);
    gen_transform(&s, 0, XF_RATCHET, -1);
    n = 0;
    for (int i = 0; i < 16; i++) n += tr->step[i].ratchet != 0;
    CHECK(n == 2);
    gen_transform(&s, 0, XF_RATCHET, 0);
    for (int i = 0; i < 16; i++) CHECK(tr->step[i].ratchet == 0);
    gen_transform(&s, 0, XF_VARY, 20);   /* every unlocked note, no more */
    n = 0;
    for (int i = 0; i < 16; i++) if (tr->step[i].cond) { n++; CHECK(tr->step[i].cond != COND_PRE && tr->step[i].cond != COND_NOT_PRE); }
    CHECK(n == 7);
    gen_transform(&s, 0, XF_VARY, 0);
    for (int i = 0; i < 16; i++) CHECK(tr->step[i].cond == 0);
    CHECK(strcmp(gen_xform_name(XF_VARY), "VARY") == 0);

    /* EVO TRIG: ratchets and conditions come and go over bars, locked steps never change */
    tr->step[4].ratchet = 3;
    tr->evo = (EvolveSettings){.every = 1, .amount = 100, .kinds = EVO_TRIG};
    int seen_r = 0, seen_c = 0;
    for (int b = 0; b < 40; b++) {
        gen_evolve_bar(&s, 0);
        for (int i = 0; i < 16; i++) { seen_r |= i != 4 && tr->step[i].ratchet; seen_c |= tr->step[i].cond != 0; }
        CHECK(tr->step[4].ratchet == 3 && tr->step[4].cond == 0);
    }
    CHECK(seen_r && seen_c);
    for (int i = 0; i < 16; i++) CHECK(tr->step[i].on == (i % 2 == 0));   /* the rhythm itself stays */

    /* drums */
    seq_track_type(&s, 1, TRACK_DRUMS);
    SeqTrack *d = &s.track[1];
    for (int i = 0; i < 16; i += 4) d->drum.vel[2][i] = 80;
    gen_transform(&s, 1, XF_RATCHET, 2);
    n = 0;
    for (int i = 0; i < 16; i++) n += d->drum.rat[2][i] != 0;
    CHECK(n == 2);
    /* a ratchet moves with its hit */
    int at = -1;
    for (int i = 0; i < 16; i++) if (d->drum.rat[2][i]) { at = i; break; }
    int rv = d->drum.rat[2][at];
    gen_transform(&s, 1, XF_SHIFT, 1);
    CHECK(d->drum.rat[2][(at + 1) % 16] == rv && d->drum.vel[2][(at + 1) % 16]);
}

static void test_trig_save(void) {
    Seq s, t; Cap c, c2;
    setup(&s, &c);
    s.track[0].step[5].on = 1;
    s.track[0].step[5].ratchet = 3;
    s.track[0].step[5].cond = (uint8_t)seq_cond_make(2, 4);
    s.track[0].step[6].cond = COND_NOT_PRE;
    gen_set_anchor(&s, 0);
    seq_slot_store(&s, 0, 2);
    seq_track_type(&s, 3, TRACK_DRUMS);
    s.track[3].drum.vel[4][7] = 70;
    s.track[3].drum.rat[4][7] = 2;
    s.track[3].drum.cond[4][7] = COND_FIRST;
    gen_set_anchor(&s, 3);
    seq_slot_store(&s, 3, 1);
    FILE *f = tmpfile();
    seq_save(&s, f);
    rewind(f);
    setup(&t, &c2);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.track[0].step[5].ratchet == 3 && t.track[0].step[5].cond == seq_cond_make(2, 4));
    CHECK(t.track[0].step[6].cond == COND_NOT_PRE && t.track[0].step[6].ratchet == 0);
    CHECK(t.track[0].anchor[5].ratchet == 3 && t.track[0].slot[2].step[5].cond == seq_cond_make(2, 4));
    CHECK(t.track[3].drum.rat[4][7] == 2 && t.track[3].drum.cond[4][7] == COND_FIRST);
    CHECK(t.track[3].drum_anchor.rat[4][7] == 2 && t.track[3].slot[1].drum.cond[4][7] == COND_FIRST);
}

int main(void) {
    test_rates();
    test_polymeter_and_realign();
    test_euclid_lanes();
    test_euclid_transform();
    test_save();
    test_cond_names();
    test_ratchets();
    test_conditions();
    test_ratchet_tools();
    test_trig_save();
    if (fails) { fprintf(stderr, "%d rhythm test(s) failed\n", fails); return 1; }
    printf("rhythm tests passed\n");
    return 0;
}
