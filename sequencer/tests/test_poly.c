/* Tests for polyphonic tracks: chord notes on a step (SeqStep.more), VOICES
 * MONO/POLY, overlapping notes, the voice cap, the CHORDS generator, WRITE
 * CHORD, saving, and no hung notes on stop, mute, panic or a switch.
 * Pure engine code: no SDL, no MIDI, no hardware. */
#include "../src/engine.h"
#include "../src/gen.h"
#include "../src/drum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* Captures MIDI: which notes are down per channel, the most at once, and
 * what was sent on each tick. */
typedef struct {
    int down[16][128];
    int max_down[16];
    int ons, offs, double_on;
    int on_at[64], n_on_at;   /* notes switched on since the last clear */
} Cap;

static void cap_send(void *ctx, const uint8_t *m, int len) {
    Cap *c = ctx;
    if (len != 3) return;
    int ch = m[0] & 15, st = m[0] & 0xF0;
    if (st == 0x90 && m[2]) {
        if (c->down[ch][m[1]]) c->double_on++;
        c->down[ch][m[1]] = 1;
        c->ons++;
        if (c->n_on_at < 64) c->on_at[c->n_on_at++] = m[1];
        int n = 0;
        for (int k = 0; k < 128; k++) n += c->down[ch][k];
        if (n > c->max_down[ch]) c->max_down[ch] = n;
    } else if (st == 0x80 || (st == 0x90 && !m[2])) {
        c->down[ch][m[1]] = 0;
        c->offs++;
    } else if (st == 0xB0 && (m[1] == 123 || m[1] == 120)) {
        memset(c->down[ch], 0, sizeof c->down[ch]);
    }
}

static int down_count(const Cap *c, int ch) {
    int n = 0;
    for (int k = 0; k < 128; k++) n += c->down[ch][k];
    return n;
}

static int all_down(const Cap *c) {
    int n = 0;
    for (int ch = 0; ch < 16; ch++) n += down_count(c, ch);
    return n;
}

static void ticks(Seq *s, int n) { for (int i = 0; i < n; i++) seq_tick(s); }

/* C major, a plain track 0 */
static void setup(Seq *s, Cap *c) {
    memset(c, 0, sizeof *c);
    seq_init(s, cap_send, c);
    s->clock_out = 0;
    seq_set_scale(s, 1, 0, 0);
    for (int t = 0; t < SEQ_TRACKS; t++) for (int i = 0; i < SEQ_STEPS; i++) s->track[t].step[i].on = 0;
    s->track[0].len = 16;
}

static int scale_major(const Seq *s) { return strcmp(scale_name(s->scale), "MAJOR") == 0; }

static void test_step_notes(void) {
    Seq s; Cap c;
    setup(&s, &c);
    CHECK(scale_major(&s));
    SeqStep *st = &s.track[0].step[0];
    uint8_t nn[SEQ_POLY];
    /* an empty step: the first note becomes its own note */
    CHECK(seq_step_add_note(&s, 0, 0, 60) == 0);
    CHECK(st->on && st->note == 60 && seq_step_more_count(st) == 0);
    CHECK(seq_step_add_note(&s, 0, 0, 64) == 0);
    CHECK(seq_step_add_note(&s, 0, 0, 67) == 0);
    CHECK(seq_step_add_note(&s, 0, 0, 67) == 1);   /* already there */
    CHECK(seq_step_more_count(st) == 2 && st->more[0] == 2 && st->more[1] == 4);
    CHECK(seq_step_notes(&s, st, nn) == 3 && nn[0] == 60 && nn[1] == 64 && nn[2] == 67);
    CHECK(seq_step_chord_shape(&s, st) == CHORD_TRIAD);
    CHECK(seq_step_add_note(&s, 0, 0, 71) == 0);
    CHECK(seq_step_add_note(&s, 0, 0, 72) == -1);   /* four notes is the most */
    CHECK(seq_step_chord_shape(&s, st) == CHORD_SEVENTH);
    /* chord notes follow the step's note */
    seq_step_adjust(&s, 0, 0, 1, 0, 0, 0);   /* up a scale degree: D minor seventh */
    CHECK(seq_step_notes(&s, st, nn) == 4 && nn[0] == 62 && nn[1] == 65 && nn[2] == 69 && nn[3] == 72);
    /* take out the step's own note: the lowest chord note takes over */
    CHECK(seq_step_remove_note(&s, 0, 0, 62) == 0);
    CHECK(st->on && st->note == 65 && seq_step_notes(&s, st, nn) == 3 && nn[1] == 69 && nn[2] == 72);
    CHECK(seq_step_remove_note(&s, 0, 0, 72) == 0);
    CHECK(seq_step_remove_note(&s, 0, 0, 50) == -1);
    CHECK(seq_step_remove_note(&s, 0, 0, 65) == 0 && st->on && st->note == 69 && !seq_step_more_count(st));
    CHECK(seq_step_remove_note(&s, 0, 0, 69) == 0 && !st->on);
    /* chord shapes on a step, and below the note */
    st->on = 1; st->note = 48;
    seq_step_set_chord(&s, 0, 0, CHORD_OPEN);
    CHECK(seq_step_notes(&s, st, nn) == 3 && nn[1] == 55 && nn[2] == 64);
    seq_step_set_chord(&s, 0, 0, CHORD_OFF);
    CHECK(!seq_step_more_count(st) && seq_step_chord_shape(&s, st) == CHORD_OFF);
    CHECK(seq_step_add_note(&s, 0, 0, 43) == 0 && st->more[0] == -3);   /* a G under the C */
    CHECK(seq_step_chord_shape(&s, st) == -1);
    /* a new scale keeps the chord in it */
    seq_step_set_chord(&s, 0, 0, CHORD_SEVENTH);
    seq_set_scale(&s, SCALE_MIN_PENT, 0, 1);
    int n = seq_step_notes(&s, st, nn);
    for (int k = 0; k < n; k++) CHECK(seq_in_scale(&s, nn[k]));
}

