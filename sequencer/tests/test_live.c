/* M > LIVE: count-in, SONG LENGTH, stop at the bar line, Song Position,
 * start mark, and saving the settings. */
#include "../src/engine.h"
#include "../src/gen.h"
#include "../src/live.h"
#include "../src/song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
#define BAR (SEQ_PPQN * 4)

/* Every message, with the tick it went out on. */
typedef struct { uint8_t m[3]; int n; uint32_t at; } Msg;
typedef struct { Msg msg[20000]; int n; uint32_t now; } Log;
static void rec(void *ctx, const uint8_t *m, int n) {
    Log *l = ctx;
    if (l->n >= 20000) return;
    Msg *x = &l->msg[l->n++];
    memcpy(x->m, m, (size_t)(n > 3 ? 3 : n));
    x->n = n;
    x->at = l->now;
}
static void run(Seq *s, Log *l, int ticks) {
    for (int i = 0; i < ticks; i++) { seq_tick(s); l->now++; }
}
static int first(const Log *l, uint8_t status, int from) {   /* index of the first such message, or -1 */
    for (int i = from; i < l->n; i++)
        if (status >= 0xF0 ? l->msg[i].m[0] == status : (l->msg[i].m[0] & 0xF0) == status) return i;
    return -1;
}
static int count(const Log *l, uint8_t status) {
    int c = 0;
    for (int i = 0; i < l->n; i++) c += status >= 0xF0 ? l->msg[i].m[0] == status : (l->msg[i].m[0] & 0xF0) == status;
    return c;
}

static Seq s, t;
static Log lg;

static void fresh(void) {
    memset(&lg, 0, sizeof lg);
    seq_init(&s, rec, &lg);
    seq_load_demo(&s);
}

