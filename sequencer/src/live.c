#include "live.h"

#include <stdlib.h>
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
    l->quant = 1;
    l->scene = l->scene_q = l->song_q = l->song_now = -1;
}

void live_send_start(Seq *s) {
    if (!s->transport_out) return;
    if (s->live.spp) out3(s, 0xF2, 0, 0);   /* Song Position 0: a DAW starts at its bar 1 */
    out1(s, 0xFA);
}

/* The lane to roll on (SNARE, else SN or CLAP, else lane 2) and the closed hat
 * (HAT with CL or CH, or a name starting CH; else lane 3). */
static int snare_lane(const Seq *s) {
    for (int l = 0; l < SEQ_LANES; l++) if (strstr(s->lane_name[l], "SNARE")) return l;
    for (int l = 0; l < SEQ_LANES; l++) if (!strncmp(s->lane_name[l], "SD", 2) || strstr(s->lane_name[l], "SN")) return l;
    for (int l = 0; l < SEQ_LANES; l++) if (strstr(s->lane_name[l], "CLAP")) return l;
    return 1;
}
static int hat_lane(const Seq *s) {
    for (int l = 0; l < SEQ_LANES; l++) {
        const char *n = s->lane_name[l];
        if ((strstr(n, "HAT") && (strstr(n, "CL") || strstr(n, "CH"))) || !strncmp(n, "CH", 2)) return l;
    }
    return 2;   /* FELUCCA and GM: lane 3 */
}

/* The click plays on the first drum track, as its closed hat (hat_lane);
 * with no drum track, a high C on track 1. */