static void test_mono_poly_playback(void) {
    Seq s; Cap c;
    setup(&s, &c);
    SeqStep *st = &s.track[0].step[0];
    st->on = 1; st->note = 60; st->gate = 100; st->hold = 3;   /* four steps long */
    seq_step_set_chord(&s, 0, 0, CHORD_TRIAD);
    s.track[0].step[1] = (SeqStep){1, 72, 100, 50, 100, 0, 0, 0, {0}};
    /* MONO: only the step's note, and the next note cuts it */
    seq_start(&s);
    ticks(&s, 1);
    CHECK(c.n_on_at == 1 && c.on_at[0] == 60);
    ticks(&s, 24);
    CHECK(!c.down[0][60] && c.down[0][72]);
    seq_stop(&s);
    CHECK(all_down(&c) == 0);

    /* POLY: the chord, ringing for its own length under the next note */
    seq_track_poly(&s, 0, 1);
    c.n_on_at = 0;
    seq_start(&s);
    ticks(&s, 1);
    CHECK(c.n_on_at == 3 && c.on_at[0] == 60 && c.on_at[1] == 64 && c.on_at[2] == 67);
    ticks(&s, 24);
    CHECK(c.down[0][60] && c.down[0][64] && c.down[0][67] && c.down[0][72]);
    ticks(&s, 71);   /* ticks 0-95 done: still held */
    CHECK(c.down[0][60]);
    ticks(&s, 1);    /* tick 96: four steps are over */
    CHECK(!c.down[0][60] && !c.down[0][64] && !c.down[0][67]);
    seq_stop(&s);
    CHECK(all_down(&c) == 0 && c.double_on == 0);

    /* the CHORD row still fills a single note on a POLY track */
    seq_step_set_chord(&s, 0, 0, CHORD_OFF);
    s.track[0].chord = CHORD_FIFTH;
    c.n_on_at = 0;
    seq_start(&s);
    ticks(&s, 1);
    CHECK(c.n_on_at == 2 && c.on_at[0] == 60 && c.on_at[1] == 67);
    seq_stop(&s);
    CHECK(all_down(&c) == 0);
}

