#include "gen.h"

#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))

static const struct { const char *name; uint16_t mask; } SCALES[] = {
    {"CHROMATIC", 0xFFF},
    {"MAJOR", BIT(0) | BIT(2) | BIT(4) | BIT(5) | BIT(7) | BIT(9) | BIT(11)},
    {"MINOR", BIT(0) | BIT(2) | BIT(3) | BIT(5) | BIT(7) | BIT(8) | BIT(10)},
    {"DORIAN", BIT(0) | BIT(2) | BIT(3) | BIT(5) | BIT(7) | BIT(9) | BIT(10)},
    {"PHRYGIAN", BIT(0) | BIT(1) | BIT(3) | BIT(5) | BIT(7) | BIT(8) | BIT(10)},
    {"LYDIAN", BIT(0) | BIT(2) | BIT(4) | BIT(6) | BIT(7) | BIT(9) | BIT(11)},
    {"MIXOLYDIAN", BIT(0) | BIT(2) | BIT(4) | BIT(5) | BIT(7) | BIT(9) | BIT(10)},
    {"HARM MINOR", BIT(0) | BIT(2) | BIT(3) | BIT(5) | BIT(7) | BIT(8) | BIT(11)},
    {"MAJ PENT", BIT(0) | BIT(2) | BIT(4) | BIT(7) | BIT(9)},
    {"MIN PENT", BIT(0) | BIT(3) | BIT(5) | BIT(7) | BIT(10)},
    {"BLUES", BIT(0) | BIT(3) | BIT(5) | BIT(6) | BIT(7) | BIT(10)},
    {"WHOLE TONE", BIT(0) | BIT(2) | BIT(4) | BIT(6) | BIT(8) | BIT(10)},
};
#define NSCALES ((int)(sizeof SCALES / sizeof SCALES[0]))

int scale_count(void) { return NSCALES; }
const char *scale_name(int sc) { return SCALES[sc < 0 || sc >= NSCALES ? 0 : sc].name; }

int seq_in_scale(const Seq *s, int note) {
    int pc = ((note - s->root) % 12 + 12) % 12;
    return (SCALES[s->scale < 0 || s->scale >= NSCALES ? 0 : s->scale].mask >> pc) & 1;
}

int gen_pool(const Seq *s, int t, uint8_t pool[64]) {
    const GenSettings *g = &s->track[t].gen;
    int n = 0, hi = g->base + 12 * g->span;
    if (hi > 127) hi = 127;
    for (int note = g->base; note <= hi && n < 64; note++)
        if (seq_in_scale(s, note)) pool[n++] = (uint8_t)note;
    if (n == 0) pool[n++] = g->base > 127 ? 127 : g->base;
    return n;
}

int gen_step_note(const Seq *s, int note, int steps) {
    int dir = steps < 0 ? -1 : 1;
    for (int k = steps < 0 ? -steps : steps; k > 0; k--) {
        int nn = note + dir;
        while (nn >= 0 && nn <= 127 && !seq_in_scale(s, nn)) nn += dir;
        if (nn < 0 || nn > 127) break;
        note = nn;
    }
    return note < 0 ? 0 : note > 127 ? 127 : note;
}

/* ---- generator descriptions ---- */

