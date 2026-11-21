#include "engine.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "drum.h"
#include "gen.h"

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int8_t draw_offset(Seq *s, const SeqTrack *tr, const SeqStep *st);
static int live_offset(const SeqTrack *tr);

uint32_t seq_rand(Seq *s) {
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}

static void emit3(Seq *s, uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m[3] = {a, b, c};
    if (s->send) s->send(s->ctx, m, 3);
}

static void emit1(Seq *s, uint8_t a) {
    if (s->send) s->send(s->ctx, &a, 1);
}

static void release_lane(Seq *s, SeqTrack *t, int l) {
    if (!(t->drum_down & (1 << l))) return;
    emit3(s, 0x80 | t->channel, t->drum_note[l], 0);
    t->drum_down &= (uint8_t)~(1 << l);
}

static void release_pv(Seq *s, SeqTrack *t, int k) {
    emit3(s, 0x80 | t->channel, t->pv_note[k], 0);
    t->npv--;
    t->pv_note[k] = t->pv_note[t->npv];
    t->pv_off[k] = t->pv_off[t->npv];
}

static void release(Seq *s, SeqTrack *t) {
    for (int l = 0; t->drum_down && l < SEQ_LANES; l++) release_lane(s, t, l);
    while (t->npv) release_pv(s, t, t->npv - 1);
    if (t->sounding >= 0) {
        emit3(s, 0x80 | t->channel, (uint8_t)t->sounding, 0);
        for (int k = 0; k < t->nvoice; k++) emit3(s, 0x80 | t->channel, t->voice[k], 0);
        t->sounding = -1;
        t->nvoice = 0;
    }
}

void seq_init(Seq *s, SeqSend send, void *ctx) {
    static const uint8_t base[SEQ_TRACKS] = {36, 48, 60, 72};
    memset(s, 0, sizeof *s);
    s->bpm = 120;
    s->scale = SCALE_MIN_PENT;
    s->clock_out = 1;
    s->transport_out = 1;
    s->style = STYLE_AMBIENT;
    s->rng = 0x2545F491u;
    s->send = send;
    s->ctx = ctx;
    seq_drum_map_gm(s);
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        tr->len = SEQ_DEFAULT_LEN;
        tr->channel = (uint8_t)t;
        tr->sounding = -1;
        tr->last_note = -1;
        tr->gen = (GenSettings){GEN_NONE, {0, 0, 0, 0}, (uint8_t)(base[t] - 12), 2, 0x9E37u + (uint32_t)t * 0x2F1Bu};
        tr->evo = (EvolveSettings){0, 20, EVO_PITCH | EVO_RHYTHM, 0, 0, 0};
        tr->chain_bars = 4;
        tr->style = STYLE_AMBIENT;
        tr->part = (uint8_t)t;
        tr->drum_busy = 50;
        for (int l = 0; l < SEQ_LANES; l++)
            for (int i = 0; i < SEQ_STEPS; i++) tr->drum.prob[l][i] = tr->drum_anchor.prob[l][i] = 100;
        for (int k = 0; k < SEQ_SLOTS; k++) tr->slot[k].drum = tr->drum;
        for (int i = 0; i < SEQ_STEPS; i++)
            tr->step[i] = (SeqStep){0, base[t], 100, 50, 100, 0, 0, 0, {0}};
    }
}

void seq_load_demo(Seq *s) {
    static const int kick[] = {0, 4, 8, 12};
    static const int hat[] = {2, 6, 10, 14};
    static const int bass[] = {0, 3, 6, 10, 12};
    static const int bassn[] = {36, 36, 39, 34, 36};
    static const int lead[] = {0, 4, 7, 11, 14};
    static const int leadn[] = {67, 70, 72, 70, 74};
    for (unsigned k = 0; k < 4; k++) s->track[0].step[kick[k]].on = 1;
    for (unsigned k = 0; k < 4; k++) {
        SeqStep *st = &s->track[1].step[hat[k]];
        st->on = 1; st->note = 54; st->vel = 80; st->gate = 25;
    }
    for (unsigned k = 0; k < 5; k++) {
        SeqStep *st = &s->track[2].step[bass[k]];
        st->on = 1; st->note = (uint8_t)bassn[k]; st->gate = 70;
    }
    for (unsigned k = 0; k < 5; k++) {
        SeqStep *st = &s->track[3].step[lead[k]];
        st->on = 1; st->note = (uint8_t)leadn[k]; st->vel = 90; st->prob = 80;
    }
}

void seq_start(Seq *s) {
    if (s->playing) return;
    s->tick = 0;
    s->idle_tick = 0;
    s->playing = 1;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        s->track[t].rest_left = 0;
        memset(s->track[t].played, 0, sizeof s->track[t].played);
        if (live_offset(&s->track[t])) seq_reshuffle(s, t);
    }
    if (s->transport_out) emit1(s, 0xFA);
}

void seq_stop(Seq *s) {
    if (!s->playing) return;
    s->playing = 0;
    s->resync = 0;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        release(s, &s->track[t]);
        emit3(s, 0xB0 | s->track[t].channel, 123, 0);   /* all notes off */
    }
    if (s->transport_out) emit1(s, 0xFC);
}

void seq_panic(Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        if (tr->last_note >= 0 && tr->last_note != tr->sounding) {
            emit3(s, 0x80 | tr->channel, (uint8_t)tr->last_note, 0);
            for (int k = 0; k < tr->last_nvoice; k++) emit3(s, 0x80 | tr->channel, tr->last_voice[k], 0);
        }
        release(s, tr);
    }
    for (int t = 0; t < SEQ_TRACKS; t++) {
        uint8_t ch = s->track[t].channel;
        int dup = 0;
        for (int u = 0; u < t; u++) dup |= s->track[u].channel == ch;
        if (dup) continue;
        emit3(s, 0xB0 | ch, 120, 0);   /* all sound off */
        emit3(s, 0xB0 | ch, 123, 0);   /* all notes off */
    }
}

void seq_reconnected(Seq *s) {
    seq_panic(s);
    if (s->playing && s->transport_out) s->resync = 1;
}

void seq_set_sync(Seq *s, int clock, int transport) {
    s->clock_out = clock != 0;
    if (transport && !s->transport_out && s->playing) s->resync = 1;
    s->transport_out = transport != 0;
    if (!s->transport_out) s->resync = 0;
}

void seq_track_release(Seq *s, int t) { release(s, &s->track[t]); }

/* A POLY track's note: it rings until `off`, whatever the track plays next.
 * The same note again restarts it; with every voice busy the one due to end
 * first makes room. */
static void poly_note(Seq *s, SeqTrack *tr, uint8_t note, int vel, uint32_t off) {
    for (int k = 0; k < tr->npv; k++)
        if (tr->pv_note[k] == note) { release_pv(s, tr, k); break; }
    if (tr->npv >= SEQ_POLY_VOICES) {
        int first = 0;
        for (int k = 1; k < tr->npv; k++)
            if ((int32_t)(tr->pv_off[k] - tr->pv_off[first]) < 0) first = k;
        release_pv(s, tr, first);
    }
    emit3(s, 0x90 | tr->channel, note, (uint8_t)vel);
    tr->pv_note[tr->npv] = note;
    tr->pv_off[tr->npv] = off;
    tr->npv++;
}

