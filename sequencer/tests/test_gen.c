/* Tests for the newer generators (FOLLOW, ARP, TURING, MOTIF), the pattern
 * transforms, scale snapping, and evolution's anchor, probability drift and
 * density limits. Pure engine code: no SDL, no MIDI, no hardware. */
#include "../src/engine.h"
#include "../src/gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int hits(const Seq *s, int t) { return gen_hits(s, t); }

static int same_steps(const SeqStep *a, const SeqStep *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i].on != b[i].on || a[i].note != b[i].note || a[i].vel != b[i].vel || a[i].gate != b[i].gate ||
            a[i].prob != b[i].prob || a[i].lock != b[i].lock)
            return 0;
    return 1;
}

static void test_scale_helpers(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    for (int sc = 0; sc < scale_count(); sc++) {
        for (int root = 0; root < 12; root += 5) {
            seq_set_scale(&s, sc, root, 0);
            for (int n = 0; n < 128; n++) {
                int sn = gen_snap_note(&s, n);
                CHECK(seq_in_scale(&s, sn));
                CHECK(abs(sn - n) <= 2);             /* no scale here has a gap wider than 3 semitones */
                if (seq_in_scale(&s, n)) {
                    CHECK(sn == n);
                    CHECK(gen_degree_note(&s, gen_note_degree(&s, n)) == n);
                }
            }
        }
    }

    /* switching scale with snap leaves every note in the new scale */
    seq_init(&s, NULL, NULL);
    seq_load_demo(&s);
    s.track[0].step[5].note = 61;                    /* C#: not in C minor pentatonic */
    seq_set_scale(&s, 1, 2, 1);                      /* D major */
    for (int t = 0; t < SEQ_TRACKS; t++)
        for (int i = 0; i < SEQ_STEPS; i++) CHECK(seq_in_scale(&s, s.track[t].step[i].note));
    CHECK(s.scale == 1 && s.root == 2);

    /* without snap, notes are left alone */
    seq_init(&s, NULL, NULL);
    s.track[0].step[0].note = 61;
    seq_set_scale(&s, 1, 0, 0);
    CHECK(s.track[0].step[0].note == 61);
}

static void test_follow(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    seq_load_demo(&s);                               /* track 1: kick on 0, 4, 8, 12 */
    s.track[0].step[4].vel = 127;
    gen_set_type(&s, 2, GEN_FOLLOW);
    CHECK(s.track[2].gen.p[0] == 2);                 /* defaults to the track above */
    s.track[2].gen.p[0] = 1;                         /* follow track 1 */

    s.track[2].gen.p[1] = 0;                         /* SAME */
    gen_apply(&s, 2);
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].on == s.track[0].step[i].on);
    CHECK(s.track[2].step[4].vel == 127);            /* accents follow */

    s.track[2].gen.p[1] = 1;                         /* GAPS */
    gen_apply(&s, 2);
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].on == !s.track[0].step[i].on);

    s.track[2].gen.p[1] = 2;                         /* ECHO */
    gen_apply(&s, 2);
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].on == s.track[0].step[(i + 15) % 16].on);

    s.track[2].gen.p[1] = 3;                         /* HALF */
    gen_apply(&s, 2);
    CHECK(hits(&s, 2) == 2 && s.track[2].step[0].on && !s.track[2].step[4].on && s.track[2].step[8].on);

    /* ROOT pitch plus an interval: every hit is the lowest pool note moved up 4 degrees */
    s.track[2].gen.p[1] = 0; s.track[2].gen.p[2] = 7 + 4; s.track[2].gen.p[3] = 0;
    gen_apply(&s, 2);
    { uint8_t pool[64]; gen_pool(&s, 2, pool);
      for (int i = 0; i < 16; i++) if (s.track[2].step[i].on) CHECK(s.track[2].step[i].note == gen_step_note(&s, pool[0], 4)); }

    /* HARMONY: source pitch + interval, folded into the track's own range */
    s.track[2].gen.p[3] = 1;
    s.track[0].step[8].note = 40;
    gen_apply(&s, 2);
    { uint8_t pool[64]; int n = gen_pool(&s, 2, pool);
      for (int i = 0; i < 16; i++) {
          int nt = s.track[2].step[i].note;
          CHECK(seq_in_scale(&s, nt) && nt >= pool[0] && nt <= pool[n - 1]);
      } }

    /* the source's own length wraps (polymeter): source len 3 with a hit on 0 */
    seq_init(&s, NULL, NULL);
    s.track[1].len = 3; s.track[1].step[0].on = 1;
    gen_set_type(&s, 0, GEN_FOLLOW);                 /* track 1 follows track 2 */
    gen_apply(&s, 0);
    for (int i = 0; i < 16; i++) CHECK(s.track[0].step[i].on == (i % 3 == 0));

    /* KEEP leaves this track's notes alone */
    seq_init(&s, NULL, NULL);
    s.track[1].step[2].on = 1;
    for (int i = 0; i < 16; i++) s.track[0].step[i].note = 41;
    gen_set_type(&s, 0, GEN_FOLLOW); s.track[0].gen.p[3] = 2;
    gen_apply(&s, 0);
    for (int i = 0; i < 16; i++) CHECK(s.track[0].step[i].note == 41);
}