typedef struct { const char *name; uint8_t min, max, def; } Param;
static const struct { const char *name; int n; Param p[4]; } GENS[GEN_COUNT] = {
    [GEN_NONE] = {"MANUAL", 0, {{0}}},
    [GEN_EUCLID] = {"EUCLID", 3, {{"HITS", 0, 16, 5}, {"ROTATE", 0, 15, 0}, {"PITCH", 0, 2, 0}}},
    [GEN_WALK] = {"WALK", 3, {{"STEP", 1, 7, 2}, {"BIAS", 0, 10, 5}, {"DENSITY", 1, 10, 7}}},
    [GEN_MARKOV] = {"MARKOV", 2, {{"CHAOS", 0, 10, 1}, {"RHYTHM", 0, 10, 0}}},
    [GEN_CHAOS] = {"CHAOS", 3, {{"R", 0, 10, 8}, {"SEED", 0, 15, 3}, {"DENSITY", 1, 10, 7}}},
    [GEN_CELLULAR] = {"CELLULAR", 3, {{"RULE", 0, 11, 1}, {"GENS", 0, 63, 8}, {"SEED", 0, 1, 0}}},
    [GEN_WEIGHTED] = {"WEIGHTED", 3, {{"TENSION", 0, 7, 3}, {"DENSITY", 1, 10, 7}, {"REPEAT", 0, 10, 2}}},
};
static const uint8_t CA_RULES[12] = {30, 90, 110, 150, 18, 22, 45, 54, 60, 105, 126, 184};

static int tidx(int type) { return type < 0 || type >= GEN_COUNT ? 0 : type; }
const char *gen_name(int type) { return GENS[tidx(type)].name; }
int gen_param_count(int type) { return GENS[tidx(type)].n; }
const char *gen_param_name(int type, int k) { return GENS[tidx(type)].p[k].name; }
int gen_param_min(int type, int k) { return GENS[tidx(type)].p[k].min; }
int gen_param_max(int type, int k) { return GENS[tidx(type)].p[k].max; }

void gen_param_text(int type, int k, int v, char out[16]) {
    switch (type) {
    case GEN_EUCLID:
        if (k == 2) { snprintf(out, 16, "%s", v == 0 ? "KEEP" : v == 1 ? "ROOT" : "RANDOM"); return; }
        break;
    case GEN_WALK:
        if (k == 1) { snprintf(out, 16, "%+d", v - 5); return; }
        if (k == 2) { snprintf(out, 16, "%d%%", v * 10); return; }
        break;
    case GEN_MARKOV:
        if (k == 0) { snprintf(out, 16, "%d%%", v * 10); return; }
        if (k == 1) { if (v) snprintf(out, 16, "%d%%", v * 10); else snprintf(out, 16, "KEEP"); return; }
        break;
    case GEN_CHAOS:
        if (k == 0) { snprintf(out, 16, "%.2f", 3.5 + v * 0.05); return; }
        if (k == 2) { snprintf(out, 16, "%d%%", v * 10); return; }
        break;
    case GEN_CELLULAR:
        if (k == 0) { snprintf(out, 16, "RULE %d", CA_RULES[v % 12]); return; }
        if (k == 2) { snprintf(out, 16, "%s", v ? "RANDOM" : "SINGLE"); return; }
        break;
    case GEN_WEIGHTED:
        if (k >= 1) { snprintf(out, 16, "%d%%", v * 10); return; }
        break;
    default: break;
    }
    snprintf(out, 16, "%d", v);
}

void gen_set_type(Seq *s, int t, int type) {
    GenSettings *g = &s->track[t].gen;
    g->type = (uint8_t)tidx(type);
    for (int k = 0; k < 4; k++) g->p[k] = k < GENS[g->type].n ? GENS[g->type].p[k].def : 0;
}

void gen_load_settings(GenSettings *g, int type, int p0, int p1, int p2, int p3, int base, int span) {
    int in[4] = {p0, p1, p2, p3};
    g->type = (uint8_t)tidx(type);
    for (int k = 0; k < 4; k++) {
        if (k < GENS[g->type].n) {
            int lo = GENS[g->type].p[k].min, hi = GENS[g->type].p[k].max;
            g->p[k] = (uint8_t)(in[k] < lo ? lo : in[k] > hi ? hi : in[k]);
        } else {
            g->p[k] = 0;
        }
    }
    g->base = (uint8_t)(base < 0 ? 0 : base > 115 ? 115 : base);
    g->span = (uint8_t)(span < 1 ? 1 : span > 4 ? 4 : span);
}

/* ---- generation ---- */