static void test_retrigger_and_cap(void) {
    Seq s; Cap c;
    setup(&s, &c);
    seq_track_poly(&s, 0, 1);
    /* the same note on every step, held long: it restarts, never doubles */
    for (int i = 0; i < 16; i++) s.track[0].step[i] = (SeqStep){1, 60, 100, 100, 100, 0, 8, 0, {0}};
    seq_start(&s);
    ticks(&s, 24 * 64);
    CHECK(c.double_on == 0 && c.max_down[0] == 1);
    seq_stop(&s);
    CHECK(all_down(&c) == 0);

    /* four-note chords on every step, each held 8 steps: never more than 8 at once */
    for (int i = 0; i < 16; i++) {
        s.track[0].step[i] = (SeqStep){1, (uint8_t)(48 + (i % 7) * 2), 100, 100, 100, 0, 8, 0, {0}};
        seq_step_set_chord(&s, 0, i, CHORD_SEVENTH);
    }
    memset(c.max_down, 0, sizeof c.max_down);
    seq_start(&s);
    ticks(&s, 24 * 64);
    CHECK(c.max_down[0] == SEQ_POLY_VOICES && c.double_on == 0);
    CHECK(s.track[0].npv <= SEQ_POLY_VOICES);
    seq_stop(&s);
    CHECK(all_down(&c) == 0 && s.track[0].npv == 0);
}

/* Everything that silences a track releases its POLY notes. */
static void test_release(void) {
    Seq s; Cap c;
    setup(&s, &c);
    seq_track_poly(&s, 0, 1);
    for (int i = 0; i < 16; i += 4) {
        s.track[0].step[i] = (SeqStep){1, (uint8_t)gen_snap_note(&s, 50 + i), 100, 100, 100, 0, 6, 0, {0}};
        seq_step_set_chord(&s, 0, i, CHORD_SEVENTH);
    }
    #define PLAYING() do { seq_start(&s); ticks(&s, 24 * 5 + 3); CHECK(down_count(&c, s.track[0].channel) > 4); } while (0)
    PLAYING(); seq_stop(&s); CHECK(all_down(&c) == 0);
    PLAYING(); seq_track_mute_toggle(&s, 0); CHECK(all_down(&c) == 0); seq_track_mute_toggle(&s, 0); seq_stop(&s);
    PLAYING(); seq_track_solo_toggle(&s, 1); CHECK(all_down(&c) == 0); seq_track_solo_toggle(&s, 1); seq_stop(&s);
    PLAYING(); seq_track_poly(&s, 0, 0); CHECK(all_down(&c) == 0); seq_track_poly(&s, 0, 1); seq_stop(&s);
    PLAYING(); seq_track_channel(&s, 0, 1); CHECK(all_down(&c) == 0); seq_track_channel(&s, 0, -1); seq_stop(&s);
    PLAYING(); seq_track_type(&s, 0, TRACK_DRUMS); CHECK(all_down(&c) == 0); seq_track_type(&s, 0, TRACK_NOTES); seq_stop(&s);
    /* a replug: panic releases everything; nothing is left in the voice table */
    PLAYING();
    memset(c.down, 0, sizeof c.down);   /* pretend the device forgot, then check note-offs still go out */
    int offs = c.offs;
    seq_reconnected(&s);
    CHECK(c.offs - offs >= 4 && s.track[0].npv == 0);
    seq_stop(&s);
    #undef PLAYING
}

static void roundtrip(const Seq *a, Seq *b, char *buf, size_t n) {
    FILE *f = fmemopen(buf, n, "w");
    seq_save(a, f);
    fclose(f);
    seq_init(b, NULL, NULL);
    f = fmemopen(buf, strlen(buf), "r");
    CHECK(seq_load(b, f) == 0);
    fclose(f);
}

