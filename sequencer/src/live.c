#include "live.h"

#include <string.h>

#define BAR (SEQ_PPQN * 4)

static void out3(Seq *s, uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m[3] = {a, b, c};
    if (s->send) s->send(s->ctx, m, 3);
}

static void out1(Seq *s, uint8_t a) {
    if (s->send) s->send(s->ctx, &a, 1);
}

void live_init(Live *l) {
    memset(l, 0, sizeof *l);
    l->click_note = -1;
}

void live_send_start(Seq *s) {
    if (!s->transport_out) return;
    if (s->live.spp) out3(s, 0xF2, 0, 0);   /* Song Position 0: a DAW starts at its bar 1 */
    out1(s, 0xFA);
}

/* The click plays on the first drum track, as its closed hat (a lane whose
 * name has HAT and CL or CH in it, else lane 4); with no drum track, a high C
 * on track 1. */
static void click_target(const Seq *s, uint8_t *ch, uint8_t *note) {
    for (int t = 0; t < SEQ_TRACKS; t++) {
        if (s->track[t].type != TRACK_DRUMS) continue;
        int lane = 3;
        for (int l = 0; l < SEQ_LANES; l++) {
            const char *n = s->lane_name[l];
            if (strstr(n, "HAT") && (strstr(n, "CL") || strstr(n, "CH"))) { lane = l; break; }
        }
        *ch = s->track[t].channel;
        *note = s->lane_note[lane];
        return;
    }
    *ch = s->track[0].channel;
    *note = 96;
}

static void click_off(Seq *s) {
    Live *l = &s->live;
    if (l->click_note >= 0) out3(s, 0x80 | l->click_ch, (uint8_t)l->click_note, 0);
    l->click_note = -1;
    l->click_left = 0;
}

int live_start(Seq *s) {
    Live *l = &s->live;
    l->stop_pending = 0;
    click_off(s);
    if (l->from_mark) live_restore_mark(s);
    l->count_left = (uint32_t)l->count_in * BAR;
    return l->count_left > 0;
}

void live_stop(Seq *s) {
    s->live.count_left = 0;
    s->live.stop_pending = 0;
    click_off(s);
}

void live_count_tick(Seq *s) {
    Live *l = &s->live;
    uint32_t left = l->count_left;
    if (!left) return;
    if (s->clock_out && left % (SEQ_PPQN / 24) == 0) out1(s, 0xF8);
    if (l->click_left && --l->click_left == 0) click_off(s);
    if (l->click && left % SEQ_PPQN == 0) {   /* a beat: louder on each bar's first */
        click_off(s);
        uint8_t ch, note;
        click_target(s, &ch, &note);
        out3(s, 0x90 | ch, note, left % BAR == 0 ? 120 : 75);
        l->click_ch = ch;
        l->click_note = note;
        l->click_left = 12;
    }
    if (--l->count_left == 0) {   /* the next tick is tick 0: beat 1 of bar 1 */
        click_off(s);
        live_send_start(s);
    }
}

int live_bar(Seq *s, uint32_t bar) {
    Live *l = &s->live;
    if (l->stop_pending || (l->length && bar >= l->length)) {
        seq_stop(s);
        return 1;
    }
    return 0;
}

int live_stop_request(Seq *s) {
    Live *l = &s->live;
    if (l->stop_mode == STOP_AT_BAR && !l->stop_pending && !l->count_left && s->tick % BAR != 0) {
        l->stop_pending = 1;
        return 1;
    }
    seq_stop(s);
    return 0;
}

void live_mark(Seq *s) {
    Live *l = &s->live;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        const SeqTrack *tr = &s->track[t];
        LiveTake *k = &l->mark[t];
        memcpy(k->step, tr->step, sizeof k->step);
        k->drum = tr->drum;
        k->len = tr->len;
        k->cur_slot = tr->cur_slot;
        k->chain_pos = tr->chain_pos;
        k->gen_state = tr->gen.state;
    }
    l->mark_rng = s->rng;
    l->has_mark = 1;
}

int live_restore_mark(Seq *s) {
    Live *l = &s->live;
    if (!l->has_mark) return -1;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        const LiveTake *k = &l->mark[t];
        seq_track_release(s, t);
        memcpy(tr->step, k->step, sizeof tr->step);
        tr->drum = k->drum;
        tr->len = k->len;
        tr->cur_slot = k->cur_slot;
        tr->chain_pos = (uint8_t)(tr->chain_len ? k->chain_pos % tr->chain_len : 0);
        tr->gen.state = k->gen_state;
        tr->rest_left = 0;
    }
    s->rng = l->mark_rng;
    for (int t = 0; t < SEQ_TRACKS; t++) seq_reshuffle(s, t);   /* the same live offsets too */
    s->rng = l->mark_rng;
    return 0;
}

void live_position(const Seq *s, int *bar, int *beat) {
    *bar = *beat = 0;
    if (!s->playing || s->live.count_left) return;
    uint32_t t = s->tick ? s->tick - 1 : 0;
    *bar = (int)(t / BAR) + 1;
    *beat = (int)(t % BAR / SEQ_PPQN) + 1;
}

int live_counting(const Seq *s, int *bars, int *beats) {
    uint32_t left = s->live.count_left;
    *bars = *beats = 0;
    if (!s->playing || !left) return 0;
    uint32_t b = (left + SEQ_PPQN - 1) / SEQ_PPQN;   /* beats to go, counting this one */
    *bars = (int)((b - 1) / 4);
    *beats = (int)((b - 1) % 4) + 1;
    return 1;
}

void live_save(const Seq *s, FILE *f) {
    const Live *l = &s->live;
    /* "lv COUNT CLICK LENGTH STOPMODE SPP FROMMARK"; older versions skip it */
    fprintf(f, "lv %d %d %d %d %d %d\n", l->count_in, l->click, l->length, l->stop_mode, l->spp, l->from_mark);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int live_load_line(Seq *s, const char *line) {
    int a, b, c, d, e, g;
    if (strncmp(line, "lv ", 3) != 0) return 0;
    if (sscanf(line, "lv %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g) == 6) {
        Live *l = &s->live;
        l->count_in = (uint8_t)(a <= 0 ? 0 : a <= 1 ? 1 : a <= 2 ? 2 : 4);
        l->click = b != 0;
        l->length = (uint16_t)clampi(c, 0, LIVE_MAX_LENGTH);
        l->stop_mode = d == STOP_AT_BAR ? STOP_AT_BAR : STOP_NOW;
        l->spp = e != 0;
        l->from_mark = g != 0;
    }
    return 1;
}