int main(void) {
    /* No count-in: Start, then notes on tick 0. */
    fresh();
    seq_start(&s);
    run(&s, &lg, BAR);
    CHECK(lg.msg[0].m[0] == 0xFA);
    CHECK(first(&lg, 0xF2, 0) < 0);   /* no Song Position unless asked */
    CHECK(lg.msg[first(&lg, 0x90, 0)].at == 0);

    /* One bar of count-in: clock all through, no notes, Start after 384 ticks. */
    fresh();
    s.live.count_in = 1;
    s.live.click = 0;
    seq_start(&s);
    CHECK(s.playing && first(&lg, 0xFA, 0) < 0);
    int bars, beats;
    CHECK(live_counting(&s, &bars, &beats) && bars == 0 && beats == 4);
    run(&s, &lg, BAR - 1);
    CHECK(first(&lg, 0xFA, 0) < 0 && first(&lg, 0x90, 0) < 0);
    CHECK(count(&lg, 0xF8) == 96);   /* 24 PPQN clock for the whole bar */
    run(&s, &lg, 1);
    int st = first(&lg, 0xFA, 0);
    CHECK(st >= 0 && lg.msg[st].at == BAR - 1);
    CHECK(!live_counting(&s, &bars, &beats));
    run(&s, &lg, 4);
    int on = first(&lg, 0x90, 0);
    CHECK(on > st && lg.msg[on].at == BAR);   /* bar 1 starts straight after Start */
    {   /* the clock keeps an even 4-tick spacing across the end of the count-in */
        uint32_t last = 0; int seen = 0;
        for (int i = 0; i < lg.n; i++) {
            if (lg.msg[i].m[0] != 0xF8) continue;
            if (seen) CHECK(lg.msg[i].at - last == 4);
            last = lg.msg[i].at; seen = 1;
        }
    }

    /* The click: a note each beat on the drum track, louder on beat 1, all off before Start. */
    fresh();
    s.track[3].type = TRACK_DRUMS;
    s.live.count_in = 2;
    s.live.click = 1;
    seq_start(&s);
    run(&s, &lg, 2 * BAR);
    CHECK(count(&lg, 0x90) == 8);
    on = first(&lg, 0x90, 0);
    CHECK((lg.msg[on].m[0] & 15) == s.track[3].channel && lg.msg[on].m[2] == 120);
    CHECK(lg.msg[first(&lg, 0x90, on + 1)].m[2] == 75);
    CHECK(count(&lg, 0x80) == 8);
    /* stopping during the count-in: the click stops, no Start was sent */
    fresh();
    s.live.count_in = 1; s.live.click = 1;
    seq_start(&s);
    run(&s, &lg, 5);
    seq_stop(&s);
    CHECK(count(&lg, 0x90) == 1 && count(&lg, 0x80) == 1 && first(&lg, 0xFA, 0) < 0 && !s.live.count_left);

    /* Song Position Pointer 0 right before Start. */
    fresh();
    s.live.spp = 1;
    seq_start(&s);
    CHECK(lg.n >= 2 && lg.msg[0].m[0] == 0xF2 && lg.msg[0].m[1] == 0 && lg.msg[0].m[2] == 0 && lg.msg[1].m[0] == 0xFA);
    /* ...but nothing when Start/Stop is off */
    fresh();
    s.live.spp = 1;
    seq_set_sync(&s, 1, 0);
    seq_start(&s);
    CHECK(first(&lg, 0xF2, 0) < 0 && first(&lg, 0xFA, 0) < 0);

    /* SONG LENGTH 2: stops at the bar 3 line, after exactly two bars. */
    fresh();
    s.live.length = 2;
    seq_start(&s);
    run(&s, &lg, 3 * BAR);
    CHECK(!s.playing);
    int sp = first(&lg, 0xFC, 0);
    CHECK(sp >= 0 && lg.msg[sp].at == 2 * BAR);
    for (int i = 0; i < lg.n; i++) if ((lg.msg[i].m[0] & 0xF0) == 0x90) CHECK(lg.msg[i].at < 2 * BAR);
    /* ...counting from Start, after a count-in too */
    fresh();
    s.live.length = 1; s.live.count_in = 1;
    seq_start(&s);
    run(&s, &lg, 3 * BAR);
    CHECK(lg.msg[first(&lg, 0xFC, 0)].at == 2 * BAR);

    /* STOP AT BAR: Start pressed mid-bar waits for the bar line; pressed again it stops now. */
    fresh();
    s.live.stop_mode = STOP_AT_BAR;
    seq_start(&s);
    run(&s, &lg, BAR + 100);
    CHECK(live_stop_request(&s) == 1 && s.playing);
    run(&s, &lg, BAR);
    CHECK(!s.playing && lg.msg[first(&lg, 0xFC, 0)].at == 2 * BAR);
    fresh();
    s.live.stop_mode = STOP_AT_BAR;
    seq_start(&s);
    run(&s, &lg, 50);
    CHECK(live_stop_request(&s) == 1);
    CHECK(live_stop_request(&s) == 0 && !s.playing);
    /* STOP NOW */
    fresh();
    seq_start(&s);
    run(&s, &lg, 50);
    CHECK(live_stop_request(&s) == 0 && !s.playing);

    /* Position: bar and beat. */
    fresh();
    seq_start(&s);
    run(&s, &lg, BAR + 2 * SEQ_PPQN + 5);
    live_position(&s, &bars, &beats);
    CHECK(bars == 2 && beats == 3);

    /* START FROM MARK: two takes of an evolving pattern send the same notes. */
    {
        static Log a, b;
        Seq *x = &s;
        memset(&a, 0, sizeof a);
        seq_init(x, rec, &a);
        style_apply(x, STYLE_AMBIENT, 0);
        for (int k = 0; k < SEQ_TRACKS; k++) { x->track[k].evo.every = 1; x->track[k].evo.amount = 60; }
        live_mark(x);
        x->live.from_mark = 1;
        seq_start(x);
        run(x, &a, 16 * BAR);
        seq_stop(x);
        CHECK(memcmp(x->track[0].step, x->live.mark[0].step, sizeof x->track[0].step) != 0);   /* it did evolve */
        memset(&b, 0, sizeof b);
        x->ctx = &b;
        seq_start(x);
        run(x, &b, 16 * BAR);
        seq_stop(x);
        CHECK(a.n == b.n && a.n > 100);
        for (int i = 0; i < a.n; i++) CHECK(a.msg[i].at == b.msg[i].at && !memcmp(a.msg[i].m, b.msg[i].m, 3));
        CHECK(live_restore_mark(x) == 0);
        CHECK(!memcmp(x->track[0].step, x->live.mark[0].step, sizeof x->track[0].step));
        seq_init(&t, NULL, NULL);
        CHECK(live_restore_mark(&t) == -1);
    }

    /* Settings are saved and read back; a song load keeps the session's. */
    {
        fresh();
        s.live = (Live){0};
        s.live.click_note = -1;
        s.live.count_in = 2; s.live.click = 1; s.live.length = 64; s.live.stop_mode = STOP_AT_BAR; s.live.spp = 1; s.live.from_mark = 1;
        FILE *f = tmpfile();
        seq_save(&s, f);
        rewind(f);
        seq_init(&t, NULL, NULL);
        CHECK(seq_load(&t, f) == 0);
        fclose(f);
        CHECK(t.live.count_in == 2 && t.live.click == 1 && t.live.length == 64 && t.live.stop_mode == STOP_AT_BAR
              && t.live.spp == 1 && t.live.from_mark == 1);
        seq_init(&t, NULL, NULL);   /* count-in off */
        song_apply(&s, &t);
        CHECK(s.live.count_in == 2 && s.live.length == 64);
        live_load_line(&t, "lv 3 1 5000 1 0 0\n");   /* out of range: 4 bars, 999 */
        CHECK(t.live.count_in == 4 && t.live.length == LIVE_MAX_LENGTH);
    }

    puts("live tests passed");
    return 0;
}
