/* Drum tracks: lane playback, DRUM MAP, slots, save/load, generator and
 * evolution, audition. */
#include "../src/drum.h"
#include "../src/engine.h"
#include "../src/gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXEV 20000
typedef struct { uint32_t tick; uint8_t m[3]; } Ev;
typedef struct { Ev ev[MAXEV]; int n; uint32_t *now; } Log;
static Log L;

static void rec(void *ctx, const uint8_t *m, int n) {
    Log *l = ctx;
    if (n < 3 || l->n >= MAXEV) return;
    l->ev[l->n].tick = *l->now;
    memcpy(l->ev[l->n].m, m, 3);
    l->n++;
}

static void run(Seq *s, int ticks) { for (int i = 0; i < ticks; i++) seq_tick(s); }

/* note-ons (or note-offs with on = 0) of `note` on channel ch */
static int count(int on, int ch, int note) {
    int c = 0;
    for (int i = 0; i < L.n; i++) {
        const uint8_t *m = L.ev[i].m;
        int is_on = (m[0] & 0xF0) == 0x90 && m[2], is_off = (m[0] & 0xF0) == 0x80 || ((m[0] & 0xF0) == 0x90 && !m[2]);
        if ((m[0] & 0x0F) == ch && m[1] == note && (on ? is_on : is_off)) c++;
    }
    return c;
}

static void fresh(Seq *s) {
    memset(&L, 0, sizeof L);
    seq_init(s, rec, &L);
    L.now = &s->tick;
}

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

static int test_play(void) {
    static Seq s;
    fresh(&s);
    CHECK(s.lane_note[0] == 36 && s.lane_note[2] == 42 && !strcmp(s.lane_name[2], "HAT CL"));
    CHECK(s.lane_note[7] == 56 && !strcmp(s.lane_name[7], "BELL") && seq_drum_map_is(&s, DRUM_FELUCCA));
    seq_track_type(&s, 3, TRACK_DRUMS);
    SeqTrack *tr = &s.track[3];
    tr->step[0].on = 1;              /* the note steps stay, but a drum track doesn't play them */
    seq_drum_toggle(&s, 3, 0, 0);    /* kick and closed hat together on step 1 */
    seq_drum_toggle(&s, 3, 2, 0);
    seq_drum_toggle(&s, 3, 2, 8);
    seq_drum_adjust(&s, 3, 2, 8, -40, 0);
    CHECK(tr->drum.vel[2][8] == 60 && tr->drum.prob[2][8] == 100);
    seq_drum_toggle(&s, 3, 5, 4);    /* a rim that never plays */
    seq_drum_adjust(&s, 3, 5, 4, 0, -100);
    seq_start(&s);
    run(&s, 384);                    /* one bar */
    CHECK(count(1, 3, 36) == 1 && count(1, 3, 42) == 2);
    CHECK(count(1, 3, 37) == 0);
    CHECK(count(1, 3, tr->step[0].note) == 0);
    CHECK(count(0, 3, 36) == 1 && count(0, 3, 42) == 2);   /* every hit released */
    /* both step-1 notes start on tick 0, the hat's offs come SEQ_DRUM_GATE later */
    for (int i = 0; i < L.n; i++)
        if ((L.ev[i].m[0] & 0xF0) == 0x80 && L.ev[i].m[1] == 36) CHECK(L.ev[i].tick == SEQ_DRUM_GATE);
    for (int i = 0; i < L.n; i++)
        if ((L.ev[i].m[0] & 0xF0) == 0x90 && L.ev[i].m[1] == 42 && L.ev[i].m[2] == 60) CHECK(L.ev[i].tick == 8 * SEQ_TICKS_PER_STEP);
    /* the DRUM MAP is read as notes are sent */
    seq_lane_note(&s, 0, 1);
    CHECK(s.lane_note[0] == 37);
    run(&s, 384);
    CHECK(count(1, 3, 37) == 1);
    /* stop releases a sounding hit */
    run(&s, 2);
    CHECK(tr->drum_down != 0);
    seq_stop(&s);
    CHECK(tr->drum_down == 0);
    /* back to notes: the step plays again, the lanes are kept */
    seq_track_type(&s, 3, TRACK_NOTES);
    L.n = 0;
    seq_start(&s);
    run(&s, 384);
    CHECK(count(1, 3, tr->step[0].note) == 1 && count(1, 3, 37) == 0);
    CHECK(tr->drum.vel[0][0] == 100);
    seq_stop(&s);
    return 0;
}

