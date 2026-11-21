/* Track locks, step locks (note and drum tracks) and ALL STEPS. */
#include "../src/drum.h"
#include "../src/engine.h"
#include "../src/gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void nosend(void *ctx, const uint8_t *m, int n) { (void)ctx; (void)m; (void)n; }

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

static void bars(Seq *s, int n) { for (int i = 0; i < n * SEQ_PPQN * 4; i++) seq_tick(s); }

/* A locked track keeps its pattern through roll, evolve, transforms and NEW FROM STYLE. */
static int test_track_lock(void) {
    static Seq s;
    seq_init(&s, nosend, NULL);
    style_apply(&s, STYLE_AMBIENT, 0);
    for (int t = 0; t < SEQ_TRACKS; t++) { s.track[t].evo.every = 1; s.track[t].evo.amount = 100; s.track[t].evo.kinds = EVO_ALL; }
    gen_set_type(&s, 1, GEN_EUCLID);
    SeqTrack *tr = &s.track[1];
    seq_track_lock_toggle(&s, 1);
    CHECK(tr->locked);
    SeqStep before[SEQ_STEPS];
    memcpy(before, tr->step, sizeof before);
    uint8_t len = tr->len;
    for (int k = 0; k < 20; k++) gen_roll(&s, 1);
    CHECK(!memcmp(before, tr->step, sizeof before));
    gen_transform(&s, 1, XF_REVERSE, 1);
    gen_transform(&s, 1, XF_CLEAR, 1);
    CHECK(!memcmp(before, tr->step, sizeof before));
    seq_start(&s);
    bars(&s, 16);
    CHECK(!memcmp(before, tr->step, sizeof before) && tr->len == len);
    style_apply(&s, STYLE_TECHNO, 0);
    CHECK(!memcmp(before, tr->step, sizeof before));
    CHECK(style_apply(&s, STYLE_BLANK, 0) == 1);
    CHECK(!memcmp(before, tr->step, sizeof before) && tr->gen.type == GEN_EUCLID);
    /* the unlocked tracks did change */
    CHECK(s.track[0].gen.type == GEN_NONE);
    /* unlock: rolls work again */
    seq_track_lock_toggle(&s, 1);
    int changed = 0;
    for (int k = 0; k < 20 && !changed; k++) { gen_roll(&s, 1); changed = memcmp(before, tr->step, sizeof before) != 0; }
    CHECK(changed);
    return 0;
}

/* A locked drum track and drum step locks. */
static int test_drum_locks(void) {
    static Seq s;
    seq_init(&s, nosend, NULL);
    s.style = STYLE_TECHNO;
    seq_track_type(&s, 3, TRACK_DRUMS);
    SeqTrack *tr = &s.track[3];
    tr->len = 16;
    gen_drum_roll(&s, 3);
    /* lock steps 0 and 5 with a known content */
    for (int l = 0; l < SEQ_LANES; l++) { tr->drum.vel[l][0] = (uint8_t)(10 + l); tr->drum.prob[l][0] = 77; tr->drum.vel[l][5] = 0; }
    seq_step_lock_toggle(&s, 3, 0);
    seq_step_lock_toggle(&s, 3, 5);
    CHECK(seq_step_locked(&s, 3, 0) && seq_step_locked(&s, 3, 5) && !seq_step_locked(&s, 3, 1));
    CHECK(tr->drum_step_lock == ((1u << 0) | (1u << 5)));
    tr->evo = (EvolveSettings){1, 100, EVO_ALL, 0, 0, 0};
    for (int k = 0; k < 30; k++) {
        gen_drum_roll(&s, 3);
        gen_evolve_bar(&s, 3);
        for (int op = 0; op < XF_COUNT; op++) gen_transform(&s, 3, op, 3);
        SeqDrums full;
        memset(&full, 0, sizeof full);
        for (int l = 0; l < SEQ_LANES; l++) for (int i = 0; i < SEQ_STEPS; i++) { full.vel[l][i] = 99; full.prob[l][i] = 100; }
        gen_drum_morph(&s, 3, &full, 50);
        for (int l = 0; l < SEQ_LANES; l++) {
            CHECK(tr->drum.vel[l][0] == 10 + l && tr->drum.prob[l][0] == 77);
            CHECK(tr->drum.vel[l][5] == 0);
        }
    }
    /* the rest did change */
    CHECK(seq_drum_hits(tr) > 8);
    /* a locked drum track: nothing changes */
    seq_track_lock_toggle(&s, 3);
    SeqDrums d = tr->drum;
    gen_roll(&s, 3);
    gen_evolve_bar(&s, 3);
    gen_transform(&s, 3, XF_CLEAR, 1);
    CHECK(!memcmp(&d, &tr->drum, sizeof d));
    /* saved and loaded */
    FILE *f = tmpfile();
    seq_save(&s, f);
    rewind(f);
    static Seq b;
    seq_init(&b, nosend, NULL);
    CHECK(seq_load(&b, f) == 0);
    fclose(f);
    CHECK(b.track[3].locked && b.track[3].drum_step_lock == ((1u << 0) | (1u << 5)) && !b.track[0].locked && !b.track[2].drum_step_lock);
    return 0;
}