static void test_arp(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    seq_set_scale(&s, 1, 0, 0);                      /* C major */
    gen_set_type(&s, 2, GEN_ARP);
    GenSettings *g = &s.track[2].gen;
    g->base = 48; g->span = 2;
    g->p[0] = 0; g->p[1] = 0; g->p[2] = 1; g->p[3] = 1;   /* triad, up, 1 octave, every step */
    gen_apply(&s, 2);
    static const int up[3] = {48, 52, 55};
    for (int i = 0; i < 16; i++) { CHECK(s.track[2].step[i].on); CHECK(s.track[2].step[i].note == up[i % 3]); }

    g->p[1] = 1; gen_apply(&s, 2);                   /* down */
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].note == up[2 - i % 3]);

    g->p[1] = 2; gen_apply(&s, 2);                   /* up-down: 48 52 55 52 */
    static const int ud[4] = {48, 52, 55, 52};
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].note == ud[i % 4]);

    g->p[0] = 1; g->p[1] = 4; gen_apply(&s, 2);      /* seventh, inward: 48 59 52 55 */
    static const int in[4] = {48, 59, 52, 55};
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].note == in[i % 4]);

    g->p[0] = 0; g->p[1] = 0; g->p[2] = 2; gen_apply(&s, 2);   /* two octaves: 48 52 55 60 64 67 */
    CHECK(s.track[2].step[3].note == 60 && s.track[2].step[5].note == 67 && s.track[2].step[6].note == 48);

    g->p[2] = 1; g->p[3] = 4; gen_apply(&s, 2);      /* rate 1/4: hits on 0, 4, 8, 12 only */
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].on == (i % 4 == 0));

    g->p[3] = 1; g->p[1] = 3; gen_apply(&s, 2);      /* random order uses chord tones only */
    for (int i = 0; i < 16; i++) { int n = s.track[2].step[i].note; CHECK(n == 48 || n == 52 || n == 55); }

    /* octaves are capped by the track's range */
    g->span = 1; g->p[1] = 0; g->p[2] = 4; gen_apply(&s, 2);
    for (int i = 0; i < 16; i++) CHECK(s.track[2].step[i].note <= 60);
}

