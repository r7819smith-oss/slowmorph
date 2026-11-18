#include "../src/engine.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { int on[SEQ_TRACKS]; int off[SEQ_TRACKS]; uint32_t first_on[SEQ_TRACKS];
                 uint32_t first_off[SEQ_TRACKS]; int clocks, starts, stops, cc123; uint32_t *now; } Log;

static void rec(void *ctx, const uint8_t *m, int n) {
    Log *l = ctx; (void)n;
    uint8_t st = m[0] & 0xF0, ch = m[0] & 0x0F;
    if (m[0] == 0xF8) l->clocks++;
    else if (m[0] == 0xFA) l->starts++;
    else if (m[0] == 0xFC) l->stops++;
    else if (st == 0x90) { if (!l->on[ch]++) l->first_on[ch] = *l->now; }
    else if (st == 0x80) { if (!l->off[ch]++) l->first_off[ch] = *l->now; }
    else if (st == 0xB0 && m[1] == 123) l->cc123++;
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
    seq_step_adjust(&s, 0, 0, 200, 0, 0, 0);
    CHECK(s.track[0].step[0].note == 127 && s.track[0].step[0].on);
    seq_step_adjust(&s, 0, 1, 0, -500, -500, -500);
    CHECK(s.track[0].step[1].vel == 1 && s.track[0].step[1].gate == 5 && s.track[0].step[1].prob == 0);
    seq_track_len(&s, 0, -99); CHECK(s.track[0].len == 1);
    seq_adjust_bpm(&s, 999); CHECK(s.bpm == 300);

    /* note names */
    char nm[8];
    seq_note_name(60, nm); CHECK(strcmp(nm, "C4") == 0);
    seq_note_name(61, nm); CHECK(strcmp(nm, "C#4") == 0);

    /* save / load round trip */
    Seq a, b;
    seq_init(&a, NULL, NULL); seq_load_demo(&a); a.bpm = 97; a.swing = 4; a.track[2].len = 11; a.track[3].mute = 1;
    FILE *f = tmpfile(); seq_save(&a, f); rewind(f);
    seq_init(&b, NULL, NULL);
    CHECK(seq_load(&b, f) == 0); fclose(f);
    CHECK(b.bpm == 97 && b.swing == 4 && b.track[2].len == 11 && b.track[3].mute == 1);
    CHECK(memcmp(a.track[2].step, b.track[2].step, sizeof a.track[2].step) == 0);
    f = tmpfile(); fputs("garbage\n", f); rewind(f);
    CHECK(seq_load(&b, f) != 0); fclose(f);

    puts("engine tests passed");
    return 0;
}