/* Send one step's note (and its chord notes); called at the tick the step is due. */
static void play_step(Seq *s, int t, uint32_t now, const SeqStep *st) {
    SeqTrack *tr = &s->track[t];
    if (st->prob < 100 && (int)(seq_rand(s) % 100) >= st->prob) return;

    if (tr->poly) {
        uint32_t gl = (uint32_t)st->gate * SEQ_TICKS_PER_STEP / 100 + (uint32_t)st->hold * SEQ_TICKS_PER_STEP;
        uint32_t off = now + (gl ? gl : 1);
        int vel = seq_swell_vel(s, t, now, st->vel);
        uint8_t nn[SEQ_POLY];
        int n = seq_step_notes(s, st, nn);
        for (int k = 0; k < n; k++) poly_note(s, tr, nn[k], vel, off);
        if (n == 1 && tr->chord) {   /* a single note gets the CHORD row's chord, a little softer */
            uint8_t cn[SEQ_CHORD_MAX];
            int nc = gen_chord_notes(s, st->note, tr->chord, cn);
            int cv = vel * 4 / 5 < 1 ? 1 : vel * 4 / 5;
            for (int k = 0; k < nc; k++) poly_note(s, tr, cn[k], cv, off);
        }
        tr->last_note = -1;   /* release() covers every POLY note */
        return;
    }

    release(s, tr);   /* retrigger */
    int vel = seq_swell_vel(s, t, now, st->vel);
    emit3(s, 0x90 | tr->channel, st->note, (uint8_t)vel);
    tr->nvoice = 0;
    if (tr->chord) {
        uint8_t cn[SEQ_CHORD_MAX];
        int nc = gen_chord_notes(s, st->note, tr->chord, cn);
        int cv = vel * 4 / 5 < 1 ? 1 : vel * 4 / 5;   /* chord notes sit under the step's note */
        for (int k = 0; k < nc; k++) {
            emit3(s, 0x90 | tr->channel, cn[k], (uint8_t)cv);
            tr->voice[tr->nvoice++] = cn[k];
        }
    }
    tr->sounding = st->note;
    tr->last_note = st->note;
    tr->last_nvoice = tr->nvoice;
    memcpy(tr->last_voice, tr->voice, sizeof tr->voice);
    uint32_t gl = (uint32_t)st->gate * SEQ_TICKS_PER_STEP / 100 + (uint32_t)st->hold * SEQ_TICKS_PER_STEP;
    tr->off_tick = now + (gl ? gl : 1);
}

/* A drum track's step: every lane with a hit there plays its own note (from
 * the DRUM MAP), each with its own chance, as short one-shots. */
static void play_drums(Seq *s, int t, uint32_t now, int idx) {
    SeqTrack *tr = &s->track[t];
    for (int l = 0; l < SEQ_LANES; l++) {
        int v = tr->drum.vel[l][idx];
        if (!v || !seq_lane_audible(tr, l)) continue;
        int p = tr->drum.prob[l][idx];
        if (p < 100 && (int)(seq_rand(s) % 100) >= p) continue;
        release_lane(s, tr, l);
        uint8_t note = s->lane_note[l];
        for (int m = 0; m < SEQ_LANES; m++)   /* two lanes on one note: the new hit takes over */
            if (m != l && (tr->drum_down & (1 << m)) && tr->drum_note[m] == note) release_lane(s, tr, m);
        emit3(s, 0x90 | tr->channel, note, (uint8_t)seq_swell_vel(s, t, now, v));
        tr->drum_note[l] = note;
        tr->drum_down |= (uint8_t)(1 << l);
        tr->drum_off[l] = now + SEQ_DRUM_GATE;
    }
}

/* The UI's auditioned note: its note-off, playing or stopped. */
static void audition_tick(Seq *s) {
    if (s->aud_left && --s->aud_left == 0) emit3(s, 0x80 | s->aud_ch, (uint8_t)s->aud_note, 0);
}

void seq_tick(Seq *s) {
    audition_tick(s);
    if (!s->playing) {
        /* Keep sending clock while stopped, so the FM1 locks to the tempo
         * before Start (and shows it). Start then restarts the count. */
        if (s->clock_out && s->idle_tick++ % (SEQ_PPQN / 24) == 0) emit1(s, 0xF8);
        return;
    }
    uint32_t now = s->tick;
    if (s->resync && now % (SEQ_PPQN * 4) == 0) {   /* bar line: the receiver's beat 1 */
        s->resync = 0;
        if (s->transport_out) { emit1(s, 0xFC); emit1(s, 0xFA); }
    }
    if (s->clock_out && now % (SEQ_PPQN / 24) == 0) emit1(s, 0xF8);

    if (now && now % (SEQ_PPQN * 4) == 0) {          /* a bar boundary */
        uint32_t bar = now / (SEQ_PPQN * 4);
        for (int t = 0; t < SEQ_TRACKS; t++) seq_chain_bar(s, t, bar);
        /* Rests: after a rest ends the track plays at least one bar. */
        for (int t = 0; t < SEQ_TRACKS; t++) {
            SeqTrack *tr = &s->track[t];
            if (tr->rest_left) tr->rest_left--;
            else if (tr->rest && (int)(seq_rand(s) % 100) < tr->rest) tr->rest_left = (uint8_t)(1 + seq_rand(s) % 4);
        }
        for (int t = 0; t < SEQ_TRACKS; t++) {
            uint8_t e = s->track[t].evo.every;
            if (e && bar % e == 0) gen_evolve_bar(s, t);
        }
    }

    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        if (tr->sounding >= 0 && (int32_t)(now - tr->off_tick) >= 0) release(s, tr);
        for (int k = tr->npv - 1; k >= 0; k--)
            if ((int32_t)(now - tr->pv_off[k]) >= 0) release_pv(s, tr, k);
        for (int l = 0; tr->drum_down && l < SEQ_LANES; l++)
            if ((tr->drum_down & (1 << l)) && (int32_t)(now - tr->drum_off[l]) >= 0) release_lane(s, tr, l);
        int drums = tr->type == TRACK_DRUMS;
        if (!seq_track_audible(s, t) || tr->rest_left) continue;   /* a rest lets held notes ring out */

        /* A step plays at its grid time + swing + its own offset. Swing follows
         * the shared 1/16 grid, not the track's own step number, so an
         * odd-length track stays in the same groove as the others. Swing +
         * offset is -11..23 ticks, so a step due now is either this 1/16's
         * step (on time or late) or the next one (early). If both are due on
         * the same tick, the next step's note cuts the first one off. */
        uint32_t gstep = now / SEQ_TICKS_PER_STEP;
        int phase = (int)(now % SEQ_TICKS_PER_STEP);
        for (int ahead = 0; ahead < 2; ahead++) {
            uint32_t g = gstep + (uint32_t)ahead;
            int idx = (int)(g % tr->len);
            const SeqStep *st = &tr->step[idx];
            if (!(drums ? seq_drum_column(tr, idx) : st->on) || tr->played[idx] == g + 1) continue;
            int live = live_offset(tr);
            int when = ((g & 1) ? s->swing : 0) + (live ? tr->jit[idx] : drums ? 0 : st->nudge);   /* drum steps have no OFFSET */
            int due = ahead == 0 ? (when == phase || (when < 0 && now == 0))   /* an early first step plays at Start */
                                 : (when < 0 && when + SEQ_TICKS_PER_STEP == phase);
            if (!due) continue;
            tr->played[idx] = g + 1;
            if (live) tr->jit[idx] = draw_offset(s, tr, st);   /* where it lands next time round */
            if (drums) play_drums(s, t, now, idx);
            else play_step(s, t, now, st);
        }
    }
    s->tick++;
}