static void click_target(const Seq *s, uint8_t *ch, uint8_t *note) {
    for (int t = 0; t < SEQ_TRACKS; t++) {
        if (s->track[t].type != TRACK_DRUMS) continue;
        int lane = hat_lane(s);
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

static void arr_begin(Seq *s);

static void switch_song(Seq *s) {
    Live *l = &s->live;
    void *song = l->song_next;
    void (*swap)(void *, void *) = l->song_swap;
    int k = l->song_q;
    l->song_next = NULL;
    l->song_swap = NULL;
    l->song_q = -1;
    if (!song || !swap) return;
    swap(s, song);   /* song_apply keeps s->live */
    s->live.song_now = (int8_t)k;
    s->live.scene = -1;
    arr_begin(s);    /* the new song's own arrangement, from its start */
}

void live_start_song_now(Seq *s) { if (s->live.song_q >= 0) switch_song(s); }

static void arr_fills(Seq *s) {
    const Arrange *a = &s->arr;
    s->live.fill_auto = 0;
    if (!a->on || a->pos >= a->n) return;
    const ArrSection *sec = &a->sec[a->pos];
    if (sec->fill == ARR_FILL_OFF || a->bar + 1 != sec->bars) return;
    for (int t = 0; t < SEQ_TRACKS; t++)
        if (sec->fill == ARR_FILL_ALL || s->track[t].type == TRACK_DRUMS) s->live.fill_auto |= (uint16_t)(1u << t);
}

/* Start, or a song swapped in: section 1, bar 1. */
static void arr_begin(Seq *s) {
    Arrange *a = &s->arr;
    a->pos = 0;
    a->bar = 0;
    s->live.fill_auto = 0;
    if (!a->on || !a->n) return;
    live_scene_recall(s, a->sec[0].scene);
    arr_fills(s);
}

/* A bar line: 1 if the arrangement ended and playback stopped. */
static int arr_bar(Seq *s) {
    Arrange *a = &s->arr;
    if (!a->on || !a->n) { s->live.fill_auto = 0; return 0; }
    if (a->pos >= a->n) a->pos = 0;
    if (++a->bar >= a->sec[a->pos].bars) {
        a->bar = 0;
        if (++a->pos >= a->n) {
            if (a->end == ARR_STOP) { a->pos = 0; s->live.fill_auto = 0; seq_stop(s); return 1; }
            a->pos = 0;
        }
        live_scene_recall(s, a->sec[a->pos].scene);
    }
    arr_fills(s);
    return 0;
}

int live_start(Seq *s) {
    Live *l = &s->live;
    l->stop_pending = 0;
    click_off(s);
    if (l->song_q >= 0) switch_song(s);   /* queued while stopped: in before bar 1 */
    if (l->scene_q >= 0) { live_scene_recall(s, l->scene_q); l->scene_q = -1; }
    if (l->from_mark) live_restore_mark(s);
    arr_begin(s);
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
    if (arr_bar(s)) return 1;   /* ARRANGE ended with STOP */
    if (bar % (l->quant ? l->quant : 1) == 0) {   /* a switch bar */
        if (l->song_q >= 0) switch_song(s);
        if (l->scene_q >= 0) { live_scene_recall(s, l->scene_q); l->scene_q = -1; }
    }
    return 0;
}

/* ---- scenes ---- */

static int chained(const SeqTrack *tr) { return tr->chain_len > 1; }

int live_scene_tracks(const Seq *s, int k) {
    int n = 0;
    if (k < 0 || k >= LIVE_SCENES) return 0;
    for (int t = 0; t < SEQ_TRACKS; t++) n += s->track[t].slot[k].used != 0;
    return n;
}

void live_scene_store(Seq *s, int k) {
    if (k < 0 || k >= LIVE_SCENES) return;
    for (int t = 0; t < SEQ_TRACKS; t++) seq_slot_store(s, t, k);
    s->live.scene = (int8_t)k;
}

int live_scene_recall(Seq *s, int k) {
    int n = 0;
    if (k < 0 || k >= LIVE_SCENES) return 0;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        if (chained(&s->track[t]) || !s->track[t].slot[k].used) continue;
        if (seq_slot_recall(s, t, k) == 0) n++;
    }
    s->live.scene = (int8_t)k;
    return n;
}

int live_queue_scene(Seq *s, int k) {
    Live *l = &s->live;
    if (k < 0 || k >= LIVE_SCENES) { l->scene_q = -1; return 0; }
    if (!s->playing || l->count_left) {
        live_scene_recall(s, k);
        l->scene_q = -1;
        return 0;
    }
    l->scene_q = (int8_t)k;
    return 1;
}

void live_queue_song(Seq *s, int k, void *song, void (*swap)(void *, void *)) {
    Live *l = &s->live;
    if (l->song_next) {   /* replace one already waiting */
        void *old = l->song_next;
        l->song_next = NULL;
        free(old);
    }
    l->song_q = -1;
    l->song_next = NULL;
    l->song_swap = NULL;
    if (k < 0 || !song || !swap) { free(song); return; }
    l->song_q = (int8_t)k;
    l->song_next = song;
    l->song_swap = swap;
}

int live_switch_in(const Seq *s, int *bars, int *beats) {
    const Live *l = &s->live;
    *bars = *beats = 0;
    if (!s->playing || l->count_left || (l->scene_q < 0 && l->song_q < 0)) return 0;
    uint32_t q = (uint32_t)(l->quant ? l->quant : 1) * BAR;
    uint32_t now = s->tick, next = (now / q + 1) * q;
    if (now % q == 0 && now) next = now;   /* this tick is the switch bar */
    uint32_t b = (next - now + SEQ_PPQN - 1) / SEQ_PPQN;   /* beats to go, rounded up */
    *bars = (int)(b / 4);
    *beats = (int)(b % 4);
    return 1;
}

void live_set_quant(Seq *s, int bars) {
    s->live.quant = (uint8_t)(bars <= 1 ? 1 : bars <= 2 ? 2 : bars <= 4 ? 4 : 8);
}

/* ---- DENSITY ---- */

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void live_density(Seq *s, int t, int d) {
    if (t < 0 || t >= SEQ_TRACKS) return;
    s->live.density[t] = (int8_t)clampi(s->live.density[t] + d, -100, 100);
}

void live_fill(Seq *s, int t, int on) {
    if (t < 0 || t >= SEQ_TRACKS) return;
    if (on) s->live.fill |= (uint16_t)(1u << t);
    else s->live.fill &= (uint16_t)~(1u << t);
}

static int filling(const Seq *s, int t) { return ((s->live.fill | s->live.fill_auto) >> t) & 1; }

int live_busy(const Seq *s, int t) { return s->live.density[t] != 0 || filling(s, t); }

/* Sixteenth of the bar being played, 0..15. */
static int bar16(const Seq *s) { return (int)(s->tick % BAR / (BAR / 16)); }

static int pct(Seq *s, int chance) { return chance > 0 && (int)(seq_rand(s) % 100) < chance; }

const SeqStep *live_note_step(Seq *s, int t, int idx, const SeqStep *st, SeqStep *tmp) {
    const SeqTrack *tr = &s->track[t];
    int d = s->live.density[t];
    if (filling(s, t)) {   /* every step; the last beat of the bar an octave up */
        int lift = bar16(s) >= 12 ? 12 : 0;
        if (st->on) { *tmp = *st; tmp->vel = (uint8_t)(st->vel + 10 > 127 ? 127 : st->vel + 10); }
        else {
            const SeqStep *src = NULL;
            for (int k = 1; k <= tr->len && !src; k++)
                if (tr->step[(idx - k + tr->len) % tr->len].on) src = &tr->step[(idx - k + tr->len) % tr->len];
            if (!src) return NULL;
            *tmp = *src;
            tmp->vel = (uint8_t)(src->vel * 4 / 5 < 1 ? 1 : src->vel * 4 / 5);
            tmp->hold = 0; tmp->nudge = st->nudge; tmp->lock = 0; tmp->ratchet = 0; tmp->cond = 0;
            tmp->gate = (uint8_t)(src->gate > 80 ? 80 : src->gate);
        }
        tmp->prob = 100;
        if (tmp->note + lift <= 127) tmp->note = (uint8_t)(tmp->note + lift);
        return tmp;
    }
    if (st->on) {
        if (d >= 0) return st;
        return pct(s, idx % 8 == 0 ? -d / 2 : -d) ? NULL : st;   /* beats 1 and 3 hold on longest */
    }
    if (d <= 0 || !pct(s, d / 2)) return NULL;
    for (int k = 1; k < tr->len; k++) {   /* the track's last note before this step */
        const SeqStep *src = &tr->step[(idx - k + tr->len) % tr->len];
        if (!src->on) continue;
        *tmp = *src;
        tmp->vel = (uint8_t)(src->vel * 3 / 5 < 1 ? 1 : src->vel * 3 / 5);
        tmp->hold = 0;
        tmp->prob = 100;
        tmp->nudge = st->nudge;
        tmp->lock = 0;
        return tmp;
    }
    return NULL;
}

static int is_kick(const Seq *s, int l) { return strstr(s->lane_name[l], "KICK") || strstr(s->lane_name[l], "BD"); }


int live_drum_step(Seq *s, int t, int idx, uint8_t vel[SEQ_LANES]) {
    const SeqTrack *tr = &s->track[t];
    int d = s->live.density[t], n = 0;
    for (int l = 0; l < SEQ_LANES; l++) {
        vel[l] = 0;
        if (!seq_lane_audible(tr, l)) continue;
        int v = tr->drum.vel[l][idx];
        if (v) {
            int p = tr->drum.prob[l][idx];
            if (p < 100 && (int)(seq_rand(s) % 100) >= p) continue;
            if (d < 0 && pct(s, is_kick(s, l) ? -d / 2 : -d)) continue;
            vel[l] = (uint8_t)v;
            n++;
        } else if (d > 0 && !is_kick(s, l)) {   /* ghost hits on lanes that already play */
            int hits = 0, sum = 0;
            for (int i = 0; i < tr->len; i++) if (tr->drum.vel[l][i]) { hits++; sum += tr->drum.vel[l][i]; }
            if (!hits || !pct(s, d / 3)) continue;
            vel[l] = (uint8_t)clampi(sum / hits / 2, 1, 127);
            n++;
        }
    }
    if (filling(s, t)) {
        int p = bar16(s), sn = snare_lane(s), hh = hat_lane(s);
        if (p >= 8 && seq_lane_audible(tr, sn)) {   /* snare roll, rising */
            int v = 50 + (p - 8) * 10;
            if (vel[sn] < v) { n += !vel[sn]; vel[sn] = (uint8_t)v; }
        }
        if (seq_lane_audible(tr, hh) && !vel[hh]) { vel[hh] = (uint8_t)(p % 2 ? 55 : 80); n++; }
    }
    return n;
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

int live_arr_total(const Seq *s) {
    int n = 0;
    for (int i = 0; i < s->arr.n && i < ARR_MAX; i++) n += s->arr.sec[i].bars;
    return n;
}

static ArrSection arr_clean(ArrSection x) {
    x.scene = (uint8_t)clampi(x.scene, 0, LIVE_SCENES - 1);
    x.bars = (uint8_t)clampi(x.bars, 1, 64);
    x.fill = (uint8_t)clampi(x.fill, ARR_FILL_OFF, ARR_FILL_ALL);
    return x;
}

void live_arr_insert(Seq *s, int i, ArrSection sec) {
    Arrange *a = &s->arr;
    if (a->n >= ARR_MAX) return;
    i = clampi(i, 0, a->n);
    memmove(&a->sec[i + 1], &a->sec[i], (size_t)(a->n - i) * sizeof a->sec[0]);
    a->sec[i] = arr_clean(sec);
    a->n++;
    if (s->playing && a->on && i <= a->pos && a->n > 1) a->pos++;   /* keep playing the same section */
}

void live_arr_delete(Seq *s, int i) {
    Arrange *a = &s->arr;
    if (i < 0 || i >= a->n) return;
    memmove(&a->sec[i], &a->sec[i + 1], (size_t)(a->n - i - 1) * sizeof a->sec[0]);
    a->n--;
    if (i < a->pos) a->pos--;
    if (a->pos >= a->n) { a->pos = 0; a->bar = 0; }
}

int live_arr_where(const Seq *s, int *bar) {
    *bar = 0;
    if (!s->playing || s->live.count_left || !s->arr.on || !s->arr.n) return -1;
    *bar = s->arr.bar;
    return s->arr.pos;
}

void live_arr_save(const Seq *s, FILE *f) {
    const Arrange *a = &s->arr;
    if (!a->on && !a->n) return;
    fprintf(f, "ar %d %d %d\n", a->on, a->end, a->n);   /* "ar ON END N", then "as I SCENE BARS FILL"; older versions skip them */
    for (int i = 0; i < a->n; i++) fprintf(f, "as %d %d %d %d\n", i, a->sec[i].scene, a->sec[i].bars, a->sec[i].fill);
}

void live_save(const Seq *s, FILE *f) {
    const Live *l = &s->live;
    /* "lv COUNT CLICK LENGTH STOPMODE SPP FROMMARK"; older versions skip it */
    fprintf(f, "lv %d %d %d %d %d %d\n", l->count_in, l->click, l->length, l->stop_mode, l->spp, l->from_mark);
    fprintf(f, "lq %d\n", l->quant);   /* scene and song switch bars */
    for (int t = 0; t < SEQ_TRACKS; t++)
        if (l->density[t]) fprintf(f, "ld %d %d\n", t, l->density[t]);
}

int live_load_line(Seq *s, const char *line) {
    int a, b, c, d, e, g;
    if (sscanf(line, "ar %d %d %d", &a, &b, &c) == 3) {
        s->arr.on = a != 0;
        s->arr.end = b == ARR_STOP ? ARR_STOP : ARR_LOOP;
        s->arr.n = (uint8_t)clampi(c, 0, ARR_MAX);
        for (int i = 0; i < s->arr.n; i++) s->arr.sec[i] = arr_clean((ArrSection){0, 4, 0});
        return 1;
    }
    if (sscanf(line, "as %d %d %d %d", &a, &b, &c, &d) == 4) {
        if (a >= 0 && a < s->arr.n) s->arr.sec[a] = arr_clean((ArrSection){(uint8_t)clampi(b, 0, 255), (uint8_t)clampi(c, 0, 255), (uint8_t)clampi(d, 0, 255)});
        return 1;
    }
    if (sscanf(line, "lq %d", &a) == 1) { live_set_quant(s, a); return 1; }
    if (sscanf(line, "ld %d %d", &a, &b) == 2) {
        if (a >= 0 && a < SEQ_TRACKS) s->live.density[a] = (int8_t)clampi(b, -100, 100);
        return 1;
    }
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