static int nearest(const uint8_t *pool, int n, int note) {
    int best = 0, bd = 1000;
    for (int i = 0; i < n; i++) {
        int d = pool[i] > note ? pool[i] - note : note - pool[i];
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static int pick(Seq *s, int n) { return (int)(seq_rand(s) % (uint32_t)n); }
static int chance(Seq *s, int pct) { return (int)(seq_rand(s) % 100) < pct; }

void gen_apply(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    const GenSettings *g = &tr->gen;
    if (g->type == GEN_NONE || g->type >= GEN_COUNT) return;

    uint8_t pool[64];
    int n = gen_pool(s, t, pool), len = tr->len;
    int on[SEQ_STEPS], idx[SEQ_STEPS];
    for (int i = 0; i < SEQ_STEPS; i++) { on[i] = tr->step[i].on; idx[i] = -1; }
    const uint8_t *p = g->p;

    switch (g->type) {
    case GEN_EUCLID: {
        int hits = p[0] > len ? len : p[0];
        for (int i = 0; i < len; i++) {
            int j = (i + p[1]) % len;
            on[i] = (j * hits) % len < hits;
            if (p[2] == 1) idx[i] = 0;
            else if (p[2] == 2) idx[i] = pick(s, n);
        }
        break;
    }
    case GEN_WALK: {
        int pos = nearest(pool, n, tr->step[0].note);
        for (int i = 0; i < len; i++) {
            int d = 1 + pick(s, p[0] ? p[0] : 1);
            pos += chance(s, 50 + ((int)p[1] - 5) * 10) ? d : -d;
            if (pos < 0) pos = -pos;
            if (pos >= n) pos = 2 * (n - 1) - pos;
            if (pos < 0) pos = 0;
            if (pos >= n) pos = n - 1;
            idx[i] = pos;
            on[i] = chance(s, p[2] * 10);
        }
        break;
    }
    case GEN_MARKOV: {
        uint8_t cnt[64][64];
        memset(cnt, 0, sizeof cnt);
        int first = -1, prev = -1;
        for (int i = 0; i < len; i++) {
            if (!tr->step[i].on) continue;
            int k = nearest(pool, n, tr->step[i].note);
            if (first < 0) first = k;
            if (prev >= 0 && cnt[prev][k] < 255) cnt[prev][k]++;
            prev = k;
        }
        if (prev >= 0 && cnt[prev][first] < 255) cnt[prev][first]++;
        int cur = first >= 0 ? first : pick(s, n);
        for (int i = 0; i < len; i++) {
            idx[i] = cur;
            if (p[1]) on[i] = chance(s, p[1] * 10);
            int sum = 0;
            for (int k = 0; k < n; k++) sum += cnt[cur][k];
            if (sum == 0 || chance(s, p[0] * 10)) {
                cur = pick(s, n);
            } else {
                int r = pick(s, sum), k = 0;
                while (r >= cnt[cur][k]) { r -= cnt[cur][k]; k++; }
                cur = k;
            }
        }
        break;
    }
    case GEN_CHAOS: {
        double r = 3.5 + p[0] * 0.05;
        double x = 0.12 + p[1] * 0.05 + (double)(seq_rand(s) % 1000) / 1e6;
        for (int w = 0; w < 8; w++) x = r * x * (1 - x);
        for (int i = 0; i < len; i++) {
            x = r * x * (1 - x);
            int k = (int)(x * n);
            idx[i] = k >= n ? n - 1 : k;
            x = r * x * (1 - x);
            on[i] = x < p[2] / 10.0;
        }
        break;
    }
    case GEN_CELLULAR: {
        uint8_t cur[SEQ_STEPS] = {0}, prev[SEQ_STEPS];
        uint8_t rule = CA_RULES[p[0] % 12];
        if (p[2]) for (int i = 0; i < SEQ_STEPS; i++) cur[i] = seq_rand(s) & 1;
        else cur[SEQ_STEPS / 2] = 1;
        memcpy(prev, cur, sizeof prev);
        for (int gen = 0; gen < p[1]; gen++) {
            memcpy(prev, cur, sizeof prev);
            for (int i = 0; i < SEQ_STEPS; i++) {
                int nb = (prev[(i + SEQ_STEPS - 1) % SEQ_STEPS] << 2) | (prev[i] << 1) | prev[(i + 1) % SEQ_STEPS];
                cur[i] = (rule >> nb) & 1;
            }
        }
        for (int i = 0; i < len; i++) {
            int nb = (prev[(i + SEQ_STEPS - 1) % SEQ_STEPS] << 2) | (prev[i] << 1) | prev[(i + 1) % SEQ_STEPS];
            on[i] = cur[i];
            idx[i] = nb * (n - 1) / 7;
        }
        break;
    }
    case GEN_WEIGHTED: {
        int w[64], total = 0;
        for (int k = 0; k < n; k++) {
            int pc = ((pool[k] - s->root) % 12 + 12) % 12;
            int base = pc == 0 ? 8 : pc == 7 ? 6 : (pc == 3 || pc == 4) ? 5 : pc == 5 ? 3 : (pc == 10 || pc == 11) ? 3 : 2;
            w[k] = base * (8 - p[0]) + p[0];      /* tension flattens the weights toward 1 */
            total += w[k];
        }
        int prev = -1;
        for (int i = 0; i < len; i++) {
            int k;
            if (prev >= 0 && chance(s, p[2] * 10)) {
                k = prev;
            } else {
                int r = pick(s, total);
                k = 0;
                while (r >= w[k]) { r -= w[k]; k++; }
            }
            idx[i] = prev = k;
            on[i] = chance(s, p[1] * 10);
        }
        break;
    }
    default: return;
    }

    for (int i = 0; i < len; i++) {
        SeqStep *st = &tr->step[i];
        if (st->lock) continue;
        st->on = (uint8_t)(on[i] != 0);
        if (idx[i] >= 0) st->note = pool[idx[i]];
    }
}

void gen_roll(Seq *s, int t) {
    GenSettings *g = &s->track[t].gen;
    if (g->type == GEN_CELLULAR) g->p[1] = (uint8_t)((g->p[1] + 1) % 64);
    gen_apply(s, t);
}

/* ---- evolution ---- */

void gen_evolve_bar(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    const EvolveSettings *e = &tr->evo;
    if (e->kinds & EVO_REGEN) gen_roll(s, t);

    uint8_t pool[64];
    int n = gen_pool(s, t, pool), len = tr->len;
    int kinds[3], nk = 0;
    if (e->kinds & EVO_PITCH) kinds[nk++] = EVO_PITCH;
    if (e->kinds & EVO_RHYTHM) kinds[nk++] = EVO_RHYTHM;
    if (e->kinds & EVO_VEL) kinds[nk++] = EVO_VEL;

    for (int i = 0; nk && i < len; i++) {
        SeqStep *st = &tr->step[i];
        if (st->lock || !chance(s, e->amount)) continue;
        switch (kinds[pick(s, nk)]) {
        case EVO_PITCH: {
            int k = nearest(pool, n, st->note) + (pick(s, 2) ? 1 : -1);
            k = k < 0 ? 0 : k >= n ? n - 1 : k;
            st->note = pool[k];
            break;
        }
        case EVO_RHYTHM:
            st->on ^= 1;
            break;
        case EVO_VEL: {
            int v = st->vel + (pick(s, 2) ? 1 : -1) * (8 + pick(s, 12));
            st->vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
            break;
        }
        }
    }

    if ((e->kinds & EVO_ROTATE) && chance(s, e->amount)) {
        int locked = 0;
        for (int i = 0; i < len; i++) locked |= tr->step[i].lock;
        if (!locked && len > 1) {
            SeqStep first = tr->step[0];
            for (int i = 0; i < len - 1; i++) tr->step[i] = tr->step[i + 1];
            tr->step[len - 1] = first;
        }
    }
}
