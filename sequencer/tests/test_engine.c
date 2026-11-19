#include "../src/engine.h"
#include "../src/gen.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { int on[SEQ_TRACKS]; int off[SEQ_TRACKS]; uint32_t first_on[SEQ_TRACKS];
                 uint32_t first_off[SEQ_TRACKS]; int clocks, starts, stops, cc123, cc120; uint32_t *now; } Log;

static void rec(void *ctx, const uint8_t *m, int n) {
    Log *l = ctx; (void)n;
    uint8_t st = m[0] & 0xF0, ch = m[0] & 0x0F;
    if (m[0] == 0xF8) l->clocks++;
    else if (m[0] == 0xFA) l->starts++;
    else if (m[0] == 0xFC) l->stops++;
    else if (st == 0x90) { if (!l->on[ch]++) l->first_on[ch] = *l->now; }
    else if (st == 0x80) { if (!l->off[ch]++) l->first_off[ch] = *l->now; }
    else if (st == 0xB0 && m[1] == 123) l->cc123++;
    else if (st == 0xB0 && m[1] == 120) l->cc120++;
}

static void run(Seq *s, int ticks) { for (int i = 0; i < ticks; i++) seq_tick(s); }

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main(void) {
    Log l; Seq s;

    /* four on the floor: 4 notes per bar (384 ticks), clock = 24/beat */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    seq_load_demo(&s);
    seq_start(&s); run(&s, 384);
    CHECK(l.starts == 1);
    CHECK(l.on[0] == 4);
    CHECK(l.clocks == 96);          /* 4 beats * 24 */
    CHECK(l.on[1] == 4 && l.on[2] == 5);

    /* clock master: clock keeps running while stopped, with no notes; none with clock off */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    seq_load_demo(&s);
    run(&s, 96);
    CHECK(l.clocks == 24 && l.on[0] == 0 && l.starts == 0);
    seq_start(&s); run(&s, 96); seq_stop(&s); run(&s, 96);
    CHECK(l.starts == 1 && l.stops == 1 && l.clocks == 72);
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    s.clock_out = 0;
    run(&s, 96); seq_start(&s); run(&s, 96);
    CHECK(l.clocks == 0 && l.starts == 0);

    /* reconnect: panic now (note-off for the lost note, CC120 + CC123 per
     * channel), keep the position, re-send Start at the next bar line */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    seq_load_demo(&s);
    seq_start(&s); run(&s, 100);
    {
        int offs = l.off[0] + l.off[1] + l.off[2] + l.off[3];
        seq_reconnected(&s);
        CHECK(l.off[0] + l.off[1] + l.off[2] + l.off[3] > offs);
        CHECK(l.cc120 == 4 && l.cc123 == 4);
        CHECK(s.playing && s.tick == 100 && l.starts == 1);
        run(&s, 383 - 100);
        CHECK(l.starts == 1);
        run(&s, 2);
        CHECK(l.starts == 2 && l.stops == 1 && s.resync == 0);
    }
    /* reconnect while stopped: panic only, no Start */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    seq_reconnected(&s); run(&s, 400);
    CHECK(l.cc120 == 4 && l.starts == 0 && s.resync == 0);

    /* gate: 50% of 24 ticks => note off 12 ticks after on */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    s.track[0].step[0].on = 1;
    seq_start(&s); run(&s, 30);
    CHECK(l.first_on[0] == 0 && l.first_off[0] == 12);

    /* swing delays odd steps */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    s.track[0].step[1].on = 1; s.swing = 6;
    seq_start(&s); run(&s, 48);
    CHECK(l.first_on[0] == 24 + 6);

    /* swing follows the global grid: a length-3 track's step 0 lands on the
     * swung off-beat at global step 3 (tick 72 + 6) */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    s.track[0].len = 3; s.track[0].step[0].on = 1; s.swing = 6;
    seq_start(&s); run(&s, 24 * 3 + 1);
    CHECK(l.on[0] == 1);
    run(&s, 6);
    CHECK(l.on[0] == 2);

    /* probability 0 never plays */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    for (int i = 0; i < 16; i++) { s.track[0].step[i].on = 1; s.track[0].step[i].prob = 0; }
    seq_start(&s); run(&s, 384 * 4);
    CHECK(l.on[0] == 0);

    /* probability 50 plays roughly half */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    for (int i = 0; i < 16; i++) { s.track[0].step[i].on = 1; s.track[0].step[i].prob = 50; }
    seq_start(&s); run(&s, 384 * 100);
    CHECK(l.on[0] > 1600 * 40 / 100 && l.on[0] < 1600 * 60 / 100);

    /* polymeter: length 3 on a 16-step grid fires every 3rd step */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    s.track[0].len = 3; s.track[0].step[0].on = 1;
    seq_start(&s); run(&s, 24 * 9);
    CHECK(l.on[0] == 3);

    /* mute silences and releases a held note */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    s.track[0].step[0].on = 1; s.track[0].step[0].gate = 100;
    seq_start(&s); run(&s, 5);
    seq_track_mute_toggle(&s, 0);
    CHECK(l.off[0] == 1);
    run(&s, 384 * 2);
    CHECK(l.on[0] == 1);

    /* stop releases notes and sends all-notes-off + STOP */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    s.track[1].step[0].on = 1; s.track[1].step[0].gate = 100;
    seq_start(&s); run(&s, 3); seq_stop(&s);
    CHECK(l.off[1] == 1 && l.stops == 1 && l.cc123 == SEQ_TRACKS);
    run(&s, 100);
    CHECK(l.on[1] == 1);

    /* editing and clamping */
    seq_init(&s, NULL, NULL);
    s.scale = SCALE_CHROMATIC;
    seq_step_adjust(&s, 0, 0, 200, 0, 0, 0);
    CHECK(s.track[0].step[0].note == 127 && s.track[0].step[0].on);
    seq_step_adjust(&s, 0, 1, 0, -500, -500, -500);
    CHECK(s.track[0].step[1].vel == 1 && s.track[0].step[1].gate == 5 && s.track[0].step[1].prob == 0);
    seq_track_len(&s, 0, -99); CHECK(s.track[0].len == 1);
    seq_adjust_bpm(&s, 999); CHECK(s.bpm == 300);


    /* scales: stepping by degree stays in the scale (C minor pentatonic) */
    seq_init(&s, NULL, NULL);
    CHECK(s.scale == SCALE_MIN_PENT && s.root == 0);
    CHECK(gen_step_note(&s, 60, 1) == 63 && gen_step_note(&s, 60, -1) == 58 && gen_step_note(&s, 60, 5) == 72);
    CHECK(gen_step_note(&s, 61, 1) == 63);          /* off-scale note moves to the next scale note */
    s.root = 2;                                     /* D minor pentatonic */
    CHECK(gen_step_note(&s, 62, 1) == 65);
    s.root = 0;
    {
        uint8_t pool[64];
        int n = gen_pool(&s, 2, pool);              /* base 48, 2 octaves */
        CHECK(n == 11 && pool[0] == 48 && pool[n - 1] == 72);
        for (int i = 0; i < n; i++) CHECK(seq_in_scale(&s, pool[i]));
    }

    /* euclid: 4 hits in 16 steps land on every 4th step; keeps notes with PITCH=KEEP */
    seq_init(&s, NULL, NULL);
    gen_set_type(&s, 0, GEN_EUCLID);
    s.track[0].gen.p[0] = 4;
    s.track[0].step[5].note = 99;
    gen_apply(&s, 0);
    for (int i = 0; i < 16; i++) CHECK(s.track[0].step[i].on == (i % 4 == 0));
    CHECK(s.track[0].step[5].note == 99);
    s.track[0].gen.p[0] = 16; gen_apply(&s, 0);
    for (int i = 0; i < 16; i++) CHECK(s.track[0].step[i].on);
    s.track[0].gen.p[0] = 5; s.track[0].gen.p[1] = 3; gen_apply(&s, 0);     /* rotation keeps the hit count */
    { int c = 0; for (int i = 0; i < 16; i++) c += s.track[0].step[i].on; CHECK(c == 5); }

    /* locked steps survive generation and evolution */
    seq_init(&s, NULL, NULL);
    gen_set_type(&s, 0, GEN_EUCLID);
    s.track[0].gen.p[0] = 0;
    s.track[0].step[3] = (SeqStep){.on = 1, .note = 77, .vel = 90, .gate = 40, .prob = 100, .lock = 1};
    gen_apply(&s, 0);
    CHECK(s.track[0].step[3].on == 1 && s.track[0].step[3].note == 77);
    s.track[0].evo = (EvolveSettings){.every = 1, .amount = 100, .kinds = EVO_PITCH | EVO_RHYTHM | EVO_VEL | EVO_REGEN};
    for (int k = 0; k < 20; k++) gen_evolve_bar(&s, 0);
    CHECK(s.track[0].step[3].on == 1 && s.track[0].step[3].note == 77 && s.track[0].step[3].vel == 90);

    /* every generator writes only scale notes inside the pool range */
    for (int type = GEN_WALK; type < GEN_COUNT; type++) {
        seq_init(&s, NULL, NULL);
        s.track[1].step[0].on = 1; s.track[1].step[4].on = 1; s.track[1].step[4].note = 55;
        gen_set_type(&s, 1, type);
        for (int k = 0; k < 30; k++) {
            gen_roll(&s, 1);
            for (int i = 0; i < 16; i++) {
                int nt = s.track[1].step[i].note;
                CHECK(seq_in_scale(&s, nt) && nt >= 36 && nt <= 60 + 12);
            }
        }
    }

    /* markov with zero chaos only uses notes that were in the learned phrase */
    seq_init(&s, NULL, NULL);
    s.track[0].step[0] = (SeqStep){.on = 1, .note = 36, .vel = 100, .gate = 50, .prob = 100, .lock = 0};
    s.track[0].step[1] = (SeqStep){.on = 1, .note = 39, .vel = 100, .gate = 50, .prob = 100, .lock = 0};
    s.track[0].step[2] = (SeqStep){.on = 1, .note = 43, .vel = 100, .gate = 50, .prob = 100, .lock = 0};
    gen_set_type(&s, 0, GEN_MARKOV);
    s.track[0].gen.p[0] = 0;
    gen_apply(&s, 0);
    for (int i = 0; i < 16; i++) { int nt = s.track[0].step[i].note; CHECK(nt == 36 || nt == 39 || nt == 43); }

    /* cellular rule 90 from a single cell, 0 generations: just the centre cell */
    seq_init(&s, NULL, NULL);
    gen_set_type(&s, 0, GEN_CELLULAR);
    s.track[0].gen.p[0] = 1; s.track[0].gen.p[1] = 0;
    gen_apply(&s, 0);
    for (int i = 0; i < 16; i++) CHECK(s.track[0].step[i].on == (i == 8));
    s.track[0].gen.p[1] = 1; gen_apply(&s, 0);       /* rule 90: neighbours of the centre */
    for (int i = 0; i < 16; i++) CHECK(s.track[0].step[i].on == (i == 7 || i == 9));
    gen_roll(&s, 0);
    CHECK(s.track[0].gen.p[1] == 2);

    /* evolution: amount 0 changes nothing, amount 100 changes something */
    seq_init(&s, NULL, NULL); seq_load_demo(&s);
    { SeqTrack before = s.track[3];
      s.track[3].evo = (EvolveSettings){.every = 1, .amount = 0, .kinds = EVO_PITCH | EVO_RHYTHM | EVO_VEL | EVO_ROTATE};
      gen_evolve_bar(&s, 3);
      CHECK(memcmp(before.step, s.track[3].step, sizeof before.step) == 0);
      s.track[3].evo.amount = 100;
      gen_evolve_bar(&s, 3);
      CHECK(memcmp(before.step, s.track[3].step, sizeof before.step) != 0); }

    /* evolution is driven by the bar clock: every 2 bars means ticks 768, not 384 */
    memset(&l, 0, sizeof l); seq_init(&s, rec, &l); l.now = &s.tick;
    seq_load_demo(&s);
    s.track[0].evo = (EvolveSettings){.every = 2, .amount = 100, .kinds = EVO_RHYTHM};
    { SeqTrack before = s.track[0];
      seq_start(&s); run(&s, 400);
      CHECK(memcmp(before.step, s.track[0].step, sizeof before.step) == 0);
      run(&s, 400);
      CHECK(memcmp(before.step, s.track[0].step, sizeof before.step) != 0); }

    /* same seed, same result */
    { Seq x, y;
      seq_init(&x, NULL, NULL); seq_init(&y, NULL, NULL);
      gen_set_type(&x, 0, GEN_WALK); gen_set_type(&y, 0, GEN_WALK);
      gen_apply(&x, 0); gen_apply(&y, 0);
      CHECK(memcmp(x.track[0].step, y.track[0].step, sizeof x.track[0].step) == 0); }

    /* note names */
    char nm[8];
    seq_note_name(60, nm); CHECK(strcmp(nm, "C4") == 0);
    seq_note_name(61, nm); CHECK(strcmp(nm, "C#4") == 0);

    /* save / load round trip */
    Seq a, b;
    seq_init(&a, NULL, NULL); seq_load_demo(&a); a.bpm = 97; a.swing = 4; a.track[2].len = 11; a.track[3].mute = 1;
    a.scale = 3; a.root = 7; gen_set_type(&a, 1, GEN_MARKOV); a.track[1].gen.p[0] = 4; a.track[1].gen.base = 40;
    a.track[2].evo = (EvolveSettings){.every = 4, .amount = 35, .kinds = EVO_PITCH | EVO_REGEN}; a.track[0].step[2].lock = 1;
    FILE *f = tmpfile(); seq_save(&a, f); rewind(f);
    seq_init(&b, NULL, NULL);
    CHECK(seq_load(&b, f) == 0); fclose(f);
    CHECK(b.bpm == 97 && b.swing == 4 && b.track[2].len == 11 && b.track[3].mute == 1);
    CHECK(memcmp(a.track[2].step, b.track[2].step, sizeof a.track[2].step) == 0);
    CHECK(b.scale == 3 && b.root == 7 && b.track[1].gen.type == GEN_MARKOV && b.track[1].gen.p[0] == 4);
    CHECK(b.track[1].gen.base == 40 && b.track[2].evo.every == 4 && b.track[2].evo.amount == 35);
    CHECK(b.track[2].evo.kinds == (EVO_PITCH | EVO_REGEN) && b.track[0].step[2].lock == 1);
    f = tmpfile(); fputs("garbage\n", f); rewind(f);
    CHECK(seq_load(&b, f) != 0); fclose(f);

    puts("engine tests passed");
    return 0;
}