/* Note step locks hold through every generator path. */
static int test_note_step_locks(void) {
    static Seq s;
    seq_init(&s, nosend, NULL);
    gen_set_type(&s, 0, GEN_MARKOV);
    SeqTrack *tr = &s.track[0];
    tr->step[3] = (SeqStep){1, 61, 33, 44, 55, 0, 2, -3, {0}};
    seq_step_lock_toggle(&s, 0, 3);
    CHECK(seq_step_locked(&s, 0, 3));
    SeqStep keep = tr->step[3];
    tr->evo = (EvolveSettings){1, 100, EVO_ALL, 50, 4, 8};
    gen_set_anchor(&s, 0);
    for (int k = 0; k < 30; k++) {
        gen_roll(&s, 0);
        gen_evolve_bar(&s, 0);
        for (int op = 0; op < XF_COUNT; op++) gen_transform(&s, 0, op, 2);
        CHECK(!memcmp(&keep, &tr->step[3], sizeof keep));
    }
    return 0;
}

static int test_all_steps(void) {
    static Seq s;
    seq_init(&s, nosend, NULL);
    SeqTrack *tr = &s.track[2];
    tr->len = 8;
    for (int i = 0; i < SEQ_STEPS; i++) tr->step[i] = (SeqStep){0, 60, 100, 50, 100, 0, 0, 0, {0}};
    tr->step[1].on = tr->step[4].on = 1;
    tr->step[1].vel = 60; tr->step[4].vel = 90;
    int lo, hi, avg;
    CHECK(seq_all_range(&s, 2, ALL_VEL, &lo, &hi, &avg) == 2 && lo == 60 && hi == 90 && avg == 75);
    seq_all_adjust(&s, 2, ALL_VEL, 10);
    CHECK(tr->step[1].vel == 70 && tr->step[4].vel == 100 && tr->step[0].vel == 110);
    CHECK(tr->step[8].vel == 100);   /* beyond the length: untouched */
    seq_all_adjust(&s, 2, ALL_VEL, 100);
    CHECK(tr->step[1].vel == 127 && tr->step[4].vel == 127);
    seq_all_set(&s, 2, ALL_GATE, 30);
    CHECK(tr->step[1].gate == 30 && tr->step[7].gate == 30 && tr->step[8].gate == 50);
    seq_all_adjust(&s, 2, ALL_GATE, -100);
    CHECK(tr->step[1].gate == 5);
    seq_all_adjust(&s, 2, ALL_NUDGE, -20);
    CHECK(tr->step[1].nudge == -SEQ_MAX_NUDGE);
    seq_all_adjust(&s, 2, ALL_HOLD, 3);
    CHECK(tr->step[4].hold == 3);
    seq_all_adjust(&s, 2, ALL_PROB, -30);
    CHECK(tr->step[4].prob == 70);
    tr->step[1].prob = 20;
    CHECK(seq_all_range(&s, 2, ALL_PROB, &lo, &hi, &avg) == 2 && avg == 45);
    /* locks: lock every step, then clear */
    tr->step[2].lock = 1;
    CHECK(seq_locked_steps(&s, 2) == 1);
    CHECK(seq_all_lock_toggle(&s, 2) == 0 && seq_locked_steps(&s, 2) == 0);
    CHECK(seq_all_lock_toggle(&s, 2) == 1 && seq_locked_steps(&s, 2) == 8 && !tr->step[8].lock);
    /* no notes: range is empty */
    tr->step[1].on = tr->step[4].on = 0;
    CHECK(seq_all_range(&s, 2, ALL_VEL, &lo, &hi, &avg) == 0);

    /* drums: velocity and probability of every hit, gate does nothing */
    seq_track_type(&s, 3, TRACK_DRUMS);
    SeqTrack *dt = &s.track[3];
    dt->len = 16;
    seq_drum_toggle(&s, 3, 0, 0);
    seq_drum_toggle(&s, 3, 2, 4);
    seq_drum_adjust(&s, 3, 2, 4, -50, -40);
    CHECK(seq_all_range(&s, 3, ALL_VEL, &lo, &hi, &avg) == 2 && lo == 50 && hi == 100 && avg == 75);
    seq_all_adjust(&s, 3, ALL_VEL, -60);
    CHECK(dt->drum.vel[0][0] == 40 && dt->drum.vel[2][4] == 1 && dt->drum.vel[1][1] == 0);   /* a hit never goes */
    seq_all_set(&s, 3, ALL_PROB, 80);
    CHECK(dt->drum.prob[0][0] == 80 && dt->drum.prob[2][4] == 80);
    CHECK(seq_all_range(&s, 3, ALL_GATE, &lo, &hi, &avg) == 0);
    CHECK(seq_all_lock_toggle(&s, 3) == 1 && dt->drum_step_lock == 0xFFFFu);
    CHECK(seq_all_lock_toggle(&s, 3) == 0 && dt->drum_step_lock == 0);
    return 0;
}

int main(void) {
    if (test_track_lock() || test_drum_locks() || test_note_step_locks() || test_all_steps()) return 1;
    printf("lock tests passed\n");
    return 0;
}
