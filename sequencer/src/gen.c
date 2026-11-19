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

static uint16_t scale_mask(const Seq *s) { return SCALES[s->scale < 0 || s->scale >= NSCALES ? 0 : s->scale].mask; }

static int scale_size(const Seq *s) {
    int n = 0;
    for (uint16_t m = scale_mask(s); m; m >>= 1) n += m & 1;
    return n;
}

int gen_snap_note(const Seq *s, int note) {
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    for (int d = 0; d < 12; d++) {
        if (note - d >= 0 && seq_in_scale(s, note - d)) return note - d;
        if (note + d <= 127 && seq_in_scale(s, note + d)) return note + d;
    }
    return note;
}

int gen_note_degree(const Seq *s, int note) {
    int deg = -1;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    for (int k = 0; k <= note; k++) deg += seq_in_scale(s, k);
    return deg < 0 ? 0 : deg;
}

int gen_degree_note(const Seq *s, int degree) {
    int last = 0, deg = -1;
    for (int k = 0; k <= 127; k++) {
        if (!seq_in_scale(s, k)) continue;
        last = k;
        if (++deg >= degree) return k;
    }
    return last;
}

int gen_hits(const Seq *s, int t) {
    int n = 0;
    for (int i = 0; i < s->track[t].len; i++) n += s->track[t].step[i].on;
    return n;
}

/* Move `note` by whole octaves until it lies within [lo, hi] (if it can). */
static int fold_octave(int note, int lo, int hi) {
    while (note < lo && note + 12 <= 127) note += 12;
    while (note > hi && note - 12 >= 0) note -= 12;
    return note;
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
    [GEN_FOLLOW] = {"FOLLOW", 4, {{"SOURCE", 1, 4, 1}, {"RHYTHM", 0, 3, 0}, {"INTERVAL", 0, 14, 7}, {"PITCH", 0, 2, 0}}},
    [GEN_ARP] = {"ARP", 4, {{"CHORD", 0, 5, 0}, {"ORDER", 0, 4, 0}, {"OCTAVES", 1, 4, 2}, {"RATE", 1, 4, 1}}},
    [GEN_TURING] = {"TURING", 4, {{"CHANGE", 0, 10, 2}, {"DENSITY", 0, 10, 6}, {"RANGE", 1, 8, 4}, {"LENGTH", 2, 16, 8}}},
    [GEN_MOTIF] = {"MOTIF", 4, {{"LENGTH", 2, 8, 4}, {"DEVELOP", 0, 10, 5}, {"DENSITY", 1, 10, 7}, {"LEAP", 1, 4, 2}}},
};
static const char *FOLLOW_RHYTHM[4] = {"SAME", "GAPS", "ECHO", "HALF"};
static const char *FOLLOW_PITCH[3] = {"ROOT", "HARMONY", "KEEP"};
static const char *ARP_CHORD[6] = {"TRIAD", "SEVENTH", "SUS2", "SUS4", "ADD9", "POWER"};
static const char *ARP_ORDER[5] = {"UP", "DOWN", "UP-DOWN", "RANDOM", "INWARD"};
/* Chord shapes in scale degrees, and in semitones for the chromatic scale. */
static const int8_t ARP_DEG[6][5] = {{0, 2, 4, -1}, {0, 2, 4, 6, -1}, {0, 1, 4, -1}, {0, 3, 4, -1}, {0, 2, 4, 8, -1}, {0, 4, -1}};
static const int8_t ARP_SEMI[6][5] = {{0, 4, 7, -1}, {0, 4, 7, 10, -1}, {0, 2, 7, -1}, {0, 5, 7, -1}, {0, 4, 7, 14, -1}, {0, 7, -1}};
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
    case GEN_FOLLOW:
        if (k == 0) { snprintf(out, 16, "TRACK %d", v); return; }
        if (k == 1) { snprintf(out, 16, "%s", FOLLOW_RHYTHM[v & 3]); return; }
        if (k == 2) { snprintf(out, 16, "%+d", v - 7); return; }
        if (k == 3) { snprintf(out, 16, "%s", FOLLOW_PITCH[v % 3]); return; }
        break;
    case GEN_ARP:
        if (k == 0) { snprintf(out, 16, "%s", ARP_CHORD[v % 6]); return; }
        if (k == 1) { snprintf(out, 16, "%s", ARP_ORDER[v % 5]); return; }
        if (k == 3) { snprintf(out, 16, "1/%d", 16 / v); return; }
        break;
    case GEN_TURING:
        if (k == 0) { snprintf(out, 16, "%d%%", v * 5); return; }
        if (k == 1) { snprintf(out, 16, "%d%%", v * 10); return; }
        if (k == 2) { snprintf(out, 16, "%d/8", v); return; }
        break;
    case GEN_MOTIF:
        if (k == 1 || k == 2) { snprintf(out, 16, "%d%%", v * 10); return; }
        break;
    default: break;
    }
    snprintf(out, 16, "%d", v);
}