static void test_turing(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    gen_set_type(&s, 3, GEN_TURING);
    GenSettings *g = &s.track[3].gen;
    CHECK((g->state & 0xFFFF) != 0);
    g->p[0] = 0; g->p[1] = 10; g->p[3] = 4;          /* never changes, always on, loop of 4 */
    gen_apply(&s, 3);
    SeqStep first[SEQ_STEPS];
    memcpy(first, s.track[3].step, sizeof first);
    for (int i = 0; i < 16; i++) CHECK(s.track[3].step[i].note == s.track[3].step[i % 4].note);

    for (int k = 0; k < 10; k++) gen_roll(&s, 3);    /* CHANGE 0: rolling just goes round the loop */
    CHECK(same_steps(first, s.track[3].step, 16));

    g->p[0] = 10; g->p[3] = 16;                      /* 50% of bits flip each roll */
    gen_apply(&s, 3);
    memcpy(first, s.track[3].step, sizeof first);
    int changed = 0;
    for (int k = 0; k < 5; k++) { gen_roll(&s, 3); changed |= !same_steps(first, s.track[3].step, 16); }
    CHECK(changed);

    g->p[1] = 0; gen_apply(&s, 3);                   /* density 0: silent */
    CHECK(hits(&s, 3) == 0);

    /* small CHANGE drifts slowly: one roll at 5% flips few of 16 bits */
    g->p[0] = 1; g->p[1] = 10;
    int flips = 0;
    for (int k = 0; k < 200; k++) {
        uint32_t before = g->state & 0xFFFF;
        gen_roll(&s, 3);
        flips += __builtin_popcount(before ^ (g->state & 0xFFFF)) > 0;
    }
    CHECK(flips > 50 && flips < 190);   /* P(no flip in 16 bits) = 0.95^16 = 44% */
}

static void test_motif(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    gen_set_type(&s, 2, GEN_MOTIF);
    GenSettings *g = &s.track[2].gen;
    g->p[1] = 0;                                     /* no development: the motif just repeats */
    for (int cl = 2; cl <= 8; cl++) {
        g->p[0] = (uint8_t)cl;
        gen_apply(&s, 2);
        CHECK(s.track[2].step[0].on);
        for (int i = cl; i < 16; i++) {
            CHECK(s.track[2].step[i].on == s.track[2].step[i % cl].on);
            CHECK(s.track[2].step[i].note == s.track[2].step[i % cl].note);
        }
    }
    /* with full development, at least one repeat differs from the first cell */
    g->p[0] = 4; g->p[1] = 10; g->p[2] = 10;
    int developed = 0;
    for (int k = 0; k < 10 && !developed; k++) {
        gen_apply(&s, 2);
        for (int i = 4; i < 16; i++) developed |= s.track[2].step[i].note != s.track[2].step[i % 4].note;
    }
    CHECK(developed);
}

/* Every generator, across every scale, writes in-scale notes inside the pool. */
static void test_all_generators_in_range(void) {
    for (int sc = 0; sc < scale_count(); sc++) {
        for (int type = GEN_EUCLID; type < GEN_COUNT; type++) {
            Seq s;
            seq_init(&s, NULL, NULL);
            seq_load_demo(&s);
            seq_set_scale(&s, sc, 5, 1);
            gen_set_type(&s, 1, type);
            if (type == GEN_EUCLID) s.track[1].gen.p[2] = 2;   /* random pitches */
            uint8_t pool[64];
            int n = gen_pool(&s, 1, pool);
            for (int k = 0; k < 20; k++) {
                gen_roll(&s, 1);
                for (int i = 0; i < 16; i++) {
                    int nt = s.track[1].step[i].note;
                    if (type == GEN_FOLLOW && s.track[1].gen.p[3] == 2) continue;
                    CHECK(seq_in_scale(&s, nt));
                    CHECK(nt >= pool[0] && nt <= pool[n - 1] + 12);
                    if (!seq_in_scale(&s, nt)) { fprintf(stderr, "  gen %s scale %s note %d\n", gen_name(type), scale_name(sc), nt); return; }
                }
            }
        }
    }
    /* parameter text never overflows or crashes */
    for (int type = 0; type < GEN_COUNT; type++)
        for (int k = 0; k < gen_param_count(type); k++)
            for (int v = gen_param_min(type, k); v <= gen_param_max(type, k); v++) {
                char out[16];
                gen_param_text(type, k, v, out);
                CHECK(strlen(out) > 0 && strlen(out) < 16);
            }
}

