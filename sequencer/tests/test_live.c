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

static void swap_song(void *seq, void *song) { song_apply(seq, song); free(song); }
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

    /* Scenes: store, recall now while stopped, queue to the switch bar while playing. */
    {
        fresh();
        live_scene_store(&s, 2);
        CHECK(live_scene_tracks(&s, 2) == SEQ_TRACKS && live_scene_tracks(&s, 3) == 0);
        SeqStep keep = s.track[0].step[0];
        s.track[0].step[0].on = !keep.on;
        s.track[1].chain_len = 2;   /* a chained track keeps its chain */
        s.track[1].step[5].note = 99;
        CHECK(live_queue_scene(&s, 2) == 0);   /* stopped: now */
        CHECK(s.track[0].step[0].on == keep.on && s.track[1].step[5].note == 99 && s.live.scene == 2);
        s.track[1].chain_len = 0;
        s.track[0].step[0].on = !keep.on;
        live_set_quant(&s, 2);
        seq_start(&s);
        run(&s, &lg, BAR / 2);
        CHECK(live_queue_scene(&s, 2) == 1 && s.live.scene_q == 2);
        int bars, beats;
        CHECK(live_switch_in(&s, &bars, &beats) && bars == 1 && beats == 2);   /* 1.5 bars to the bar-3 line */
        run(&s, &lg, BAR);   /* bar 2 line: not a multiple of 2 bars */
        CHECK(s.track[0].step[0].on != keep.on && s.live.scene_q == 2);
        run(&s, &lg, BAR / 2 + 1);
        CHECK(s.track[0].step[0].on == keep.on && s.live.scene_q == -1 && !live_switch_in(&s, &bars, &beats));
        CHECK(live_queue_scene(&s, 1) == 1 && live_queue_scene(&s, -1) == 0 && s.live.scene_q == -1);
        live_set_quant(&s, 3); CHECK(s.live.quant == 4);
    }

    /* A queued song comes in at the switch bar, in time, and frees itself. */
    {
        fresh();
        live_set_quant(&s, 1);
        seq_start(&s);
        run(&s, &lg, 10);
        Seq *song = malloc(sizeof *song);
        seq_init(song, NULL, NULL);
        song->bpm = 77;
        song->live.count_in = 4;   /* a song's own live settings are ignored */
        live_queue_song(&s, 5, song, swap_song);
        run(&s, &lg, BAR - 11);
        CHECK(s.bpm != 77 && s.live.song_q == 5);
        run(&s, &lg, 2);
        CHECK(s.bpm == 77 && s.live.song_now == 5 && s.live.song_q == -1 && !s.live.song_next && s.playing);
        CHECK(s.live.count_in == 0 && s.tick == BAR + 1);
        Seq *other = malloc(sizeof *other);
        seq_init(other, NULL, NULL);
        live_queue_song(&s, 2, other, swap_song);
        live_queue_song(&s, -1, NULL, NULL);   /* cancel: freed (ASan checks) */
        CHECK(s.live.song_q == -1 && !s.live.song_next);
    }

    /* DENSITY: thinner drops notes, busier adds ghost notes; 0 is the pattern exactly. */
    {
        int base, thin, busy;
        fresh(); seq_start(&s); run(&s, &lg, 8 * BAR); base = count(&lg, 0x90);
        fresh(); for (int k = 0; k < SEQ_TRACKS; k++) live_density(&s, k, -80);
        seq_start(&s); run(&s, &lg, 8 * BAR); thin = count(&lg, 0x90);
        fresh(); for (int k = 0; k < SEQ_TRACKS; k++) live_density(&s, k, 100);
        CHECK(s.live.density[0] == 100);
        seq_start(&s); run(&s, &lg, 8 * BAR); busy = count(&lg, 0x90);
        CHECK(thin < base * 2 / 3 && busy > base);
        Seq d; seq_init(&d, NULL, NULL); seq_load_demo(&d);
        CHECK(!memcmp(s.track[2].step, d.track[2].step, sizeof d.track[2].step));
        /* drums: hats get ghosts, the kick does not */
        fresh();
        s.track[3].type = TRACK_DRUMS;
        memset(s.track[3].drum.vel, 0, sizeof s.track[3].drum.vel);
        s.track[3].drum.vel[0][0] = 100;   /* lane 1 KICK */
        s.track[3].drum.vel[3][2] = 100;   /* lane 4 HAT */
        live_density(&s, 3, 100);
        for (int k = 0; k < 3; k++) s.track[k].mute = 1;
        seq_start(&s); run(&s, &lg, 8 * BAR);
        int kick = 0, hat = 0;
        for (int i = 0; i < lg.n; i++) if ((lg.msg[i].m[0] & 0xF0) == 0x90) { kick += lg.msg[i].m[1] == s.lane_note[0]; hat += lg.msg[i].m[1] == s.lane_note[3]; }
        CHECK(kick == 8 && hat > 12);
        /* saved */
        FILE *f = tmpfile();
        live_set_quant(&s, 8);
        seq_save(&s, f); rewind(f);
        seq_init(&t, NULL, NULL);
        CHECK(seq_load(&t, f) == 0);
        fclose(f);
        CHECK(t.live.density[3] == 100 && t.live.density[0] == 0 && t.live.quant == 8);
    }

    /* FILL: a drum track rolls its snare over the second half of each bar while held. */
    {
        fresh();
        s.track[3].type = TRACK_DRUMS;
        memset(s.track[3].drum.vel, 0, sizeof s.track[3].drum.vel);
        s.track[3].drum.vel[0][0] = 100;   /* KICK on step 1 only */
        for (int k = 0; k < 3; k++) s.track[k].mute = 1;
        live_fill(&s, 3, 1);
        CHECK(live_busy(&s, 3) && !live_busy(&s, 2));
        seq_start(&s); run(&s, &lg, BAR);
        int sn = 0, hh = 0, kick = 0, last = 0, rising = 1;
        for (int i = 0; i < lg.n; i++) {
            if ((lg.msg[i].m[0] & 0xF0) != 0x90) continue;
            if (lg.msg[i].m[1] == s.lane_note[1]) { sn++; CHECK(lg.msg[i].at >= BAR / 2); rising &= lg.msg[i].m[2] > last; last = lg.msg[i].m[2]; }
            hh += lg.msg[i].m[1] == s.lane_note[2];
            kick += lg.msg[i].m[1] == s.lane_note[0];
        }
        CHECK(sn == 8 && hh == 16 && kick == 1 && rising);
        live_fill(&s, 3, 0);
        int before = count(&lg, 0x90);
        run(&s, &lg, BAR);
        CHECK(count(&lg, 0x90) - before == 1);   /* back to the kick alone */
        /* a note track plays every step, the last beat an octave up */
        fresh();
        for (int k = 1; k < 4; k++) s.track[k].mute = 1;
        memset(s.track[0].step, 0, sizeof s.track[0].step);
        for (int i = 0; i < SEQ_STEPS; i++) s.track[0].step[i].note = 40;
        s.track[0].step[0] = (SeqStep){1, 48, 100, 50, 100, 0, 0, 0, {0}};
        live_fill(&s, 0, 1);
        seq_start(&s); run(&s, &lg, BAR);
        int notes = 0, high = 0;
        for (int i = 0; i < lg.n; i++) if ((lg.msg[i].m[0] & 0xF0) == 0x90) { notes++; high += lg.msg[i].m[1] == 60; CHECK(lg.msg[i].m[1] == 48 || lg.msg[i].m[1] == 60); }
        CHECK(notes == 16 && high == 4);
    }

    /* ARRANGE: sections switch scenes on their bar lines, fill their last bar, loop or stop. */
    {
        fresh();
        s.track[3].type = TRACK_DRUMS;
        live_scene_store(&s, 0);                 /* scene 1: the demo */
        for (int k = 0; k < SEQ_TRACKS; k++) memset(s.track[k].step, 0, sizeof s.track[k].step);
        memset(s.track[3].drum.vel, 0, sizeof s.track[3].drum.vel);
        live_scene_store(&s, 1);                 /* scene 2: silence */
        s.arr.on = 1;
        live_arr_insert(&s, 0, (ArrSection){0, 2, ARR_FILL_DRUMS});
        live_arr_insert(&s, 1, (ArrSection){1, 1, ARR_FILL_OFF});
        live_arr_insert(&s, 1, (ArrSection){0, 200, 9});   /* clamped: 64 bars, fill ALL */
        CHECK(s.arr.n == 3 && s.arr.sec[1].bars == 64 && s.arr.sec[1].fill == ARR_FILL_ALL);
        live_arr_delete(&s, 1);
        CHECK(s.arr.n == 2 && s.arr.sec[1].scene == 1 && live_arr_total(&s) == 3);
        s.arr.end = ARR_STOP;
        seq_start(&s);                            /* section 1 (the demo) before the first note */
        CHECK(s.track[0].step[0].on && s.live.fill_auto == 0);
        int bar;
        run(&s, &lg, BAR + 5);
        CHECK(live_arr_where(&s, &bar) == 0 && bar == 1 && s.live.fill_auto == (1u << 3));   /* its last bar: drum fill */
        run(&s, &lg, BAR);
        CHECK(live_arr_where(&s, &bar) == 1 && bar == 0 && !s.track[0].step[0].on && s.live.fill_auto == 0);
        int before = count(&lg, 0x90);
        run(&s, &lg, BAR);
        CHECK(!s.playing && count(&lg, 0x90) == before && lg.msg[first(&lg, 0xFC, 0)].at == 3 * BAR);   /* STOP after 3 bars */
        /* LOOP: back to section 1 */
        s.arr.end = ARR_LOOP;
        seq_start(&s);
        run(&s, &lg, 3 * BAR + 5);
        CHECK(s.playing && live_arr_where(&s, &bar) == 0 && s.track[0].step[0].on);
        seq_stop(&s);
        CHECK(live_arr_where(&s, &bar) == -1);
        /* saved with the song */
        FILE *f = tmpfile();
        seq_save(&s, f); rewind(f);
        seq_init(&t, NULL, NULL);
        CHECK(seq_load(&t, f) == 0);
        fclose(f);
        CHECK(t.arr.on && t.arr.end == ARR_LOOP && t.arr.n == 2 && t.arr.sec[0].bars == 2 && t.arr.sec[0].fill == ARR_FILL_DRUMS && t.arr.sec[1].scene == 1);
        for (int k = 0; k < ARR_MAX + 2; k++) live_arr_insert(&t, 0, (ArrSection){0, 1, 0});
        CHECK(t.arr.n == ARR_MAX);
    }

    puts("live tests passed");
    return 0;
}