void gen_set_type(Seq *s, int t, int type) {
    GenSettings *g = &s->track[t].gen;
    g->type = (uint8_t)tidx(type);
    for (int k = 0; k < 4; k++) g->p[k] = k < GENS[g->type].n ? GENS[g->type].p[k].def : 0;
    /* FOLLOW defaults to the track above (track 1 follows track 2). */
    if (g->type == GEN_FOLLOW) g->p[0] = (uint8_t)(t == 0 ? 2 : t);
    if (g->type == GEN_TURING && (g->state & 0xFFFF) == 0) g->state = (seq_rand(s) & 0xFFFF) | 1;
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
    int on[SEQ_STEPS], idx[SEQ_STEPS], note[SEQ_STEPS], vel[SEQ_STEPS];
    for (int i = 0; i < SEQ_STEPS; i++) { on[i] = tr->step[i].on; idx[i] = note[i] = vel[i] = -1; }
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
    case GEN_FOLLOW: {
        int src = p[0] - 1;
        if (src < 0 || src >= SEQ_TRACKS) src = 0;
        const SeqTrack *so = &s->track[src];
        int lo = pool[0], hi = pool[n - 1], hits = 0;
        for (int i = 0; i < len; i++) {
            int j = i % so->len, jp = (i + so->len - 1) % so->len;
            const SeqStep *ss = &so->step[j];
            switch (p[1] & 3) {
            case 0: on[i] = ss->on; break;                         /* SAME */
            case 1: on[i] = !ss->on; break;                        /* GAPS: play where the source rests */
            case 2: on[i] = so->step[jp].on; ss = &so->step[jp]; break;  /* ECHO: one step behind */
            default: on[i] = ss->on && (hits++ % 2 == 0); break;   /* HALF: every other source hit */
            }
            int iv = (int)p[2] - 7;
            if (p[3] == 0) note[i] = gen_step_note(s, lo, iv < 0 ? 0 : iv);
            else if (p[3] == 1) note[i] = fold_octave(gen_step_note(s, gen_snap_note(s, ss->note), iv), lo, hi);
            if (ss->on && (p[1] & 3) != 1) vel[i] = ss->vel;     /* accents follow the source */
        }
        break;
    }
    case GEN_ARP: {
        int chromatic = scale_size(s) == 12, sz = scale_size(s);
        const int8_t *shape = chromatic ? ARP_SEMI[p[0] % 6] : ARP_DEG[p[0] % 6];
        int root = gen_note_degree(s, pool[0]), octs = p[2] > g->span ? g->span : p[2];
        int tones[24], nt = 0;
        for (int o = 0; o < octs; o++)
            for (int k = 0; k < 5 && shape[k] >= 0 && nt < 24; k++) {
                int nn = chromatic ? pool[0] + shape[k] + 12 * o : gen_degree_note(s, root + shape[k] + sz * o);
                if (nn <= 127 && (nt == 0 || nn > tones[nt - 1])) tones[nt++] = nn;
            }
        if (nt == 0) tones[nt++] = pool[0];
        int seq[48], ns = 0;
        switch (p[1] % 5) {
        case 0: for (int k = 0; k < nt; k++) seq[ns++] = tones[k]; break;
        case 1: for (int k = nt - 1; k >= 0; k--) seq[ns++] = tones[k]; break;
        case 2: for (int k = 0; k < nt; k++) seq[ns++] = tones[k];
                for (int k = nt - 2; k > 0; k--) seq[ns++] = tones[k];
                break;
        case 3: for (int k = 0; k < nt; k++) seq[ns++] = tones[k]; break;  /* picked at random below */
        default: for (int a = 0, b = nt - 1; a <= b; a++, b--) { seq[ns++] = tones[a]; if (a != b) seq[ns++] = tones[b]; }
        }
        int pos = 0;
        for (int i = 0; i < len; i++) {
            on[i] = i % p[3] == 0;
            if (!on[i]) continue;
            note[i] = (p[1] % 5) == 3 ? seq[pick(s, ns)] : seq[pos++ % ns];
        }
        break;
    }
    case GEN_TURING: {
        int L = p[3] < 2 ? 2 : p[3] > 16 ? 16 : p[3];
        uint32_t reg = g->state & 0xFFFF;
        int range = n * p[2] / 8;
        if (range < 1) range = 1;
        for (int i = 0; i < len; i++) {
            int w = 0;
            for (int b = 0; b < 8; b++) w |= (int)((reg >> ((i + b) % L)) & 1) << (7 - b);
            idx[i] = w * range / 256;
            on[i] = ((w ^ (w >> 3) ^ (w << 2)) & 0xFF) < p[1] * 26;
        }
        break;
    }
    case GEN_MOTIF: {
        int cl = p[0] < 2 ? 2 : p[0], off[8], mon[8];
        off[0] = 0; mon[0] = 1;
        for (int k = 1; k < cl; k++) {
            int step = 1 + pick(s, p[3]);
            if (chance(s, 30)) step = 0;                       /* repeated notes make motifs catchy */
            off[k] = off[k - 1] + (pick(s, 2) ? step : -step);
            mon[k] = chance(s, p[2] * 10);
        }
        int start = n / 3, trans = 0, inv = 0, retro = 0;
        for (int i = 0; i < len; i++) {
            int c = i / cl, k = i % cl;
            if (k == 0 && c > 0) {                             /* choose how this repeat develops */
                trans = inv = retro = 0;
                if (chance(s, p[1] * 10)) {
                    switch (pick(s, 4)) {
                    case 0: trans = (pick(s, 2) ? 1 : -1) * (1 + pick(s, 3)); break;
                    case 1: inv = 1; break;
                    case 2: retro = 1; break;
                    default: inv = retro = 1; break;
                    }
                }
            }
            int kk = retro ? cl - 1 - k : k;
            int pos = start + trans + (inv ? -off[kk] : off[kk]);
            while (pos < 0 || pos >= n) pos = pos < 0 ? -pos : 2 * (n - 1) - pos;
            if (n == 1) pos = 0;
            idx[i] = pos;
            on[i] = mon[kk];
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
        if (note[i] >= 0) st->note = (uint8_t)(note[i] > 127 ? 127 : note[i]);
        if (vel[i] > 0) st->vel = (uint8_t)vel[i];
    }
}

void gen_roll(Seq *s, int t) {
    GenSettings *g = &s->track[t].gen;
    if (g->type == GEN_CELLULAR) g->p[1] = (uint8_t)((g->p[1] + 1) % 64);
    if (g->type == GEN_TURING) {
        /* One trip round the loop: each bit that wraps may flip. */
        int L = g->p[3] < 2 ? 2 : g->p[3] > 16 ? 16 : g->p[3];
        uint32_t reg = g->state & 0xFFFF, mask = (1u << L) - 1, top = reg & ~mask;
        reg &= mask;
        if (reg == 0) reg = 1;
        for (int k = 0; k < L; k++) {
            uint32_t bit = reg & 1;
            if (chance(s, g->p[0] * 5)) bit ^= 1;
            reg = (reg >> 1) | (bit << (L - 1));
        }
        g->state = top | reg;
    }
    gen_apply(s, t);
}

/* ---- evolution ---- */

/* Positions within the track's length that are not locked. */
static int unlocked(const SeqTrack *tr, int out[SEQ_STEPS]) {
    int n = 0;
    for (int i = 0; i < tr->len; i++) if (!tr->step[i].lock) out[n++] = i;
    return n;
}

/* A sounding note near step i, so newly switched-on steps fit the phrase. */
static int neighbour_note(const SeqTrack *tr, int i) {
    for (int d = 1; d < tr->len; d++) {
        int a = (i - d + tr->len) % tr->len, b = (i + d) % tr->len;
        if (tr->step[a].on) return tr->step[a].note;
        if (tr->step[b].on) return tr->step[b].note;
    }
    return tr->step[i].note;
}

static void switch_on(Seq *s, SeqTrack *tr, int i) {
    (void)s;
    tr->step[i].note = (uint8_t)neighbour_note(tr, i);
    tr->step[i].on = 1;
}

/* Keep the number of hits within the evolution's min/max. */
static void keep_density(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    int pos[SEQ_STEPS], np = unlocked(tr, pos);
    int lo = tr->evo.min_hits, hi = tr->evo.max_hits ? tr->evo.max_hits : SEQ_STEPS;   /* 0 = no maximum */
    if (hi < lo) hi = lo;
    for (int guard = 0; guard < SEQ_STEPS && gen_hits(s, t) < lo; guard++) {
        int cand[SEQ_STEPS], nc = 0;
        for (int k = 0; k < np; k++) if (!tr->step[pos[k]].on) cand[nc++] = pos[k];
        if (!nc) break;
        switch_on(s, tr, cand[pick(s, nc)]);
    }
    for (int guard = 0; guard < SEQ_STEPS && gen_hits(s, t) > hi; guard++) {
        int cand[SEQ_STEPS], nc = 0;
        for (int k = 0; k < np; k++) if (tr->step[pos[k]].on) cand[nc++] = pos[k];
        if (!nc) break;
        tr->step[cand[pick(s, nc)]].on = 0;
    }
}

void gen_evolve_bar(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    const EvolveSettings *e = &tr->evo;

    /* Pull first, so this bar's new changes are heard before they can be undone. */
    if (tr->has_anchor && e->pull) {
        for (int i = 0; i < tr->len; i++) {
            SeqStep *st = &tr->step[i];
            const SeqStep *an = &tr->anchor[i];
            if (st->lock) continue;
            int differs = st->on != an->on || st->note != an->note || st->vel != an->vel || st->prob != an->prob ||
                          st->gate != an->gate;
            if (differs && chance(s, e->pull)) { *st = *an; st->lock = 0; }
        }
    }

    if (e->kinds & EVO_REGEN) gen_roll(s, t);

    uint8_t pool[64];
    int n = gen_pool(s, t, pool), len = tr->len;
    int kinds[4], nk = 0;
    if (e->kinds & EVO_PITCH) kinds[nk++] = EVO_PITCH;
    if (e->kinds & EVO_RHYTHM) kinds[nk++] = EVO_RHYTHM;
    if (e->kinds & EVO_VEL) kinds[nk++] = EVO_VEL;
    if (e->kinds & EVO_PROB) kinds[nk++] = EVO_PROB;

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
            if (st->on) st->on = 0;
            else switch_on(s, tr, i);
            break;
        case EVO_VEL: {
            int v = st->vel + (pick(s, 2) ? 1 : -1) * (8 + pick(s, 12));
            st->vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
            break;
        }
        case EVO_PROB: {
            int v = st->prob + (pick(s, 2) ? 1 : -1) * (5 + pick(s, 11));
            st->prob = (uint8_t)(v < 10 ? 10 : v > 100 ? 100 : v);
            break;
        }
        }
    }

    if ((e->kinds & EVO_ROTATE) && chance(s, e->amount)) gen_transform(s, t, XF_SHIFT, pick(s, 2) ? 1 : -1);

    keep_density(s, t);
}