static int test_shuffle_and_mute(void) {
    static Seq s;
    fresh(&s);
    seq_track_type(&s, 0, TRACK_DRUMS);
    for (int i = 0; i < 16; i++) seq_drum_toggle(&s, 0, 2, i);   /* hats on every 16th */
    seq_track_offgrid(&s, 0, 1);
    seq_track_shuffle(&s, 0, 6);
    seq_start(&s);
    run(&s, 384 * 4);
    CHECK(count(1, 0, 42) >= 63 && count(1, 0, 42) <= 65);   /* an early step 1 of the next bar may already be in */
    for (int i = 0; i < L.n; i++)
        if ((L.ev[i].m[0] & 0xF0) == 0x90) {
            int ph = (int)(L.ev[i].tick % SEQ_TICKS_PER_STEP);
            CHECK(ph != 0);   /* never on the grid */
        }
    seq_track_mute_toggle(&s, 0);
    int before = count(1, 0, 42);
    run(&s, 384);
    CHECK(count(1, 0, 42) == before);
    seq_stop(&s);
    return 0;
}

static int test_save_load(void) {
    static Seq s, t;
    fresh(&s);
    seq_track_type(&s, 2, TRACK_DRUMS);
    s.track[2].drum_busy = 70;
    seq_drum_lock(&s, 2, 1);
    seq_drum_toggle(&s, 2, 0, 0);
    seq_drum_toggle(&s, 2, 7, 31);
    seq_drum_adjust(&s, 2, 7, 31, -27, -55);
    seq_slot_store(&s, 2, 3);
    seq_drum_toggle(&s, 2, 4, 6);    /* differs from the slot */
    gen_set_anchor(&s, 2);
    seq_lane_note(&s, 3, 2);
    seq_lane_set_name(&s, 3, "pad hit");
    FILE *f = tmpfile();
    seq_save(&s, f);
    rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    const SeqTrack *a = &s.track[2], *b = &t.track[2];
    CHECK(b->type == TRACK_DRUMS && b->drum_busy == 70 && b->drum_lock == 2);
    CHECK(!memcmp(&a->drum, &b->drum, sizeof a->drum));
    CHECK(!memcmp(&a->drum_anchor, &b->drum_anchor, sizeof a->drum));
    CHECK(!memcmp(&a->slot[3].drum, &b->slot[3].drum, sizeof a->drum));
    CHECK(b->drum.vel[7][31] == 73 && b->drum.prob[7][31] == 45);
    CHECK(t.lane_note[3] == 48 && !strcmp(t.lane_name[3], "PAD HIT"));
    CHECK(t.track[0].type == TRACK_NOTES);
    /* slots keep the lanes */
    seq_slot_recall(&t, 2, 3);
    CHECK(t.track[2].drum.vel[4][6] == 0 && t.track[2].drum.vel[0][0] == 100);
    /* a file from an older build: no drum lines, GM map, note tracks */
    static const char old[] = "seq4h 1\nbpm 90\nt 0 0 16 0\ns 0 0 1 60 100 50 100 0\n";
    f = tmpfile();
    fputs(old, f);
    rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.bpm == 90 && t.track[0].step[0].on && t.track[0].type == TRACK_NOTES);
    CHECK(t.lane_note[0] == 36 && !strcmp(t.lane_name[7], "BELL") && t.track[0].drum_busy == 50);
    /* names: blank gives the GM name back, odd characters become spaces */
    seq_lane_set_name(&t, 1, "  ");
    CHECK(!strcmp(t.lane_name[1], "SNARE"));
    seq_lane_set_name(&t, 1, "sn*re2long");
    CHECK(!strcmp(t.lane_name[1], "SN RE2LO"));
    return 0;
}