int seq_playhead(const Seq *s, int t) {
    if (!s->playing) return -1;
    uint32_t last = s->tick ? s->tick - 1 : 0;
    return (int)((last / SEQ_TICKS_PER_STEP) % s->track[t].len);
}

int seq_swell_vel(const Seq *s, int t, uint32_t now, int vel) {
    const SeqTrack *tr = &s->track[t];
    if (!tr->swell || !tr->swell_depth) return vel;
    /* A triangle from quiet (step velocity minus depth%) to full and back,
     * offset a quarter period per track so the tracks swell in turn. */
    uint32_t period = (uint32_t)tr->swell * SEQ_PPQN * 4, half = period / 2;
    uint32_t ph = (now + (uint32_t)t * (period / 4)) % period;
    uint32_t tri = (ph < half ? ph : period - ph) * 1000 / half;   /* 0..1000 */
    int f = 1000 - (int)(tr->swell_depth * (1000 - tri) / 100);
    int v = vel * f / 1000;
    return v < 1 ? 1 : v > 127 ? 127 : v;
}

void seq_step_toggle(Seq *s, int t, int i) { s->track[t].step[i].on ^= 1; }

int seq_step_more_count(const SeqStep *st) {
    int n = 0;
    while (n < SEQ_POLY - 1 && st->more[n]) n++;
    return n;
}

int seq_step_notes(const Seq *s, const SeqStep *st, uint8_t out[SEQ_POLY]) {
    int n = 0;
    out[n++] = st->note;
    int m = seq_step_more_count(st);
    if (!m) return n;
    int d = gen_note_degree(s, st->note);
    for (int k = 0; k < m; k++) {
        int dd = d + st->more[k];
        if (dd < 0) continue;
        int note = gen_degree_note(s, dd), dup = 0;
        for (int j = 0; j < n; j++) dup |= out[j] == note;
        if (!dup) out[n++] = (uint8_t)note;
    }
    return n;
}

int seq_step_has_note(const Seq *s, const SeqStep *st, int note) {
    if (!st->on) return 0;
    uint8_t nn[SEQ_POLY];
    int n = seq_step_notes(s, st, nn);
    for (int k = 0; k < n; k++) if (nn[k] == note) return 1;
    return 0;
}

/* Rewrite a step's chord notes from absolute notes, as degrees from its note. */
static void set_more(const Seq *s, SeqStep *st, const uint8_t *notes, int n) {
    int d = gen_note_degree(s, st->note), m = 0;
    memset(st->more, 0, sizeof st->more);
    for (int k = 0; k < n && m < SEQ_POLY - 1; k++) {
        int off = gen_note_degree(s, notes[k]) - d;
        if (!off || off < -SEQ_MORE_MAX || off > SEQ_MORE_MAX) continue;
        int dup = 0;
        for (int j = 0; j < m; j++) dup |= st->more[j] == off;
        if (!dup) st->more[m++] = (int8_t)off;
    }
}

int seq_step_add_note(Seq *s, int t, int i, int note) {
    SeqStep *st = &s->track[t].step[i];
    note = clampi(note, 0, 127);
    if (!st->on) {
        st->on = 1;
        st->note = (uint8_t)note;
        memset(st->more, 0, sizeof st->more);
        return 0;
    }
    if (seq_step_has_note(s, st, note)) return 1;
    uint8_t nn[SEQ_POLY];
    int n = seq_step_notes(s, st, nn);
    if (n >= SEQ_POLY) return -1;
    int off = gen_note_degree(s, note) - gen_note_degree(s, st->note);
    if (!off || off < -SEQ_MORE_MAX || off > SEQ_MORE_MAX) return -1;
    nn[n++] = (uint8_t)note;
    set_more(s, st, nn + 1, n - 1);
    return 0;
}

int seq_step_remove_note(Seq *s, int t, int i, int note) {
    SeqStep *st = &s->track[t].step[i];
    if (!seq_step_has_note(s, st, note)) return -1;
    uint8_t nn[SEQ_POLY], rest[SEQ_POLY];
    int n = seq_step_notes(s, st, nn), r = 0;
    for (int k = 0; k < n; k++) if (nn[k] != note) rest[r++] = nn[k];
    if (!r) { st->on = 0; memset(st->more, 0, sizeof st->more); return 0; }
    if (note == st->note) {   /* the lowest chord note takes over */
        int lo = 0;
        for (int k = 1; k < r; k++) if (rest[k] < rest[lo]) lo = k;
        uint8_t tmp = rest[0]; rest[0] = rest[lo]; rest[lo] = tmp;
        st->note = rest[0];
    } else {
        for (int k = 0; k < r; k++)   /* own note first */
            if (rest[k] == st->note) { rest[k] = rest[0]; rest[0] = st->note; break; }
    }
    set_more(s, st, rest + 1, r - 1);
    return 0;
}

void seq_step_set_chord(Seq *s, int t, int i, int chord) {
    SeqStep *st = &s->track[t].step[i];
    uint8_t cn[SEQ_CHORD_MAX];
    int nc = gen_chord_notes(s, st->note, chord, cn);
    set_more(s, st, cn, nc);
    if (nc) st->on = 1;
}

int seq_step_chord_shape(const Seq *s, const SeqStep *st) {
    int m = seq_step_more_count(st);
    if (!m) return CHORD_OFF;
    SeqStep tmp = *st;
    for (int c = CHORD_OFF + 1; c < CHORD_COUNT; c++) {
        uint8_t cn[SEQ_CHORD_MAX];
        int nc = gen_chord_notes(s, st->note, c, cn);
        set_more(s, &tmp, cn, nc);
        if (!memcmp(tmp.more, st->more, sizeof tmp.more)) return c;
    }
    return -1;
}

void seq_track_poly(Seq *s, int t, int poly) {
    SeqTrack *tr = &s->track[t];
    release(s, tr);
    tr->poly = poly != 0;
}