static void test_save_load(void) {
    static char buf1[1 << 18], buf2[1 << 18];
    Seq s, l; Cap c;
    setup(&s, &c);
    seq_track_poly(&s, 2, 1);
    s.track[2].step[3] = (SeqStep){1, 60, 90, 50, 100, 0, 0, 0, {0}};
    seq_step_set_chord(&s, 2, 3, CHORD_SEVENTH);
    seq_step_add_note(&s, 2, 5, 50);
    seq_step_add_note(&s, 2, 5, 43);   /* a chord note below */
    gen_set_anchor(&s, 2);
    seq_slot_store(&s, 2, 4);
    roundtrip(&s, &l, buf1, sizeof buf1);
    CHECK(strstr(buf1, "vo 2 1\n") && strstr(buf1, "x 2 3 2 4 6\n") && strstr(buf1, "ax 2 3 2 4 6\n") && strstr(buf1, "sx 2 4 3 2 4 6\n"));
    CHECK(l.track[2].poly == 1 && l.track[0].poly == 0);
    CHECK(!memcmp(l.track[2].step, s.track[2].step, sizeof s.track[2].step));
    CHECK(!memcmp(l.track[2].anchor, s.track[2].anchor, sizeof s.track[2].anchor));
    CHECK(!memcmp(l.track[2].slot[4].step, s.track[2].slot[4].step, sizeof s.track[2].slot[4].step));
    roundtrip(&l, &s, buf2, sizeof buf2);
    CHECK(strcmp(buf1, buf2) == 0);   /* save, load, save: the same file */

    /* an older file (no vo or x lines) loads as MONO single notes */
    char old[] = "seq4h 1\nbpm 90\nt 0 0 16 0\ns 0 2 1 62 100 50 100 0\n";
    FILE *f = fmemopen(old, strlen(old), "r");
    seq_init(&l, NULL, NULL);
    CHECK(seq_load(&l, f) == 0);
    fclose(f);
    CHECK(!l.track[0].poly && l.track[0].step[2].on && !seq_step_more_count(&l.track[0].step[2]));
    /* bad chord lines are cleaned up */
    char bad[] = "seq4h 1\ns 0 2 1 62 100 50 100 0\nx 0 2 0 99 3\nx 9 2 1 2 3\nvo 7 1\n";
    f = fmemopen(bad, strlen(bad), "r");
    seq_init(&l, NULL, NULL);
    CHECK(seq_load(&l, f) == 0);
    fclose(f);
    CHECK(l.track[0].step[2].more[0] == 3 && l.track[0].step[2].more[1] == 0);
}

static void test_chords_generator(void) {
    Seq s; Cap c;
    setup(&s, &c);
    SeqTrack *tr = &s.track[1];
    tr->len = 32;
    tr->gen.base = 48; tr->gen.span = 2;
    CHECK(!tr->poly);
    gen_set_type(&s, 1, GEN_CHORDS);
    CHECK(tr->poly == 1);   /* chords need POLY */
    CHECK(strcmp(gen_name(GEN_CHORDS), "CHORDS") == 0 && strcmp(gen_short_name(GEN_CHORDS), "CHORDS") == 0);
    s.rng = 99;
    gen_apply(&s, 1);
    /* HOLD, every 16: two chords, each held to the next */
    CHECK(gen_hits(&s, 1) == 2 && tr->step[0].on && tr->step[16].on);
    CHECK(tr->step[0].hold == 15 && tr->step[16].hold == 15);
    CHECK(seq_step_more_count(&tr->step[0]) == 2 && seq_step_more_count(&tr->step[16]) == 2);
    uint8_t nn[SEQ_POLY];
    int n = seq_step_notes(&s, &tr->step[0], nn);
    CHECK(n == 3 && nn[0] == 48 && nn[1] == 52 && nn[2] == 55);   /* C major, root position on the lowest note */
    /* PULSE every beat, SEVENTH, many rolls: always in the scale and the range, voices moving little */
    tr->gen.p[1] = 1; tr->gen.p[3] = 1; tr->gen.p[2] = 10;
    for (int r = 0; r < 200; r++) {
        gen_roll(&s, 1);
        CHECK(gen_hits(&s, 1) == 8);
        int prev[SEQ_POLY] = {0}, have = 0;
        for (int i = 0; i < 32; i++) {
            const SeqStep *st = &tr->step[i];
            if (!st->on) continue;
            CHECK(i % 4 == 0 && st->hold == 3);
            n = seq_step_notes(&s, st, nn);
            CHECK(n == 4);
            for (int k = 0; k < n; k++) {
                CHECK(seq_in_scale(&s, nn[k]) && nn[k] >= 48 && nn[k] <= 48 + 36);
                if (k) CHECK(nn[k] > nn[0]);
            }
            if (have) {   /* voice leading: no voice leaps more than a fifth-ish */
                int moved = 0;
                for (int k = 0; k < n; k++) moved += abs(gen_note_degree(&s, nn[k]) - prev[k]);
                CHECK(moved <= 4 * 4);
            }
            for (int k = 0; k < n; k++) prev[k] = gen_note_degree(&s, nn[k]);
            have = 1;
        }
    }
    /* STABS: short, off the beat */
    tr->gen.p[3] = 2;
    gen_apply(&s, 1);
    CHECK(tr->step[2].on && !tr->step[0].on && tr->step[2].hold == 0 && tr->step[2].gate == 50);
    /* a locked step keeps its chord */
    tr->step[2].lock = 1;
    SeqStep keep = tr->step[2];
    for (int r = 0; r < 20; r++) gen_roll(&s, 1);
    CHECK(!memcmp(&keep, &tr->step[2], sizeof keep));
    /* other generators write single notes */
    gen_set_type(&s, 1, GEN_WALK);
    gen_apply(&s, 1);
    for (int i = 0; i < 32; i++) if (i != 2) CHECK(!seq_step_more_count(&tr->step[i]));
}

