/* Tests for HARMONY (chord progressions tracks follow as they play), FOLLOW
 * modes, and KEY CHANGE > MOVE ALL. Pure engine code: no SDL, no MIDI. */
#include "../src/engine.h"
#include "../src/gen.h"
#include "../src/drum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

typedef struct { uint32_t tick; int ch, note; } On;
typedef struct { const Seq *s; On on[4096]; int n, down[16][128]; } Cap;

static void cap_send(void *ctx, const uint8_t *m, int len) {
    Cap *c = ctx;
    if (len != 3) return;
    int ch = m[0] & 15, st = m[0] & 0xF0;
    if (st == 0x90 && m[2]) { if (c->n < 4096) c->on[c->n++] = (On){c->s->tick, ch, m[1]}; c->down[ch][m[1]] = 1; }
    else if (st == 0x80 || (st == 0x90 && !m[2])) c->down[ch][m[1]] = 0;
    else if (st == 0xB0 && (m[1] == 123 || m[1] == 120)) memset(c->down[ch], 0, sizeof c->down[ch]);
}

static void ticks(Seq *s, int n) { for (int i = 0; i < n; i++) seq_tick(s); }
#define BAR (SEQ_PPQN * 4)

/* C major, everything off; track 0 plays one C4 on every beat. */
static void setup(Seq *s, Cap *c) {
    memset(c, 0, sizeof *c);
    c->s = s;
    seq_init(s, cap_send, c);
    s->clock_out = s->transport_out = 0;
    s->scale = 1;   /* checked below to be MAJOR */
    s->root = 0;
    for (int t = 0; t < SEQ_TRACKS; t++)
        for (int i = 0; i < SEQ_STEPS; i++) s->track[t].step[i].on = 0;
    SeqTrack *tr = &s->track[0];
    tr->len = 4;
    tr->step[0].on = 1;
    tr->step[0].note = 60;
}

/* The note track 0 sent on the first beat of bar b (from 0). */
static int note_at_bar(const Cap *c, int b) {
    for (int k = 0; k < c->n; k++) if (c->on[k].ch == 0 && c->on[k].tick == (uint32_t)(b * BAR)) return c->on[k].note;
    return -1;
}

static void test_names(void) {
    CHECK(strcmp(gen_harm_name(HP_I_VI_IV_V), "I-VI-IV-V") == 0);
    CHECK(strcmp(gen_harm_name(HP_OFF), "OFF") == 0);
    CHECK(strcmp(gen_harm_roman(3), "IV") == 0);
    CHECK(strcmp(gen_harm_mode_name(HARM_ROOT), "ROOT (BASS)") == 0);
}

static void test_note_maps(void) {
    Seq s; Cap c;
    setup(&s, &c);
    CHECK(strcmp(scale_name(s.scale), "MAJOR") == 0);
    /* MOVE: by scale degrees */
    CHECK(gen_harm_note(&s, 60, 0, HARM_MOVE) == 60);
    CHECK(gen_harm_note(&s, 60, 3, HARM_MOVE) == 65);   /* C up 3 degrees = F */
    CHECK(gen_harm_note(&s, 64, 4, HARM_MOVE) == 71);   /* E up 4 = B */
    /* CHORD: nearest triad tone */
    CHECK(gen_harm_note(&s, 62, 0, HARM_CHORD) == 60);  /* D -> C (tie goes down) */
    CHECK(gen_harm_note(&s, 65, 0, HARM_CHORD) == 64);  /* F -> E */
    CHECK(gen_harm_note(&s, 67, 0, HARM_CHORD) == 67);  /* G stays */
    CHECK(gen_harm_note(&s, 62, 3, HARM_CHORD) == 60);  /* IV = F A C: D -> C */
    CHECK(gen_harm_note(&s, 64, 3, HARM_CHORD) == 65);  /* E -> F */
    CHECK(gen_harm_note(&s, 71, 4, HARM_CHORD) == 71);  /* V = G B D: B stays */
    /* ROOT: the chord's root at or below */
    CHECK(gen_harm_note(&s, 36, 0, HARM_ROOT) == 36);
    CHECK(gen_harm_note(&s, 40, 3, HARM_ROOT) == 29);   /* E2 under IV: F1 */
    CHECK(gen_harm_note(&s, 43, 4, HARM_ROOT) == 43);   /* G2 under V: G2 */
    CHECK(gen_harm_note(&s, 45, 5, HARM_ROOT) == 45);   /* A2 under VI */
    CHECK(gen_harm_note(&s, 60, 2, HARM_OFF) == 60);
    /* pentatonic: chords are built from the scale's own notes */
    s.scale = SCALE_MIN_PENT;
    s.root = 9;   /* A minor pentatonic: A C D E G */
    int n = gen_harm_note(&s, 57, 0, HARM_CHORD);
    CHECK(seq_in_scale(&s, n));
    CHECK(gen_harm_note(&s, 57, 0, HARM_ROOT) == 57);
}