void seq_step_adjust(Seq *s, int t, int i, int dn, int dv, int dg, int dp) {
    SeqStep *st = &s->track[t].step[i];
    if (dn) { st->note = (uint8_t)gen_step_note(s, st->note, dn); st->on = 1; }
    st->vel = (uint8_t)clampi(st->vel + dv, 1, 127);
    st->gate = (uint8_t)clampi(st->gate + dg, 5, 100);
    st->prob = (uint8_t)clampi(st->prob + dp, 0, 100);
}

void seq_step_hold(Seq *s, int t, int i, int d) {
    SeqStep *st = &s->track[t].step[i];
    st->hold = (uint8_t)clampi(st->hold + d, 0, SEQ_MAX_HOLD);
}

void seq_step_nudge(Seq *s, int t, int i, int d) {
    SeqStep *st = &s->track[t].step[i];
    st->nudge = (int8_t)clampi(st->nudge + d, -SEQ_MAX_NUDGE, SEQ_MAX_NUDGE);
    if (live_offset(&s->track[t])) s->track[t].jit[i] = draw_offset(s, &s->track[t], st);
}

void seq_track_lock_toggle(Seq *s, int t) { s->track[t].locked ^= 1; }

int seq_step_locked(const Seq *s, int t, int i) {
    const SeqTrack *tr = &s->track[t];
    if (i < 0 || i >= SEQ_STEPS) return 0;
    return tr->type == TRACK_DRUMS ? (int)((tr->drum_step_lock >> i) & 1) : tr->step[i].lock;
}

void seq_step_lock_toggle(Seq *s, int t, int i) {
    SeqTrack *tr = &s->track[t];
    if (i < 0 || i >= SEQ_STEPS) return;
    if (tr->type == TRACK_DRUMS) tr->drum_step_lock ^= 1u << i;
    else tr->step[i].lock ^= 1;
}

/* ---- ALL STEPS ---- */

static int all_get(const SeqStep *st, int what) {
    switch (what) {
    case ALL_VEL: return st->vel;
    case ALL_GATE: return st->gate;
    case ALL_HOLD: return st->hold;
    case ALL_NUDGE: return st->nudge;
    default: return st->prob;
    }
}

static void all_put(Seq *s, int t, int i, int what, int v) {
    SeqTrack *tr = &s->track[t];
    SeqStep *st = &tr->step[i];
    switch (what) {
    case ALL_VEL: st->vel = (uint8_t)clampi(v, 1, 127); break;
    case ALL_GATE: st->gate = (uint8_t)clampi(v, 5, 100); break;
    case ALL_HOLD: st->hold = (uint8_t)clampi(v, 0, SEQ_MAX_HOLD); break;
    case ALL_NUDGE:
        st->nudge = (int8_t)clampi(v, -SEQ_MAX_NUDGE, SEQ_MAX_NUDGE);
        if (live_offset(tr)) tr->jit[i] = draw_offset(s, tr, st);
        break;
    default: st->prob = (uint8_t)clampi(v, 0, 100); break;
    }
}

/* add: 1 = v + value, 0 = value */
static void all_apply(Seq *s, int t, int what, int value, int add) {
    SeqTrack *tr = &s->track[t];
    if (what < 0 || what >= ALL_KINDS) return;
    if (tr->type == TRACK_DRUMS) {
        if (what != ALL_VEL && what != ALL_PROB) return;
        for (int l = 0; l < SEQ_LANES; l++)
            for (int i = 0; i < tr->len; i++) {
                uint8_t *v = what == ALL_VEL ? &tr->drum.vel[l][i] : &tr->drum.prob[l][i];
                if (!tr->drum.vel[l][i]) continue;   /* only hits; a velocity of 0 would remove the hit */
                int nv = add ? *v + value : value;
                *v = (uint8_t)(what == ALL_VEL ? clampi(nv, 1, 127) : clampi(nv, 0, 100));
            }
        return;
    }
    for (int i = 0; i < tr->len; i++) all_put(s, t, i, what, add ? all_get(&tr->step[i], what) + value : value);
}

void seq_all_adjust(Seq *s, int t, int what, int delta) { all_apply(s, t, what, delta, 1); }
void seq_all_set(Seq *s, int t, int what, int value) { all_apply(s, t, what, value, 0); }

int seq_all_range(const Seq *s, int t, int what, int *lo, int *hi, int *avg) {
    const SeqTrack *tr = &s->track[t];
    int n = 0, sum = 0, mn = 1 << 20, mx = -(1 << 20);
    for (int i = 0; i < tr->len; i++) {
        if (tr->type == TRACK_DRUMS) {
            if (what != ALL_VEL && what != ALL_PROB) break;
            for (int l = 0; l < SEQ_LANES; l++) {
                if (!tr->drum.vel[l][i]) continue;
                int v = what == ALL_VEL ? tr->drum.vel[l][i] : tr->drum.prob[l][i];
                n++; sum += v; mn = v < mn ? v : mn; mx = v > mx ? v : mx;
            }
            continue;
        }
        if (!tr->step[i].on) continue;
        int v = all_get(&tr->step[i], what);
        n++; sum += v; mn = v < mn ? v : mn; mx = v > mx ? v : mx;
    }
    if (!n) mn = mx = 0;
    *lo = mn; *hi = mx;
    /* rounded half away from zero, so an average offset of -2.5 is -3 */
    *avg = !n ? 0 : sum >= 0 ? (sum + n / 2) / n : -((-sum + n / 2) / n);
    return n;
}

int seq_locked_steps(const Seq *s, int t) {
    int n = 0;
    for (int i = 0; i < s->track[t].len; i++) n += seq_step_locked(s, t, i);
    return n;
}

int seq_all_lock_toggle(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    int lock = seq_locked_steps(s, t) == 0;
    for (int i = 0; i < SEQ_STEPS; i++) {
        int in = i < tr->len;
        if (tr->type == TRACK_DRUMS) {
            if (lock && in) tr->drum_step_lock |= 1u << i;
            else if (!lock) tr->drum_step_lock &= ~(1u << i);
        } else if (lock ? in : 1) tr->step[i].lock = (uint8_t)lock;
    }
    return lock;
}

int seq_any_solo(const Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++) if (s->track[t].solo) return 1;
    return 0;
}

int seq_track_audible(const Seq *s, int t) {
    const SeqTrack *tr = &s->track[t];
    return !tr->mute && (tr->solo || !seq_any_solo(s));
}

/* After a mute or solo change: note-off for anything that just went silent. */
static void release_silenced(Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++)
        if (!seq_track_audible(s, t)) release(s, &s->track[t]);
}

void seq_track_mute_toggle(Seq *s, int t) {
    s->track[t].mute ^= 1;
    release_silenced(s);
}

void seq_track_solo_toggle(Seq *s, int t) {
    s->track[t].solo ^= 1;
    release_silenced(s);
}

int seq_lane_audible(const SeqTrack *tr, int l) {
    return !(tr->drum_mute & (1 << l)) && (!tr->drum_solo || (tr->drum_solo & (1 << l)));
}

static void release_silenced_lanes(Seq *s, SeqTrack *tr) {
    for (int l = 0; l < SEQ_LANES; l++) if (!seq_lane_audible(tr, l)) release_lane(s, tr, l);
}

