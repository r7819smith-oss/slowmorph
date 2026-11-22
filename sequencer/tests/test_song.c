/* Songs: save, read back, apply to a playing session, names, old files. */
#include "../src/engine.h"
#include "../src/drum.h"
#include "../src/gen.h"
#include "../src/song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

typedef struct { int on, off, cc123, starts; } Log;
static void rec(void *ctx, const uint8_t *m, int n) {
    Log *l = ctx; (void)n;
    if (m[0] == 0xFA) l->starts++;
    else if ((m[0] & 0xF0) == 0x90) l->on++;
    else if ((m[0] & 0xF0) == 0x80) l->off++;
    else if ((m[0] & 0xF0) == 0xB0 && m[1] == 123) l->cc123++;
}

static int same_file(const char *a, const char *b) {
    FILE *fa = fopen(a, "r"), *fb = fopen(b, "r");
    int same = fa && fb;
    while (same) {
        int ca = fgetc(fa), cb = fgetc(fb);
        if (ca != cb) same = 0;
        if (ca == EOF || cb == EOF) break;
    }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return same;
}

int main(void) {
    char dir[] = "/tmp/seq4h-song-XXXXXX", p1[256], p2[256], name[SEQ_SONG_NAME + 1], out[SEQ_SONG_NAME + 1];
    CHECK(mkdtemp(dir));
    song_path(dir, 0, p1, sizeof p1);
    song_path(dir, 7, p2, sizeof p2);
    CHECK(strstr(p1, "/seq4h-song1.pat") && strstr(p2, "/seq4h-song8.pat"));
    song_dir_of("/userdata/roms/ports/seq4h/seq4h.pat", out, sizeof out);
    char d[64];
    song_dir_of("/userdata/roms/ports/seq4h/seq4h.pat", d, sizeof d);
    CHECK(!strcmp(d, "/userdata/roms/ports/seq4h"));
    song_dir_of("seq4h.pat", d, sizeof d);
    CHECK(!strcmp(d, "."));

    /* names */
    song_clean_name(2, "song 3", name); CHECK(name[0] == 0);
    song_clean_name(2, "dawn_mix!x", name); CHECK(!strcmp(name, "DAWN MIX"));
    song_show_name(2, "", out); CHECK(!strcmp(out, "SONG 3"));

    /* a pattern file from the 4-track builds loads as before; T5-T8 keep their
     * defaults: empty, MANUAL, channels 5-8. Its modulation keeps its
     * destinations (SEND TO 4 = T1-T4 ALL, 20 = CH 16). */
    {
        static Seq o;
        seq_init(&o, NULL, NULL);
        FILE *f = fopen("tests/fixtures/mod.pat", "r");
        CHECK(f && seq_load(&o, f) == 0);
        if (f) fclose(f);
        CHECK(o.track[1].len == 12 && o.track[2].len == 7);
        CHECK(o.mod.dest[1].to == TO_T1 + 1 && o.mod.dest[2].to == TO_ALL && o.mod.dest[3].to == TO_CH1 + 15);
        for (int t = SEQ_BANK; t < SEQ_TRACKS; t++) {
            const SeqTrack *tr = &o.track[t];
            CHECK(tr->channel == t && tr->len == SEQ_DEFAULT_LEN && tr->gen.type == GEN_NONE && !tr->mute);
            CHECK(tr->part == t % SEQ_BANK && tr->type == TRACK_NOTES && seq_slots_used(&o) >= 0);
            for (int i = 0; i < SEQ_STEPS; i++) CHECK(!tr->step[i].on);
        }
        f = fopen("tests/fixtures/drums.pat", "r");
        seq_init(&o, NULL, NULL);
        CHECK(f && seq_load(&o, f) == 0);
        if (f) fclose(f);
        for (int t = SEQ_BANK; t < SEQ_TRACKS; t++) CHECK(seq_drum_hits(&o.track[t]) == 0 && o.track[t].channel == t);
        /* T5-T8 go into a song and come back */
        o.track[6].step[3].on = 1; o.track[6].step[3].note = 70; o.track[6].channel = 11; o.track[6].len = 9;
        o.track[7].type = TRACK_DRUMS; o.track[7].drum.vel[2][5] = 99; o.track[7].mute = 1;
        o.mod.dest[5].to = TO_ALL2; o.mod.dest[6].to = TO_T1 + 7;
        char p8[300];
        snprintf(p8, sizeof p8, "%s/eight.pat", dir);
        CHECK(song_save_file(&o, "EIGHT", p8) == 0);
        static Seq q;
        seq_init(&q, NULL, NULL);
        CHECK(song_read_file(p8, &q, NULL) == 0);
        CHECK(q.track[6].step[3].on && q.track[6].step[3].note == 70 && q.track[6].channel == 11 && q.track[6].len == 9);
        CHECK(q.track[7].type == TRACK_DRUMS && q.track[7].drum.vel[2][5] == 99 && q.track[7].mute);
        CHECK(q.mod.dest[5].to == TO_ALL2 && q.mod.dest[6].to == TO_T1 + 7);
        SongInfo in;
        song_info(p8, 0, &in);
        CHECK(in.used && in.type[7] == TRACK_DRUMS && in.len[6] == 9);
        remove(p8);
    }

    /* a session with a bit of everything */
    static Seq a, b, c;
    Log la = {0}, lb = {0};
    seq_init(&a, rec, &la);
    style_apply(&a, STYLE_TECHNO, 0);
    a.bpm = 97; a.root = 5; a.swing = 3;
    a.track[3].type = TRACK_DRUMS;
    a.track[2].channel = 9;
    a.track[1].mute = 1;
    a.track[0].step[4].nudge = -3;
    seq_slot_store(&a, 0, 2);
    seq_slot_set_name(&a, 0, 2, "intro");
    seq_slot_store(&a, 1, 0);
    uint8_t ch[2] = {0, 2};
    seq_chain_set(&a, 0, ch, 2, 4, 0);
    a.lane_note[1] = 40;
    CHECK(song_save_file(&a, "DAWN MIX", p1) == 0);
    CHECK(access(p1, F_OK) == 0);
    char tmp[300]; snprintf(tmp, sizeof tmp, "%s.tmp", p1);
    CHECK(access(tmp, F_OK) != 0);

    /* read back: the same file again when saved, and the name */
    CHECK(song_read_file(p1, &b, name) == 0);
    CHECK(!strcmp(name, "DAWN MIX"));
    CHECK(song_save_file(&b, name, p2) == 0);
    CHECK(same_file(p1, p2));
    CHECK(b.bpm == 97 && b.root == 5 && b.track[3].type == TRACK_DRUMS && b.track[2].channel == 9);
    CHECK(b.track[0].slot[2].used && !strcmp(b.track[0].slot[2].name, "INTRO") && b.track[0].chain_len == 2);
    CHECK(b.lane_note[1] == 40 && b.track[0].step[4].nudge == -3);

    /* info for the page */
    SongInfo in;
    song_info(p1, 0, &in);
    CHECK(in.used && !strcmp(in.name, "DAWN MIX") && in.bpm == 97 && in.slots == 2 && in.type[3] == TRACK_DRUMS && in.chain[0] == 2);
    char none[300]; song_path(dir, 4, none, sizeof none);
    song_info(none, 4, &in);
    CHECK(!in.used);
    CHECK(song_read_file(none, &c, name) == -1);

    /* apply to a different, playing session: notes released, keeps playing and its tick,
     * clock setting stays the device's, solo cleared */
    seq_init(&c, rec, &lb);
    seq_load_demo(&c);
    c.clock_out = 0;
    seq_start(&c);
    for (int i = 0; i < 1000; i++) seq_tick(&c);
    uint32_t tick = c.tick;
    int on0 = lb.on;
    b.track[0].solo = 1;
    b.clock_out = 1;
    song_apply(&c, &b);
    CHECK(lb.cc123 > 0);
    CHECK(c.playing && c.tick == tick && c.clock_out == 0);
    CHECK(c.send == rec && c.ctx == &lb);
    CHECK(c.bpm == 97 && c.track[3].type == TRACK_DRUMS && !c.track[0].solo);
    CHECK(c.track[0].slot[2].used && c.track[0].chain_len == 2);
    for (int i = 0; i < 2000; i++) seq_tick(&c);
    CHECK(lb.on > on0);   /* the song plays */

    /* a plain pattern file (no name line, as older builds write) is a song too */
    FILE *f = fopen(none, "w");
    seq_save(&a, f);
    fclose(f);
    song_info(none, 4, &in);
    CHECK(in.used && in.name[0] == 0 && in.bpm == 97);
    /* and a song file loads as a pattern file (the name line is skipped) */
    static Seq e;
    seq_init(&e, NULL, NULL);
    f = fopen(p1, "r");
    CHECK(seq_load(&e, f) == 0);
    fclose(f);
    CHECK(e.bpm == 97 && e.track[0].slot[2].used);

    /* not a pattern file */
    f = fopen(none, "w"); fputs("hello\n", f); fclose(f);
    song_info(none, 4, &in);
    CHECK(!in.used);

    remove(p1); remove(p2); remove(none); rmdir(dir);
    puts("test_song: all passed");
    return 0;
}