static void test_progression(void) {
    Seq s; Cap c;
    setup(&s, &c);
    s.harm = HP_I_VI_IV_V;
    s.harm_bars = 1;
    seq_start(&s);
    ticks(&s, BAR * 5 + 1);
    CHECK(note_at_bar(&c, 0) == 60);   /* I */
    CHECK(note_at_bar(&c, 1) == 69);   /* VI: C moved up 5 degrees = A */
    CHECK(note_at_bar(&c, 2) == 65);   /* IV */
    CHECK(note_at_bar(&c, 3) == 67);   /* V */
    CHECK(note_at_bar(&c, 4) == 60);   /* round again */
    seq_stop(&s);

    /* 2 bars per chord, and ROOT mode on a bass track */
    setup(&s, &c);
    s.harm = HP_I_IV;
    s.harm_bars = 2;
    s.track[0].step[0].note = 40;   /* E2 */
    s.track[0].harm_mode = HARM_ROOT;
    seq_start(&s);
    ticks(&s, BAR * 4 + 1);
    CHECK(note_at_bar(&c, 0) == 36 && note_at_bar(&c, 1) == 36);
    CHECK(note_at_bar(&c, 2) == 29 && note_at_bar(&c, 3) == 29);
    seq_stop(&s);

    /* OFF on the track: plays as written; HARMONY OFF: everyone does */
    setup(&s, &c);
    s.harm = HP_I_IV; s.harm_bars = 1;
    s.track[0].harm_mode = HARM_OFF;
    seq_start(&s);
    ticks(&s, BAR * 2 + 1);
    CHECK(note_at_bar(&c, 1) == 60);
    seq_stop(&s);

    /* MONO notes are released as the note that was sent, nothing hangs */
    setup(&s, &c);
    s.harm = HP_I_V; s.harm_bars = 1;
    for (int i = 0; i < 4; i++) { s.track[0].step[i].on = 1; s.track[0].step[i].note = (uint8_t)(60 + 2 * i); s.track[0].step[i].hold = 3; }
    seq_start(&s);
    ticks(&s, BAR * 3);
    seq_stop(&s);
    int down = 0;
    for (int k = 0; k < 128; k++) down += c.down[0][k];
    CHECK(down == 0);

    /* drum tracks never follow */
    setup(&s, &c);
    s.harm = HP_I_V; s.harm_bars = 1;
    seq_track_type(&s, 1, TRACK_DRUMS);
    s.track[1].drum.vel[0][0] = 100;
    seq_start(&s);
    ticks(&s, BAR * 2 + 1);
    for (int k = 0; k < c.n; k++) if (c.on[k].ch == 1) CHECK(c.on[k].note == s.lane_note[0]);
    seq_stop(&s);

    /* DRIFT: changes every harm_bars bars, never the same chord twice running */
    setup(&s, &c);
    s.harm = HP_DRIFT; s.harm_bars = 1;
    seq_start(&s);
    int last = 0;
    ticks(&s, 1);
    for (int b = 1; b <= 40; b++) {
        ticks(&s, BAR);   /* crosses one bar line */
        int d = gen_harm_degree(&s, s.tick);
        CHECK(d >= 0 && d <= 6);
        CHECK(d != last);
        last = d;
    }
    seq_stop(&s);
}