static int test_map(void) {
    static Seq s;
    seq_init(&s, NULL, NULL);
    /* what Felucca plays: its 8 lanes' own notes at their own pitch */
    static const uint8_t lane[8] = {36, 38, 39, 42, 46, 45, 37, 56};
    static const char *name[8] = {"KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "RIM", "BELL"};
    for (int k = 0; k < 8; k++) {
        int st = 99;
        CHECK(!strcmp(seq_felucca_sound(lane[k], &st), name[k]) && st == 0);
    }
    int st;
    CHECK(!strcmp(seq_felucca_sound(41, &st), "TOM") && st == -7);
    CHECK(!strcmp(seq_felucca_sound(49, NULL), "CYMBAL"));
    CHECK(!strcmp(seq_felucca_sound(24, NULL), "KICK"));      /* below 35: its octave of 36..47 */
    CHECK(!strcmp(seq_felucca_sound(127, NULL), "TOM"));      /* above 81: 36 + (127 - 36) % 12 = 43, a tom */
    return 0;
}

static int test_map_next(void) {
    /* next sound up from the kick, at its own pitch: RIM 37, SNARE 38, CLAP 39, HAT CL 42 ... */
    CHECK(seq_felucca_next_sound(36, 1) == 37);
    CHECK(seq_felucca_next_sound(37, 1) == 38);
    CHECK(seq_felucca_next_sound(39, 1) == 42);
    CHECK(seq_felucca_next_sound(42, -1) == 39);
    CHECK(seq_felucca_next_sound(36, -1) == 36);   /* nothing below the kick */
    CHECK(seq_felucca_next_sound(0, 1) == 37);     /* from far below: starts at 35 */
    return 0;
}

static int test_map_migrate(void) {
    static Seq t;
    /* a PR 23 file: the GM map, no version line -> FELUCCA */
    static const char gm[] = "seq4h 1\ndm 0 36 KICK\ndm 1 38 SNARE\ndm 2 42 CL_HAT\ndm 3 46 OP_HAT\n"
                             "dm 4 39 CLAP\ndm 5 37 RIM\ndm 6 45 TOM\ndm 7 49 CRASH\n";
    FILE *f = tmpfile();
    fputs(gm, f); rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(seq_drum_map_is(&t, DRUM_FELUCCA));
    /* a map someone changed stays */
    f = tmpfile();
    fputs(gm, f); fputs("dm 0 35 KICK2\n", f); rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.lane_note[0] == 35 && t.lane_note[7] == 49 && !strcmp(t.lane_name[0], "KICK2"));
    /* GM chosen in a new build (version 2) stays GM after a save and load */
    seq_drum_map_preset(&t, DRUM_GM);
    f = tmpfile();
    seq_save(&t, f); rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(seq_drum_map_is(&t, DRUM_GM));
    return 0;
}

static int lane_hits(const SeqTrack *tr, int l) {
    int n = 0;
    for (int i = 0; i < tr->len; i++) n += tr->drum.vel[l][i] != 0;
    return n;
}

static int test_generate(void) {
    static Seq s;
    fresh(&s);
    s.track[0].style = STYLE_TECHNO;   /* a drum track rolls its own style's kit */
    seq_track_type(&s, 0, TRACK_DRUMS);
    gen_roll(&s, 0);
    SeqTrack *tr = &s.track[0];
    for (int i = 0; i < 16; i++) CHECK((tr->drum.vel[0][i] != 0) == (i % 4 == 0));   /* four on the floor */
    CHECK(tr->drum.vel[4][4] && tr->drum.vel[4][12] && lane_hits(tr, 4) == 2);    /* clap on 2 and 4 */
    /* a locked lane is left alone by rolls, evolution and CLEAR */
    seq_drum_lock(&s, 0, 0);
    memset(tr->drum.vel[0], 0, sizeof tr->drum.vel[0]);
    tr->drum.vel[0][3] = 99;
    gen_roll(&s, 0);
    CHECK(tr->drum.vel[0][3] == 99 && lane_hits(tr, 0) == 1);
    gen_transform(&s, 0, XF_CLEAR, 0);
    CHECK(tr->drum.vel[0][3] == 99 && seq_drum_hits(tr) == 1);
    /* ambient is sparse; BUSY makes it busier */
    tr->style = STYLE_AMBIENT;
    tr->drum_lock = 0;
    tr->drum_busy = 0;
    gen_roll(&s, 0);
    int sparse = seq_drum_hits(tr);
    tr->drum_busy = 100;
    gen_roll(&s, 0);
    CHECK(sparse >= 1 && seq_drum_hits(tr) > sparse && seq_drum_hits(tr) <= 24);
    /* RHYTHM evolution moves hits, it doesn't add or remove them */
    tr->evo = (EvolveSettings){1, 100, EVO_RHYTHM, 0, 0, 0};
    int counts[SEQ_LANES];
    for (int l = 0; l < SEQ_LANES; l++) counts[l] = lane_hits(tr, l);
    for (int bar = 0; bar < 50; bar++) gen_evolve_bar(&s, 0);
    for (int l = 0; l < SEQ_LANES; l++) CHECK(lane_hits(tr, l) == counts[l]);
    /* the anchor pull brings the lanes home */
    gen_set_anchor(&s, 0);
    SeqDrums home = tr->drum;
    tr->evo = (EvolveSettings){1, 100, EVO_VEL | EVO_PROB | EVO_ROTATE, 0, 0, 0};
    for (int bar = 0; bar < 8; bar++) gen_evolve_bar(&s, 0);
    CHECK(memcmp(&home, &tr->drum, sizeof home) != 0);
    tr->evo = (EvolveSettings){1, 0, 0, 100, 0, 0};
    gen_evolve_bar(&s, 0);
    for (int l = 0; l < SEQ_LANES; l++) CHECK(!memcmp(home.vel[l], tr->drum.vel[l], tr->len));
    /* transforms with no meaning on drums change nothing */
    gen_transform(&s, 0, XF_INVERT, 0);
    for (int l = 0; l < SEQ_LANES; l++) CHECK(!memcmp(home.vel[l], tr->drum.vel[l], tr->len));
    /* REVERSE twice is where it started */
    gen_transform(&s, 0, XF_REVERSE, 0);
    gen_transform(&s, 0, XF_REVERSE, 0);
    for (int l = 0; l < SEQ_LANES; l++) CHECK(!memcmp(home.vel[l], tr->drum.vel[l], tr->len));
    /* NEW FROM STYLE keeps a drum track a drum track, with a new kit pattern */
    style_apply(&s, STYLE_TECHNO, 0);
    CHECK(tr->type == TRACK_DRUMS && tr->drum.vel[0][0] && tr->drum.vel[0][4]);
    style_apply(&s, STYLE_BLANK, 0);
    CHECK(tr->type == TRACK_DRUMS && seq_drum_hits(tr) == 0);
    return 0;
}