static void test_transforms(void) {
    Seq s;
    seq_init(&s, NULL, NULL);
    seq_load_demo(&s);
    seq_set_scale(&s, s.scale, s.root, 1);           /* the demo lead has a D outside the pentatonic */
    SeqTrack *tr = &s.track[3];
    SeqStep orig[SEQ_STEPS];
    memcpy(orig, tr->step, sizeof orig);

    gen_transform(&s, 3, XF_REVERSE, 0);
    CHECK(tr->step[15].on == orig[0].on && tr->step[15].note == orig[0].note);
    gen_transform(&s, 3, XF_REVERSE, 0);
    CHECK(same_steps(orig, tr->step, 16));

    /* a locked step stays put while the others reverse around it */
    tr->step[7].lock = 1;
    memcpy(orig, tr->step, sizeof orig);
    gen_transform(&s, 3, XF_REVERSE, 0);
    CHECK(same_steps(&orig[7], &tr->step[7], 1));
    gen_transform(&s, 3, XF_REVERSE, 0);
    CHECK(same_steps(orig, tr->step, 16));
    tr->step[7].lock = 0;

    memcpy(orig, tr->step, sizeof orig);
    gen_transform(&s, 3, XF_SHIFT, 1);
    for (int i = 0; i < 16; i++) CHECK(tr->step[(i + 1) % 16].note == orig[i].note && tr->step[(i + 1) % 16].on == orig[i].on);
    gen_transform(&s, 3, XF_SHIFT, -1);
    CHECK(same_steps(orig, tr->step, 16));
    gen_transform(&s, 3, XF_SHIFT, 16);
    CHECK(same_steps(orig, tr->step, 16));

    gen_transform(&s, 3, XF_TRANSPOSE, 2);
    for (int i = 0; i < 16; i++) CHECK(gen_note_degree(&s, tr->step[i].note) == gen_note_degree(&s, orig[i].note) + 2);
    gen_transform(&s, 3, XF_TRANSPOSE, -2);
    CHECK(same_steps(orig, tr->step, 16));

    gen_transform(&s, 3, XF_INVERT, 0);
    int pivot = orig[0].note;                         /* step 0 is the first sounding note */
    CHECK(tr->step[0].note == pivot);
    for (int i = 0; i < 16; i++)
        CHECK(gen_note_degree(&s, tr->step[i].note) - gen_note_degree(&s, pivot) ==
              gen_note_degree(&s, pivot) - gen_note_degree(&s, orig[i].note));
    gen_transform(&s, 3, XF_INVERT, 0);
    CHECK(same_steps(orig, tr->step, 16));

    /* thin removes offbeats before downbeats */
    seq_init(&s, NULL, NULL);
    for (int i = 0; i < 16; i++) s.track[0].step[i].on = 1;
    gen_transform(&s, 0, XF_THIN, 8);
    CHECK(hits(&s, 0) == 8);
    for (int i = 0; i < 16; i += 2) CHECK(s.track[0].step[i].on);
    gen_transform(&s, 0, XF_THIN, 100);
    CHECK(hits(&s, 0) == 0);

    /* fill borrows a neighbour's note */
    seq_init(&s, NULL, NULL);
    s.track[0].step[0].on = 1; s.track[0].step[0].note = 43;
    for (int i = 1; i < 16; i++) s.track[0].step[i].note = 30;
    gen_transform(&s, 0, XF_FILL, 3);
    CHECK(hits(&s, 0) == 4);
    for (int i = 0; i < 16; i++) if (s.track[0].step[i].on) CHECK(s.track[0].step[i].note == 43);

    /* humanize stays within +/- amount and leaves locks alone */
    seq_init(&s, NULL, NULL);
    s.track[0].step[3].lock = 1; s.track[0].step[3].vel = 64;
    gen_transform(&s, 0, XF_HUMANIZE, 10);
    for (int i = 0; i < 16; i++) CHECK(abs(s.track[0].step[i].vel - 100) <= 10 || i == 3);
    CHECK(s.track[0].step[3].vel == 64);
    memcpy(orig, s.track[0].step, sizeof orig);
    gen_transform(&s, 0, XF_HUMANIZE, 0);
    CHECK(same_steps(orig, s.track[0].step, 16));

    /* no transform touches steps beyond the track's length */
    seq_init(&s, NULL, NULL);
    s.track[0].len = 5;
    s.track[0].step[10].on = 1; s.track[0].step[10].note = 50;
    for (int op = 0; op < XF_COUNT; op++) gen_transform(&s, 0, op, 3);
    CHECK(s.track[0].step[10].on && s.track[0].step[10].note == 50);
    CHECK(strcmp(gen_xform_name(XF_FILL), "FILL") == 0);
}