void seq_lane_mute_toggle(Seq *s, int t, int l) {
    if (l < 0 || l >= SEQ_LANES) return;
    s->track[t].drum_mute ^= (uint8_t)(1 << l);
    release_silenced_lanes(s, &s->track[t]);
}

void seq_lane_solo_toggle(Seq *s, int t, int l) {
    if (l < 0 || l >= SEQ_LANES) return;
    s->track[t].drum_solo ^= (uint8_t)(1 << l);
    release_silenced_lanes(s, &s->track[t]);
}

void seq_track_len(Seq *s, int t, int d) {
    SeqTrack *tr = &s->track[t];
    tr->len = (uint8_t)clampi(tr->len + d, 1, SEQ_STEPS);
}

void seq_track_channel(Seq *s, int t, int d) {
    SeqTrack *tr = &s->track[t];
    release(s, tr);
    tr->channel = (uint8_t)clampi(tr->channel + d, 0, 15);
}

void seq_adjust_bpm(Seq *s, int d) { s->bpm = clampi(s->bpm + d, 30, 300); }
void seq_adjust_swing(Seq *s, int d) { s->swing = clampi(s->swing + d, 0, SEQ_MAX_SWING); }

/* The offset a step plays at next: its own OFFSET plus, with live shuffle,
 * a random amount up to `shuffle` either side; with NEVER ON GRID, never
 * closer than SEQ_OFFGRID_MIN ticks to the grid. */
static int8_t draw_offset(Seq *s, const SeqTrack *tr, const SeqStep *st) {
    int a = tr->shuffle, own = tr->type == TRACK_DRUMS ? 0 : st->nudge;
    for (int tries = 0; tries < 16; tries++) {
        int v = own + (a ? (int)(seq_rand(s) % (uint32_t)(2 * a + 1)) - a : 0);
        v = clampi(v, -SEQ_MAX_NUDGE, SEQ_MAX_NUDGE);
        if (!tr->offgrid || v <= -SEQ_OFFGRID_MIN || v >= SEQ_OFFGRID_MIN) return (int8_t)v;
        if (!a) break;   /* nothing random to retry with */
    }
    /* still too close: push it off the grid, the way it already leans, else either way */
    int v = own;
    int dir = v > 0 ? 1 : v < 0 ? -1 : (seq_rand(s) & 1) ? 1 : -1;
    return (int8_t)(dir * SEQ_OFFGRID_MIN);
}

static int live_offset(const SeqTrack *tr) { return tr->shuffle || tr->offgrid; }

void seq_reshuffle(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    for (int i = 0; i < SEQ_STEPS; i++) tr->jit[i] = live_offset(tr) ? draw_offset(s, tr, &tr->step[i]) : 0;
}

void seq_track_shuffle(Seq *s, int t, int d) {
    SeqTrack *tr = &s->track[t];
    tr->shuffle = (uint8_t)clampi(tr->shuffle + d, 0, SEQ_MAX_NUDGE);
    seq_reshuffle(s, t);
}

void seq_track_offgrid(Seq *s, int t, int on) {
    s->track[t].offgrid = on != 0;
    seq_reshuffle(s, t);
}

void seq_set_scale(Seq *s, int scale, int root, int snap) {
    s->scale = clampi(scale, 0, scale_count() - 1);
    s->root = ((root % 12) + 12) % 12;
    if (!snap) return;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        for (int i = 0; i < SEQ_STEPS; i++) {
            tr->step[i].note = (uint8_t)gen_snap_note(s, tr->step[i].note);
            tr->anchor[i].note = (uint8_t)gen_snap_note(s, tr->anchor[i].note);
        }
    }
}

void seq_slot_store(Seq *s, int t, int k) {
    if (k < 0 || k >= SEQ_SLOTS) return;
    SeqTrack *tr = &s->track[t];
    memcpy(tr->slot[k].step, tr->step, sizeof tr->step);
    tr->slot[k].len = tr->len;
    tr->slot[k].drum = tr->drum;
    tr->slot[k].used = 1;
    tr->cur_slot = (uint8_t)k;
}

int seq_slot_recall(Seq *s, int t, int k) {
    if (k < 0 || k >= SEQ_SLOTS || !s->track[t].slot[k].used) return -1;
    SeqTrack *tr = &s->track[t];
    memcpy(tr->step, tr->slot[k].step, sizeof tr->step);
    tr->drum = tr->slot[k].drum;
    tr->len = tr->slot[k].len;
    tr->cur_slot = (uint8_t)k;
    return 0;
}

void seq_slot_clear(Seq *s, int t, int k) {
    if (k < 0 || k >= SEQ_SLOTS) return;
    SeqSlot *sl = &s->track[t].slot[k];
    memset(sl, 0, sizeof *sl);
    memset(sl->drum.prob, 100, sizeof sl->drum.prob);
}

int seq_slot_copy(Seq *s, int t, int from, int to) {
    if (from < 0 || from >= SEQ_SLOTS || to < 0 || to >= SEQ_SLOTS || !s->track[t].slot[from].used) return -1;
    if (from != to) s->track[t].slot[to] = s->track[t].slot[from];
    return 0;
}

static void slot_default_name(int k, char *out) { snprintf(out, SEQ_SLOT_NAME + 1, "SLOT %d", (k & 7) + 1); }

void seq_slot_set_name(Seq *s, int t, int k, const char *name) {
    if (k < 0 || k >= SEQ_SLOTS) return;
    char buf[SEQ_SLOT_NAME + 1], def[SEQ_SLOT_NAME + 1];
    int n = 0;
    for (; name && name[n] && n < SEQ_SLOT_NAME; n++) {
        char c = name[n];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        buf[n] = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ? c : ' ';
    }
    while (n > 0 && buf[n - 1] == ' ') n--;
    buf[n] = 0;
    slot_default_name(k, def);
    if (!strcmp(buf, def)) buf[0] = 0;
    memcpy(s->track[t].slot[k].name, buf, sizeof buf);
}

void seq_slot_name(const Seq *s, int t, int k, char *out) {
    if (k >= 0 && k < SEQ_SLOTS && s->track[t].slot[k].name[0]) snprintf(out, SEQ_SLOT_NAME + 1, "%s", s->track[t].slot[k].name);
    else slot_default_name(k, out);
}

void seq_chain_set(Seq *s, int t, const uint8_t *seq, int n, int bars, int morph) {
    SeqTrack *tr = &s->track[t];
    n = clampi(n, 0, SEQ_SLOTS);
    for (int i = 0; i < n; i++) tr->chain[i] = (uint8_t)clampi(seq[i], 0, SEQ_SLOTS - 1);
    tr->chain_len = (uint8_t)n;
    tr->chain_pos = 0;
    tr->chain_bars = (uint8_t)(bars <= 1 ? 1 : bars <= 2 ? 2 : bars <= 4 ? 4 : bars <= 8 ? 8 : 16);
    tr->chain_morph = (uint8_t)clampi(morph, 0, 100);
    if (n) seq_slot_recall(s, t, tr->chain[0]);
}