static int test_audition(void) {
    static Seq s;
    fresh(&s);
    seq_audition(&s, 9, 38, 90);
    CHECK(count(1, 9, 38) == 1 && count(0, 9, 38) == 0);
    run(&s, SEQ_PPQN / 2 - 1);
    CHECK(count(0, 9, 38) == 0);
    run(&s, 1);
    CHECK(count(0, 9, 38) == 1);
    /* a second audition ends the first straight away */
    seq_audition(&s, 9, 40, 90);
    seq_audition(&s, 9, 41, 90);
    CHECK(count(0, 9, 40) == 1);
    run(&s, SEQ_PPQN);
    CHECK(count(0, 9, 41) == 1);
    return 0;
}

/* Lane mute and solo: muted lanes are silent, while any lane is soloed only
 * soloed lanes play, the track's own mute is on top; mutes are saved, solos not. */
static int test_lane_mute_solo(void) {
    static Seq s, t;
    fresh(&s);
    seq_track_type(&s, 3, TRACK_DRUMS);
    SeqTrack *tr = &s.track[3];
    seq_drum_toggle(&s, 3, 0, 0);    /* kick, snare, closed hat on step 1 */
    seq_drum_toggle(&s, 3, 1, 0);
    seq_drum_toggle(&s, 3, 2, 0);
    int k = s.lane_note[0], sn = s.lane_note[1], h = s.lane_note[2];
    seq_lane_mute_toggle(&s, 3, 1);
    CHECK(!seq_lane_audible(tr, 1) && seq_lane_audible(tr, 0));
    seq_start(&s);
    run(&s, 384);
    CHECK(count(1, 3, k) == 1 && count(1, 3, sn) == 0 && count(1, 3, h) == 1);
    seq_lane_solo_toggle(&s, 3, 2);  /* solo the hat: kick goes quiet too */
    run(&s, 384);
    CHECK(count(1, 3, k) == 1 && count(1, 3, sn) == 0 && count(1, 3, h) == 2);
    seq_lane_solo_toggle(&s, 3, 1);  /* soloing a muted lane: still muted */
    run(&s, 384);
    CHECK(count(1, 3, sn) == 0 && count(1, 3, h) == 3 && count(1, 3, k) == 1);
    seq_lane_solo_toggle(&s, 3, 1);
    seq_lane_solo_toggle(&s, 3, 2);  /* no solo: back to all but the muted snare */
    run(&s, 384);
    CHECK(count(1, 3, k) == 2 && count(1, 3, h) == 4 && count(1, 3, sn) == 0);
    /* a sounding hit stops when its lane is muted */
    seq_stop(&s);
    seq_start(&s);
    run(&s, 2);
    CHECK(tr->drum_down & 1);
    seq_lane_mute_toggle(&s, 3, 0);
    CHECK(!(tr->drum_down & 1) && count(0, 3, k) == count(1, 3, k));
    seq_stop(&s);
    /* saved: the mutes, not the solos */
    seq_lane_solo_toggle(&s, 3, 4);
    FILE *f = tmpfile();
    seq_save(&s, f);
    rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.track[3].drum_mute == 3 && t.track[3].drum_solo == 0);
    /* an older "ty" line without mutes */
    static const char old[] = "seq4h 1\nty 3 1 0 50\n";
    f = tmpfile();
    fputs(old, f);
    rewind(f);
    seq_init(&t, NULL, NULL);
    CHECK(seq_load(&t, f) == 0);
    fclose(f);
    CHECK(t.track[3].type == TRACK_DRUMS && t.track[3].drum_mute == 0);
    return 0;
}

int main(void) {
    if (test_play() || test_shuffle_and_mute() || test_save_load() || test_map() || test_map_next() || test_map_migrate() || test_generate() || test_audition() || test_lane_mute_solo()) return 1;
    printf("drum tests passed\n");
    return 0;
}