static void test_evolution(void) {
    Seq s;
    /* pull 100 with nothing else enabled returns to the anchor in one bar */
    seq_init(&s, NULL, NULL);
    seq_load_demo(&s);
    gen_set_anchor(&s, 3);
    SeqStep orig[SEQ_STEPS];
    memcpy(orig, s.track[3].step, sizeof orig);
    gen_transform(&s, 3, XF_REVERSE, 0);
    gen_transform(&s, 3, XF_TRANSPOSE, 3);
    s.track[3].evo = (EvolveSettings){1, 0, 0, 100, 0, 0};
    gen_evolve_bar(&s, 3);
    CHECK(same_steps(orig, s.track[3].step, 16));

    /* pull 0 leaves it wherever it went */
    gen_transform(&s, 3, XF_TRANSPOSE, 3);
    s.track[3].evo.pull = 0;
    gen_evolve_bar(&s, 3);
    CHECK(!same_steps(orig, s.track[3].step, 16));

    /* with mutation and pull together, the pattern stays near the anchor */
    seq_init(&s, NULL, NULL);
    seq_load_demo(&s);
    gen_set_anchor(&s, 3);
    s.track[3].evo = (EvolveSettings){1, 20, EVO_PITCH | EVO_RHYTHM, 50, 0, 0};
    int total = 0;
    for (int bar = 0; bar < 200; bar++) {
        gen_evolve_bar(&s, 3);
        int diff = 0;
        for (int i = 0; i < 16; i++) diff += s.track[3].step[i].on != s.track[3].anchor[i].on || s.track[3].step[i].note != s.track[3].anchor[i].note;
        total += diff;
    }
    CHECK(total / 200 <= 6);                          /* drifts, but is held near home */
    gen_clear_anchor(&s, 3);
    CHECK(!s.track[3].has_anchor);

    /* density limits hold no matter how hard rhythm mutates */
    seq_init(&s, NULL, NULL);
    seq_load_demo(&s);
    s.track[2].evo = (EvolveSettings){1, 100, EVO_RHYTHM, 0, 4, 6};
    for (int bar = 0; bar < 300; bar++) {
        gen_evolve_bar(&s, 2);
        int h = hits(&s, 2);
        CHECK(h >= 4 && h <= 6);
        if (h < 4 || h > 6) break;
    }
    /* min above what's possible with locks stops at what it can do */
    seq_init(&s, NULL, NULL);
    s.track[0].len = 4;
    for (int i = 0; i < 4; i++) s.track[0].step[i].lock = 1;
    s.track[0].evo = (EvolveSettings){1, 100, EVO_RHYTHM, 0, 3, 0};
    gen_evolve_bar(&s, 0);
    CHECK(hits(&s, 0) == 0);

    /* probability drift stays within 10..100 and changes something */
    seq_init(&s, NULL, NULL);
    s.track[1].evo = (EvolveSettings){1, 100, EVO_PROB, 0, 0, 0};
    int moved = 0;
    for (int bar = 0; bar < 100; bar++) {
        gen_evolve_bar(&s, 1);
        for (int i = 0; i < 16; i++) {
            CHECK(s.track[1].step[i].prob >= 10 && s.track[1].step[i].prob <= 100);
            moved |= s.track[1].step[i].prob != 100;
        }
    }
    CHECK(moved);

    /* locks survive all of it, including the anchor pull and rotate */
    seq_init(&s, NULL, NULL);
    seq_load_demo(&s);
    gen_set_anchor(&s, 0);
    s.track[0].step[5] = (SeqStep){1, 40, 77, 33, 66, 1};
    s.track[0].evo = (EvolveSettings){1, 100, EVO_ALL, 100, 2, 8};
    gen_set_type(&s, 0, GEN_TURING);
    for (int bar = 0; bar < 50; bar++) {
        gen_evolve_bar(&s, 0);
        const SeqStep *st = &s.track[0].step[5];
        CHECK(st->on == 1 && st->note == 40 && st->vel == 77 && st->gate == 33 && st->prob == 66 && st->lock);
    }

    /* rotate now works with locked steps (it rotates around them) */
    seq_init(&s, NULL, NULL);
    s.track[0].step[0].on = 1; s.track[0].step[0].lock = 1;
    s.track[0].step[1].on = 1;
    s.track[0].evo = (EvolveSettings){1, 100, EVO_ROTATE, 0, 0, 0};
    gen_evolve_bar(&s, 0);
    CHECK(s.track[0].step[0].on && !s.track[0].step[1].on && hits(&s, 0) == 2);
}