static void test_key_move(void) {
    Seq s; Cap c;
    setup(&s, &c);
    s.track[0].step[0].note = 60;
    s.track[0].step[1].note = 64;
    seq_slot_store(&s, 0, 0);
    gen_set_anchor(&s, 0);
    int base = s.track[0].gen.base;
    seq_track_type(&s, 3, TRACK_DRUMS);
    s.track[3].step[0].note = 50;
    gen_key_move(&s, 2);   /* C -> D: up 2 */
    CHECK(s.root == 2);
    CHECK(s.track[0].step[0].note == 62 && s.track[0].step[1].note == 66);
    CHECK(s.track[0].slot[0].step[0].note == 62 && s.track[0].anchor[1].note == 66);
    CHECK(s.track[0].gen.base == base + 2);
    CHECK(s.track[3].step[0].note == 50);   /* drum track stays */
    gen_key_move(&s, 9);   /* D -> A: the short way is down 5 */
    CHECK(s.track[0].step[0].note == 57);
    CHECK(s.root == 9);

    /* while playing it waits for the bar line */
    setup(&s, &c);
    seq_start(&s);
    ticks(&s, 10);
    gen_key_move_at_bar(&s, 7);
    CHECK(s.root == 0 && s.key_next == 7);
    ticks(&s, BAR - 10);
    CHECK(s.root == 0);
    ticks(&s, 1);
    CHECK(s.root == 7 && s.key_next == -1);
    CHECK(note_at_bar(&c, 1) == 55 && s.track[0].step[0].note == 55);   /* C -> G: down 5 */
    seq_stop(&s);
    gen_key_move_at_bar(&s, 0);   /* stopped: at once */
    CHECK(s.root == 0 && s.track[0].step[0].note == 60);   /* G -> C: up 5 */
}

static void test_save(void) {
    Seq s, t; Cap c, c2;
    setup(&s, &c);
    s.harm = HP_VI_IV_I_V;
    s.harm_bars = 4;
    s.track[2].harm_mode = HARM_CHORD;
    s.track[0].harm_mode = HARM_OFF;
    FILE *f = tmpfile();
    seq_save(&s, f);
    rewind(f);
    setup(&t, &c2);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.harm == HP_VI_IV_I_V && t.harm_bars == 4);
    CHECK(t.track[2].harm_mode == HARM_CHORD && t.track[0].harm_mode == HARM_OFF && t.track[1].harm_mode == HARM_MOVE);

    /* NEW FROM STYLE: bass parts follow as ROOT, drum parts not at all */
    setup(&s, &c);
    int st[SEQ_TRACKS], pt[SEQ_TRACKS];
    for (int k = 0; k < SEQ_TRACKS; k++) { st[k] = k < 4 ? STYLE_HIPHOP : STYLE_KEEP; pt[k] = k % 4; }
    style_apply_tracks(&s, STYLE_HIPHOP, st, pt);
    CHECK(s.track[0].harm_mode == HARM_ROOT);   /* BASS */
    CHECK(s.track[1].harm_mode == HARM_MOVE);   /* KEYS */
    CHECK(s.track[3].harm_mode == HARM_OFF);    /* DRUMS */
}

int main(void) {
    test_names();
    test_note_maps();
    test_progression();
    test_key_move();
    test_save();
    if (fails) { fprintf(stderr, "%d harmony test(s) failed\n", fails); return 1; }
    printf("harmony tests passed\n");
    return 0;
}