void gen_set_anchor(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    memcpy(tr->anchor, tr->step, sizeof tr->anchor);
    for (int i = 0; i < SEQ_STEPS; i++) tr->anchor[i].lock = 0;
    tr->has_anchor = 1;
}

void gen_clear_anchor(Seq *s, int t) { s->track[t].has_anchor = 0; }

/* ---- transforms ---- */

const char *gen_xform_name(int op) {
    static const char *N[XF_COUNT] = {"REVERSE", "INVERT", "TRANSPOSE", "SHIFT", "THIN", "FILL", "HUMANIZE"};
    return op >= 0 && op < XF_COUNT ? N[op] : "?";
}

void gen_transform(Seq *s, int t, int op, int amount) {
    SeqTrack *tr = &s->track[t];
    int pos[SEQ_STEPS], np = unlocked(tr, pos);
    if (np == 0) return;
    SeqStep tmp[SEQ_STEPS];
    switch (op) {
    case XF_REVERSE:
        for (int k = 0; k < np; k++) tmp[k] = tr->step[pos[np - 1 - k]];
        for (int k = 0; k < np; k++) tr->step[pos[k]] = tmp[k];
        break;
    case XF_INVERT: {
        int pivot = -1;
        for (int i = 0; i < tr->len && pivot < 0; i++) if (tr->step[i].on) pivot = gen_note_degree(s, tr->step[i].note);
        if (pivot < 0) pivot = gen_note_degree(s, tr->gen.base);
        int maxdeg = gen_note_degree(s, 127);
        for (int k = 0; k < np; k++) {
            SeqStep *st = &tr->step[pos[k]];
            int d = 2 * pivot - gen_note_degree(s, st->note);
            st->note = (uint8_t)gen_degree_note(s, d < 0 ? 0 : d > maxdeg ? maxdeg : d);
        }
        break;
    }
    case XF_TRANSPOSE:
        for (int k = 0; k < np; k++) {
            SeqStep *st = &tr->step[pos[k]];
            st->note = (uint8_t)gen_step_note(s, gen_snap_note(s, st->note), amount);
        }
        break;
    case XF_SHIFT: {
        int sh = ((amount % np) + np) % np;
        for (int k = 0; k < np; k++) tmp[(k + sh) % np] = tr->step[pos[k]];
        for (int k = 0; k < np; k++) tr->step[pos[k]] = tmp[k];
        break;
    }
    case XF_THIN:
        for (int r = 0; r < amount; r++) {
            int best = -1, bscore = 1 << 30;
            for (int k = 0; k < np; k++) {
                const SeqStep *st = &tr->step[pos[k]];
                if (!st->on) continue;
                int i = pos[k];
                int score = st->vel * 4 + (i % 4 == 0 ? 1000 : i % 2 == 0 ? 500 : 0) + pick(s, 64);
                if (score < bscore) { bscore = score; best = i; }
            }
            if (best < 0) break;
            tr->step[best].on = 0;
        }
        break;
    case XF_FILL:
        for (int r = 0; r < amount; r++) {
            int cand[SEQ_STEPS], nc = 0;
            for (int k = 0; k < np; k++) if (!tr->step[pos[k]].on) cand[nc++] = pos[k];
            if (!nc) break;
            switch_on(s, tr, cand[pick(s, nc)]);
        }
        break;
    case XF_HUMANIZE:
        if (amount <= 0) break;
        for (int k = 0; k < np; k++) {
            SeqStep *st = &tr->step[pos[k]];
            int v = st->vel + pick(s, 2 * amount + 1) - amount;
            st->vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        }
        break;
    default: break;
    }
}