static void test_save_load(void) {
    Seq a, b;
    seq_init(&a, NULL, NULL);
    seq_load_demo(&a);
    gen_set_type(&a, 3, GEN_TURING);
    a.track[3].gen.state = 0xBEEF;
    gen_set_type(&a, 2, GEN_FOLLOW);
    a.track[2].gen.p[0] = 1; a.track[2].gen.p[2] = 10;
    a.track[1].evo = (EvolveSettings){2, 30, EVO_PROB | EVO_PITCH, 40, 3, 9};
    gen_set_anchor(&a, 1);
    a.track[1].anchor[6].note = 70;
    FILE *f = tmpfile();
    seq_save(&a, f); rewind(f);
    seq_init(&b, NULL, NULL);
    CHECK(seq_load(&b, f) == 0);
    fclose(f);
    CHECK(b.track[3].gen.type == GEN_TURING && b.track[3].gen.state == 0xBEEF);
    CHECK(b.track[2].gen.type == GEN_FOLLOW && b.track[2].gen.p[0] == 1 && b.track[2].gen.p[2] == 10);
    CHECK(b.track[1].evo.kinds == (EVO_PROB | EVO_PITCH) && b.track[1].evo.pull == 40);
    CHECK(b.track[1].evo.min_hits == 3 && b.track[1].evo.max_hits == 9);
    CHECK(b.track[1].has_anchor && b.track[1].anchor[6].note == 70);
    CHECK(!b.track[0].has_anchor);
    /* the restored TURING register gives the same pattern */
    gen_apply(&a, 3); gen_apply(&b, 3);
    CHECK(same_steps(a.track[3].step, b.track[3].step, 16));

    /* a file from before these features loads with neutral defaults */
    f = tmpfile();
    fputs("seq4h 1\nbpm 100\ne 0 2 30 3\ns 0 0 1 36 100 50 100 0\n", f);
    rewind(f);
    seq_init(&b, NULL, NULL);
    CHECK(seq_load(&b, f) == 0);
    fclose(f);
    CHECK(b.track[0].evo.every == 2 && b.track[0].evo.pull == 0 && b.track[0].evo.max_hits == 0);
    CHECK(!b.track[0].has_anchor);
}

int main(void) {
    test_scale_helpers();
    test_follow();
    test_arp();
    test_turing();
    test_motif();
    test_all_generators_in_range();
    test_transforms();
    test_evolution();
    test_save_load();
    if (fails) { fprintf(stderr, "%d generator check(s) failed\n", fails); return 1; }
    puts("generator tests passed");
    return 0;
}