void seq_chain_bar(Seq *s, int t, uint32_t bar) {
    SeqTrack *tr = &s->track[t];
    if (!tr->chain_len) return;
    if (bar % tr->chain_bars == 0) {
        tr->chain_pos = (uint8_t)((tr->chain_pos + 1) % tr->chain_len);
        int k = tr->chain[tr->chain_pos];
        if (!tr->chain_morph) { seq_slot_recall(s, t, k); return; }
        if (tr->slot[k].used) { tr->len = tr->slot[k].len; tr->cur_slot = (uint8_t)k; }
    }
    if (tr->chain_morph) {
        const SeqSlot *sl = &tr->slot[tr->chain[tr->chain_pos]];
        if (sl->used) { gen_morph(s, t, sl->step, tr->chain_morph); gen_drum_morph(s, t, &sl->drum, tr->chain_morph); }
    }
}

void seq_note_name(int note, char out[8]) {
    static const char *n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    note = clampi(note, 0, 127);
    snprintf(out, 8, "%s%d", n[note % 12], note / 12 - 1);
}

/* Drum hits: "PREFIX ... LANE STEP VEL PROB", one line per hit. */
static void save_hits(FILE *f, const char *prefix, const SeqDrums *d) {
    for (int l = 0; l < SEQ_LANES; l++)
        for (int i = 0; i < SEQ_STEPS; i++)
            if (d->vel[l][i]) fprintf(f, "%s %d %d %d %d\n", prefix, l, i, d->vel[l][i], d->prob[l][i]);
}

static void load_hit(SeqDrums *d, int l, int i, int v, int p) {
    if (l < 0 || l >= SEQ_LANES || i < 0 || i >= SEQ_STEPS) return;
    d->vel[l][i] = (uint8_t)clampi(v, 0, 127);
    d->prob[l][i] = (uint8_t)clampi(p, 0, 100);
}

void seq_save(const Seq *s, FILE *f) {
    char pre[32];
    fprintf(f, "seq4h 1\nbpm %d\nswing %d\nclock %d\ntransport %d\nscale %d %d\nstyle %d\n", s->bpm, s->swing, s->clock_out, s->transport_out, s->scale, s->root, s->style);
    fprintf(f, "dmv 2\n");   /* DRUM MAP version: 2 = FELUCCA is the default (see seq_load) */
    for (int l = 0; l < SEQ_LANES; l++) {   /* DRUM MAP: "dm LANE NOTE NAME", spaces as '_'; older versions skip these lines */
        char nm[SEQ_LANE_NAME + 1];
        snprintf(nm, sizeof nm, "%s", s->lane_name[l][0] ? s->lane_name[l] : seq_lane_gm_name(l));
        for (char *c = nm; *c; c++) if (*c == ' ') *c = '_';
        fprintf(f, "dm %d %d %s\n", l, s->lane_note[l], nm);
    }
    for (int t = 0; t < SEQ_TRACKS; t++) {
        const SeqTrack *tr = &s->track[t];
        fprintf(f, "t %d %d %d %d\n", t, tr->channel, tr->len, tr->mute);
        fprintf(f, "ts %d %d %d\n", t, tr->style, tr->part);   /* the track's style and part; older versions skip it */
        if (tr->type || tr->drum_lock || tr->drum_busy != 50 || tr->drum_mute)   /* drum track: "ty T TYPE LOCKS BUSY MUTES" */
            fprintf(f, "ty %d %d %d %d %d\n", t, tr->type, tr->drum_lock, tr->drum_busy, tr->drum_mute);
        fprintf(f, "pc %d %d %d %d %d\n", t, tr->chord, tr->swell, tr->swell_depth, tr->rest);
        if (tr->shuffle || tr->offgrid) fprintf(f, "sf %d %d %d\n", t, tr->shuffle, tr->offgrid);   /* older versions skip it */
        if (tr->poly) fprintf(f, "vo %d %d\n", t, tr->poly);   /* VOICES POLY; older versions skip it */
        if (tr->locked) fprintf(f, "tl %d 1\n", t);                                     /* track lock; older versions skip it */
        if (tr->drum_step_lock) fprintf(f, "dl %d %u\n", t, (unsigned)tr->drum_step_lock);   /* drum step locks, bit per step */
        fprintf(f, "g %d %d %d %d %d %d %d %d\n", t, tr->gen.type, tr->gen.p[0], tr->gen.p[1], tr->gen.p[2],
                tr->gen.p[3], tr->gen.base, tr->gen.span);
        fprintf(f, "e %d %d %d %d\n", t, tr->evo.every, tr->evo.amount, tr->evo.kinds);
        fprintf(f, "ev %d %d %d %d\n", t, tr->evo.pull, tr->evo.min_hits, tr->evo.max_hits);
        fprintf(f, "gs %d %u\n", t, (unsigned)tr->gen.state);
        for (int k = 0; k < SEQ_SLOTS; k++) {
            const SeqSlot *sl = &tr->slot[k];
            if (sl->name[0]) {   /* "sn TRACK SLOT NAME", spaces written as '_'; older versions skip this line */
                char nm[SEQ_SLOT_NAME + 1];
                snprintf(nm, sizeof nm, "%s", sl->name);
                for (char *c = nm; *c; c++) if (*c == ' ') *c = '_';
                fprintf(f, "sn %d %d %s\n", t, k, nm);
            }
            if (!sl->used) continue;
            fprintf(f, "sl %d %d %d\n", t, k, sl->len);
            for (int i = 0; i < SEQ_STEPS; i++) {
                const SeqStep *st = &sl->step[i];
                fprintf(f, "ss %d %d %d %d %d %d %d %d %d\n", t, k, i, st->on, st->note, st->vel, st->gate, st->prob, st->lock);
                if (st->hold) fprintf(f, "sh %d %d %d %d\n", t, k, i, st->hold);
                if (st->nudge) fprintf(f, "so %d %d %d %d\n", t, k, i, st->nudge);
                if (st->more[0]) fprintf(f, "sx %d %d %d %d %d %d\n", t, k, i, st->more[0], st->more[1], st->more[2]);
            }
            snprintf(pre, sizeof pre, "dsh %d %d", t, k);
            save_hits(f, pre, &sl->drum);
        }
        fprintf(f, "ch %d %d %d %d %d %d %d", t, tr->cur_slot, tr->chain_len, tr->chain_pos, tr->chain_bars, tr->chain_morph, SEQ_SLOTS);
        for (int k = 0; k < SEQ_SLOTS; k++) fprintf(f, " %d", tr->chain[k]);
        fputc('\n', f);
        if (tr->has_anchor) { snprintf(pre, sizeof pre, "dah %d", t); save_hits(f, pre, &tr->drum_anchor); }
        if (tr->has_anchor)
            for (int i = 0; i < SEQ_STEPS; i++) {
                const SeqStep *st = &tr->anchor[i];
                fprintf(f, "a %d %d %d %d %d %d %d\n", t, i, st->on, st->note, st->vel, st->gate, st->prob);
                if (st->hold) fprintf(f, "ah %d %d %d\n", t, i, st->hold);
                if (st->nudge) fprintf(f, "ao %d %d %d\n", t, i, st->nudge);
                if (st->more[0]) fprintf(f, "ax %d %d %d %d %d\n", t, i, st->more[0], st->more[1], st->more[2]);
            }
        for (int i = 0; i < SEQ_STEPS; i++) {
            const SeqStep *st = &tr->step[i];
            fprintf(f, "s %d %d %d %d %d %d %d %d\n", t, i, st->on, st->note, st->vel, st->gate, st->prob, st->lock);
            if (st->hold) fprintf(f, "h %d %d %d\n", t, i, st->hold);
            if (st->nudge) fprintf(f, "o %d %d %d\n", t, i, st->nudge);   /* offset; older versions skip it */
            /* chord notes, in scale degrees from the step's note; older versions skip it */
            if (st->more[0]) fprintf(f, "x %d %d %d %d %d\n", t, i, st->more[0], st->more[1], st->more[2]);
        }
        snprintf(pre, sizeof pre, "dh %d", t);
        save_hits(f, pre, &tr->drum);
    }
}