static void test_transforms(void) {
    Seq s; Cap c;
    setup(&s, &c);
    SeqTrack *tr = &s.track[0];
    for (int i = 0; i < 16; i += 2) tr->step[i] = (SeqStep){1, (uint8_t)(60 + i), 100, 50, 100, 0, 0, 0, {0}};
    for (int i = 0; i < 16; i += 2) tr->step[i].note = (uint8_t)gen_snap_note(&s, tr->step[i].note);
    tr->chord = CHORD_TRIAD;
    gen_transform(&s, 0, XF_CHORD, 0);
    CHECK(tr->poly == 1);
    for (int i = 0; i < 16; i += 2) CHECK(seq_step_chord_shape(&s, &tr->step[i]) == CHORD_TRIAD);
    CHECK(!seq_step_more_count(&tr->step[1]));   /* silent steps get no chord */
    gen_transform(&s, 0, XF_INVERT, 0);
    CHECK(tr->step[2].more[0] == -2 && tr->step[2].more[1] == -4);
    tr->chord = CHORD_OFF;
    gen_transform(&s, 0, XF_CHORD, 0);
    for (int i = 0; i < 16; i++) CHECK(!seq_step_more_count(&tr->step[i]));
    seq_step_set_chord(&s, 0, 4, CHORD_SUS4);
    gen_transform(&s, 0, XF_CLEAR, 0);
    CHECK(!tr->step[4].on && !seq_step_more_count(&tr->step[4]));
    /* morph carries chords too */
    SeqStep target[SEQ_STEPS];
    memcpy(target, tr->step, sizeof target);
    target[6].on = 1;
    seq_step_set_chord(&s, 0, 6, CHORD_TRIAD);
    memcpy(target[6].more, tr->step[6].more, sizeof target[6].more);
    memset(tr->step[6].more, 0, sizeof tr->step[6].more);
    gen_morph(&s, 0, target, 100);
    CHECK(seq_step_chord_shape(&s, &tr->step[6]) == CHORD_TRIAD);
}

/* A long run: the DRONE style, every track POLY, CHORDS on track 2,
 * evolution, chains, mutes, stop and start: never more than 8 notes a
 * track, no note-on twice, nothing left sounding after a stop. */
static void test_long_run(void) {
    Seq s; Cap c;
    memset(&c, 0, sizeof c);
    seq_init(&s, cap_send, &c);
    s.rng = 777;
    style_apply(&s, STYLE_DRONE, 0);
    for (int t = 0; t < SEQ_TRACKS; t++) seq_track_poly(&s, t, 1);
    gen_set_type(&s, 1, GEN_CHORDS);
    s.track[1].evo.kinds |= EVO_REGEN;
    s.track[1].evo.every = 4;
    s.track[3].evo.every = 1; s.track[3].evo.amount = 40;
    for (int t = 0; t < SEQ_TRACKS; t++) gen_apply(&s, t);
    seq_start(&s);
    for (int bar = 0; bar < 1500; bar++) {
        ticks(&s, SEQ_PPQN * 4);
        if (bar % 97 == 13) seq_track_mute_toggle(&s, bar % 4);
        if (bar % 211 == 50) { seq_stop(&s); CHECK(all_down(&c) == 0); seq_start(&s); }
    }
    for (int ch = 0; ch < 16; ch++) CHECK(c.max_down[ch] <= SEQ_POLY_VOICES);
    CHECK(c.double_on == 0);
    seq_stop(&s);
    CHECK(all_down(&c) == 0);
    for (int t = 0; t < SEQ_TRACKS; t++) CHECK(s.track[t].npv == 0 && s.track[t].sounding < 0);
}

int main(void) {
    test_step_notes();
    test_mono_poly_playback();
    test_retrigger_and_cap();
    test_release();
    test_save_load();
    test_chords_generator();
    test_transforms();
    test_long_run();
    if (fails) { fprintf(stderr, "%d poly test(s) failed\n", fails); return 1; }
    printf("poly tests passed\n");
    return 0;
}