int seq_save_file(const Seq *s, const char *path) {
    size_t n = strlen(path) + 5;
    char *tmp = malloc(n);
    if (!tmp) return -1;
    snprintf(tmp, n, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    int rc = -1;
    if (f) {
        seq_save(s, f);
        /* on the SD card before the rename, so a power cut leaves the old or the new file, never half of one */
        rc = (fflush(f) == 0 && fsync(fileno(f)) == 0) ? 0 : -1;
        if (fclose(f) != 0) rc = -1;
        if (rc == 0 && rename(tmp, path) != 0) rc = -1;
        if (rc != 0) remove(tmp);
    }
    free(tmp);
    return rc;
}

int seq_slots_used(const Seq *s) {
    int n = 0;
    for (int t = 0; t < SEQ_TRACKS; t++)
        for (int k = 0; k < SEQ_SLOTS; k++) n += s->track[t].slot[k].used;
    return n;
}

/* Chord notes from a file: in range, the used ones first, no repeats. */
static void load_more(SeqStep *st, int a, int b, int c) {
    int v[3] = {a, b, c}, m = 0;
    memset(st->more, 0, sizeof st->more);
    for (int k = 0; k < 3; k++) {
        if (!v[k] || v[k] < -SEQ_MORE_MAX || v[k] > SEQ_MORE_MAX) continue;
        int dup = 0;
        for (int j = 0; j < m; j++) dup |= st->more[j] == v[k];
        if (!dup) st->more[m++] = (int8_t)v[k];
    }
}

int seq_load(Seq *s, FILE *f) {
    char line[128], nm[16];
    int a, b, c, d, e, g, h, k, m, n;
    unsigned u;
    int map_lines = 0, map_version = 1;
    if (!fgets(line, sizeof line, f) || strncmp(line, "seq4h ", 6) != 0) return -1;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'd') {   /* drum lines */
            if (sscanf(line, "dmv %d", &a) == 1) map_version = a;
            else if (sscanf(line, "dl %d %u", &a, &u) == 2) {
                if (a >= 0 && a < SEQ_TRACKS) s->track[a].drum_step_lock = (uint32_t)u;
            }
            else if (sscanf(line, "dh %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
                if (a >= 0 && a < SEQ_TRACKS) load_hit(&s->track[a].drum, b, c, d, e);
            } else if (sscanf(line, "dah %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
                if (a >= 0 && a < SEQ_TRACKS) load_hit(&s->track[a].drum_anchor, b, c, d, e);
            } else if (sscanf(line, "dsh %d %d %d %d %d %d", &a, &k, &b, &c, &d, &e) == 6) {
                if (a >= 0 && a < SEQ_TRACKS && k >= 0 && k < SEQ_SLOTS) load_hit(&s->track[a].slot[k].drum, b, c, d, e);
            } else if (sscanf(line, "dm %d %d %15s", &a, &b, nm) == 3) {
                if (a < 0 || a >= SEQ_LANES) continue;
                s->lane_note[a] = (uint8_t)clampi(b, 0, 127);
                map_lines++;
                for (char *q = nm; *q; q++) if (*q == '_') *q = ' ';
                seq_lane_set_name(s, a, nm);
            }
            continue;
        }
        if ((n = sscanf(line, "ty %d %d %d %d %d", &a, &b, &c, &d, &e)) >= 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].drum_mute = n >= 5 ? (uint8_t)e : 0;   /* older files have no lane mutes */
            s->track[a].type = b == TRACK_DRUMS ? TRACK_DRUMS : TRACK_NOTES;
            s->track[a].drum_lock = (uint8_t)c;
            s->track[a].drum_busy = (uint8_t)clampi(d, 0, 100);
        }
        else if (sscanf(line, "tl %d %d", &a, &b) == 2) {
            if (a >= 0 && a < SEQ_TRACKS) s->track[a].locked = b != 0;
        }
        else if (sscanf(line, "bpm %d", &a) == 1) s->bpm = clampi(a, 30, 300);
        else if (sscanf(line, "swing %d", &a) == 1) s->swing = clampi(a, 0, SEQ_MAX_SWING);
        else if (sscanf(line, "clock %d", &a) == 1) s->clock_out = s->transport_out = a != 0;   /* older files: one setting for both */
        else if (sscanf(line, "transport %d", &a) == 1) s->transport_out = a != 0;          /* comes after clock */
        else if (sscanf(line, "style %d", &a) == 1) {
            s->style = clampi(a, 0, style_count() - 1);
            for (int t = 0; t < SEQ_TRACKS; t++) s->track[t].style = (uint8_t)s->style;   /* files without "ts" lines */
        }
        else if (sscanf(line, "ts %d %d %d", &a, &b, &c) == 3) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].style = (uint8_t)clampi(b, 0, style_count() - 1);
            s->track[a].part = (uint8_t)clampi(c, 0, STYLE_PARTS - 1);
        }
        /* hold lines follow the step they belong to, so the step line has already reset hold */
        else if (sscanf(line, "h %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) s->track[a].step[b].hold = (uint8_t)clampi(c, 0, SEQ_MAX_HOLD);
        } else if (sscanf(line, "ah %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) s->track[a].anchor[b].hold = (uint8_t)clampi(c, 0, SEQ_MAX_HOLD);
        } else if (sscanf(line, "sh %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_SLOTS && c >= 0 && c < SEQ_STEPS)
                s->track[a].slot[b].step[c].hold = (uint8_t)clampi(d, 0, SEQ_MAX_HOLD);
        }
        /* offset lines follow their step line too */
        else if (sscanf(line, "o %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) s->track[a].step[b].nudge = (int8_t)clampi(c, -SEQ_MAX_NUDGE, SEQ_MAX_NUDGE);
        } else if (sscanf(line, "ao %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) s->track[a].anchor[b].nudge = (int8_t)clampi(c, -SEQ_MAX_NUDGE, SEQ_MAX_NUDGE);
        } else if (sscanf(line, "so %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_SLOTS && c >= 0 && c < SEQ_STEPS)
                s->track[a].slot[b].step[c].nudge = (int8_t)clampi(d, -SEQ_MAX_NUDGE, SEQ_MAX_NUDGE);
        }
        /* chord-note lines follow their step line too (it cleared them) */
        else if (sscanf(line, "x %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) load_more(&s->track[a].step[b], c, d, e);
        } else if (sscanf(line, "ax %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_STEPS) load_more(&s->track[a].anchor[b], c, d, e);
        } else if (sscanf(line, "sx %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g) == 6) {
            if (a >= 0 && a < SEQ_TRACKS && b >= 0 && b < SEQ_SLOTS && c >= 0 && c < SEQ_STEPS)
                load_more(&s->track[a].slot[b].step[c], d, e, g);
        } else if (sscanf(line, "vo %d %d", &a, &b) == 2) {
            if (a >= 0 && a < SEQ_TRACKS) s->track[a].poly = b != 0;
        }
        else if (sscanf(line, "sn %d %d %15s", &a, &b, nm) == 3) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_SLOTS) continue;
            for (char *q = nm; *q; q++) if (*q == '_') *q = ' ';
            seq_slot_set_name(s, a, b, nm);
        }
        else if (sscanf(line, "sl %d %d %d", &a, &b, &c) == 3) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_SLOTS) continue;
            s->track[a].slot[b].len = (uint8_t)clampi(c, 1, SEQ_STEPS);
            s->track[a].slot[b].used = 1;
        } else if (sscanf(line, "ss %d %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h, &k, &n) == 9) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_SLOTS || c < 0 || c >= SEQ_STEPS) continue;
            s->track[a].slot[b].step[c] = (SeqStep){(uint8_t)(d != 0), (uint8_t)clampi(e, 0, 127),
                                                    (uint8_t)clampi(g, 1, 127), (uint8_t)clampi(h, 5, 100),
                                                    (uint8_t)clampi(k, 0, 100), (uint8_t)(n != 0), 0, 0, {0}};
        } else if (sscanf(line, "ch %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h) == 7) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            SeqTrack *tr = &s->track[a];
            int ch[SEQ_SLOTS] = {0}, off = 0, pos = 0, v;
            char *q = line;
            for (int skip = 0; skip < 8 && sscanf(q, "%*s%n", &off) == 0; skip++) q += off;   /* past the 8 header fields */
            for (int i = 0; i < SEQ_SLOTS && sscanf(q, "%d%n", &v, &off) == 1; i++) { ch[i] = v; q += off; pos = i + 1; }
            tr->cur_slot = (uint8_t)clampi(b, 0, SEQ_SLOTS - 1);
            tr->chain_len = (uint8_t)clampi(c, 0, pos);
            tr->chain_pos = (uint8_t)(tr->chain_len ? clampi(d, 0, tr->chain_len - 1) : 0);
            tr->chain_bars = (uint8_t)(e <= 1 ? 1 : e <= 2 ? 2 : e <= 4 ? 4 : e <= 8 ? 8 : 16);
            tr->chain_morph = (uint8_t)clampi(g, 0, 100);
            for (int i = 0; i < SEQ_SLOTS; i++) tr->chain[i] = (uint8_t)clampi(ch[i], 0, SEQ_SLOTS - 1);
        }
        else if (sscanf(line, "scale %d %d", &a, &b) == 2) { s->scale = clampi(a, 0, scale_count() - 1); s->root = clampi(b, 0, 11); }
        else if (sscanf(line, "g %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h, &k) == 8) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            gen_load_settings(&s->track[a].gen, b, c, d, e, g, h, k);
        } else if (sscanf(line, "gs %d %u", &a, &u) == 2) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].gen.state = u ? u : 1;
        } else if (sscanf(line, "ev %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            EvolveSettings *ev = &s->track[a].evo;
            ev->pull = (uint8_t)clampi(b, 0, 100);
            ev->min_hits = (uint8_t)clampi(c, 0, SEQ_STEPS);
            ev->max_hits = (uint8_t)(d <= 0 ? 0 : clampi(d, ev->min_hits, SEQ_STEPS));
        } else if (sscanf(line, "a %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h) == 7) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_STEPS) continue;
            s->track[a].anchor[b] = (SeqStep){(uint8_t)(c != 0), (uint8_t)clampi(d, 0, 127),
                                              (uint8_t)clampi(e, 1, 127), (uint8_t)clampi(g, 5, 100),
                                              (uint8_t)clampi(h, 0, 100), 0, 0, 0, {0}};
            s->track[a].has_anchor = 1;
        } else if (sscanf(line, "e %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            EvolveSettings *ev = &s->track[a].evo;
            ev->every = (uint8_t)clampi(b, 0, SEQ_MAX_EVERY);
            ev->amount = (uint8_t)clampi(c, 0, 100);
            ev->kinds = (uint8_t)(d & EVO_ALL);
        } else if (sscanf(line, "sf %d %d %d", &a, &b, &c) == 3) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].shuffle = (uint8_t)clampi(b, 0, SEQ_MAX_NUDGE);
            s->track[a].offgrid = c != 0;
        } else if (sscanf(line, "pc %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            SeqTrack *tr = &s->track[a];
            tr->chord = (uint8_t)clampi(b, 0, CHORD_COUNT - 1);
            tr->swell = (uint8_t)(c <= 0 ? 0 : c <= 2 ? 2 : c <= 4 ? 4 : c <= 8 ? 8 : c <= 16 ? 16 : 32);
            tr->swell_depth = (uint8_t)clampi(d, 0, 100);
            tr->rest = (uint8_t)clampi(e, 0, 100);
        } else if (sscanf(line, "t %d %d %d %d", &a, &b, &c, &d) == 4) {
            if (a < 0 || a >= SEQ_TRACKS) continue;
            s->track[a].channel = (uint8_t)clampi(b, 0, 15);
            s->track[a].len = (uint8_t)clampi(c, 1, SEQ_STEPS);
            s->track[a].mute = d != 0;
        } else if ((m = sscanf(line, "s %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h, &n)) >= 7) {
            if (a < 0 || a >= SEQ_TRACKS || b < 0 || b >= SEQ_STEPS) continue;
            s->track[a].step[b] = (SeqStep){(uint8_t)(c != 0), (uint8_t)clampi(d, 0, 127),
                                            (uint8_t)clampi(e, 1, 127), (uint8_t)clampi(g, 5, 100),
                                            (uint8_t)clampi(h, 0, 100), (uint8_t)(m == 8 && n != 0), 0, 0, {0}};
        }
    }
    /* The first drum build (PR 23) defaulted to the GM map and wrote it to
     * every pattern file; a file that still has exactly that map gets the
     * FELUCCA map, the default now. A map someone changed is kept. */
    if (map_lines && map_version < 2 && seq_drum_map_is(s, DRUM_GM)) seq_drum_map_preset(s, DRUM_FELUCCA);
    return 0;
}
