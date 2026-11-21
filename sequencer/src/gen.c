#include "gen.h"
#include "drum.h"

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
    [GEN_EUCLID] = {"EUCLID", 3, {{"HITS", 0, 32, 5}, {"ROTATE", 0, 31, 0}, {"PITCH", 0, 2, 0}}},
    [GEN_WALK] = {"WALK", 3, {{"STEP", 1, 7, 2}, {"BIAS", 0, 10, 5}, {"DENSITY", 1, 10, 7}}},
    [GEN_MARKOV] = {"MARKOV", 2, {{"CHAOS", 0, 10, 1}, {"RHYTHM", 0, 10, 0}}},
    [GEN_CHAOS] = {"CHAOS", 3, {{"R", 0, 10, 8}, {"SEED", 0, 15, 3}, {"DENSITY", 1, 10, 7}}},
    [GEN_CELLULAR] = {"CELLULAR", 3, {{"RULE", 0, 11, 1}, {"GENS", 0, 63, 8}, {"SEED", 0, 1, 0}}},
    [GEN_WEIGHTED] = {"WEIGHTED", 3, {{"TENSION", 0, 7, 3}, {"DENSITY", 1, 10, 7}, {"REPEAT", 0, 10, 2}}},
    [GEN_FOLLOW] = {"FOLLOW", 4, {{"SOURCE", 1, 4, 1}, {"RHYTHM", 0, 3, 0}, {"INTERVAL", 0, 14, 7}, {"PITCH", 0, 2, 0}}},
    [GEN_ARP] = {"ARP", 4, {{"CHORD", 0, 5, 0}, {"ORDER", 0, 4, 0}, {"OCTAVES", 1, 4, 2}, {"RATE", 1, 4, 1}}},
    [GEN_TURING] = {"TURING", 4, {{"CHANGE", 0, 10, 2}, {"DENSITY", 0, 10, 6}, {"RANGE", 1, 8, 4}, {"LENGTH", 2, 32, 8}}},
    [GEN_MOTIF] = {"MOTIF", 4, {{"LENGTH", 2, 8, 4}, {"DEVELOP", 0, 10, 5}, {"DENSITY", 1, 10, 7}, {"LEAP", 1, 4, 2}}},
    /* The ambient generators carry ambient defaults themselves, whatever the style. */
    [GEN_DRONE] = {"DRONE", 3, {{"NOTES", 1, 4, 1}, {"MOVE", 0, 10, 3}, {"HOME", 0, 10, 5}}},
    [GEN_DRIFT] = {"DRIFT", 3, {{"NOTES", 1, 8, 3}, {"HOLD", 0, 10, 8}, {"STEP", 1, 3, 1}}},
    [GEN_BELL] = {"BELL", 4, {{"NOTES", 1, 8, 2}, {"CHANCE", 1, 10, 6}, {"HEIGHT", 0, 2, 1}, {"SOFTEN", 0, 10, 4}}},
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
        if (k == 3) { snprintf(out, 16, v == 1 ? "EVERY STEP" : "EVERY %d", v); return; }
        break;
    case GEN_TURING:
        if (k == 0) { snprintf(out, 16, "%d%%", v * 5); return; }
        if (k == 1) { snprintf(out, 16, "%d%%", v * 10); return; }
        if (k == 2) { snprintf(out, 16, "%d/8", v); return; }
        break;
    case GEN_MOTIF:
        if (k == 1 || k == 2) { snprintf(out, 16, "%d%%", v * 10); return; }
        break;
    case GEN_DRONE:
        if (k == 1 || k == 2) { snprintf(out, 16, "%d%%", v * 10); return; }
        break;
    case GEN_DRIFT:
        if (k == 1) { snprintf(out, 16, "%d%%", v * 10); return; }
        if (k == 2) { snprintf(out, 16, "%d DEG", v); return; }
        break;
    case GEN_BELL:
        if (k == 1 || k == 3) { snprintf(out, 16, "%d%%", v * 10); return; }
        if (k == 2) { snprintf(out, 16, "+%d OCT", v); return; }
        break;
    default: break;
    }
    snprintf(out, 16, "%d", v);
}

static void style_defaults(int st, int type, uint8_t p[4]);

void gen_set_type(Seq *s, int t, int type) {
    GenSettings *g = &s->track[t].gen;
    g->type = (uint8_t)tidx(type);
    for (int k = 0; k < 4; k++) g->p[k] = k < GENS[g->type].n ? GENS[g->type].p[k].def : 0;
    style_defaults(s->track[t].style, g->type, g->p);
    /* FOLLOW defaults to the track above (track 1 follows track 2). */
    if (g->type == GEN_FOLLOW) g->p[0] = (uint8_t)(t == 0 ? 2 : t);
    if (g->type == GEN_TURING && g->state == 0) g->state = seq_rand(s) | 1;
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

/* ---- styles ---- */

static const char *STYLE_NAMES[STYLE_COUNT] = {"AMBIENT", "TECHNO", "MELODIC", "BLANK", "DRONE",
                                               "HIP HOP", "DRUM N BASS", "HOUSE", "TRIP HOP"};
int style_count(void) { return STYLE_COUNT; }
const char *style_name(int st) { return STYLE_NAMES[st < 0 || st >= STYLE_COUNT ? 0 : st]; }

/* Per-style generator defaults; -1 keeps the table default. Ambient wants
 * sparse, slow, mostly stepwise material; techno dense and repetitive;
 * melodic uses the table defaults. */
static const int8_t STYLE_DEF[STYLE_COUNT][GEN_COUNT][4] = {
    [STYLE_AMBIENT] = {
        [GEN_NONE] = {-1, -1, -1, -1},
        [GEN_EUCLID] = {3, -1, -1, -1},
        [GEN_WALK] = {1, -1, 4, -1},
        [GEN_MARKOV] = {1, 4, -1, -1},
        [GEN_CHAOS] = {2, -1, 4, -1},
        [GEN_CELLULAR] = {-1, -1, -1, -1},
        [GEN_WEIGHTED] = {2, 3, 4, -1},
        [GEN_FOLLOW] = {-1, -1, 11, 1},          /* a fifth above the source, as harmony */
        [GEN_ARP] = {1, 2, 2, 4},                /* seventh, up-down, 2 octaves, every 4th step */
        [GEN_TURING] = {1, 2, 4, 16},
        [GEN_MOTIF] = {5, 3, 4, 2},
    },
    [STYLE_TECHNO] = {
        [GEN_NONE] = {-1, -1, -1, -1},
        [GEN_EUCLID] = {4, -1, 1, -1},
        [GEN_WALK] = {2, -1, 8, -1},
        [GEN_MARKOV] = {1, 8, -1, -1},
        [GEN_CHAOS] = {-1, -1, 8, -1},
        [GEN_CELLULAR] = {-1, -1, -1, -1},
        [GEN_WEIGHTED] = {2, 8, 5, -1},
        [GEN_FOLLOW] = {-1, 1, -1, 0},           /* fill the source's gaps on the root */
        [GEN_ARP] = {5, 0, 1, 1},
        [GEN_TURING] = {1, 7, 3, 8},
        [GEN_MOTIF] = {4, 4, 8, 1},
    },
    [STYLE_MELODIC] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1},
                       {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1},
                       {-1, -1, -1, -1}},
};

static void style_defaults(int st, int type, uint8_t p[4]) {
    if (st < 0 || st >= STYLE_COUNT || st == STYLE_BLANK || type < 0 || type > GEN_MOTIF) return;   /* later generators: table defaults */
    /* The newer styles borrow the generator defaults of the nearest older one. */
    if (st == STYLE_DRONE || st == STYLE_TRIPHOP) st = STYLE_AMBIENT;
    else if (st == STYLE_DNB || st == STYLE_HOUSE) st = STYLE_TECHNO;
    else if (st == STYLE_HIPHOP) st = STYLE_MELODIC;
    for (int k = 0; k < GENS[type].n; k++) {
        int v = STYLE_DEF[st][type][k];
        if (v >= 0) p[k] = (uint8_t)(v < GENS[type].p[k].min ? GENS[type].p[k].min : v > GENS[type].p[k].max ? GENS[type].p[k].max : v);
    }
}

/* Shapes: a style's finishing pass over a generated track, for what the
 * generators can't do alone (accents, gate variation, chord tones, call and
 * response). Written into every step's note/velocity/gate, so rolls and
 * evolution keep the feel; locked steps keep the core fixed. */
enum { SH_NONE, SH_TECHNO_HATS, SH_TECHNO_BASS, SH_TECHNO_STAB,
       SH_MEL_BASS, SH_MEL_ARP, SH_MEL_CALL, SH_MEL_ANSWER,
       SH_CHORD_ROOTS,   /* every note on the root of the progression's chord (CHORD stacks the rest) */
       SH_DRUM_NOTES };  /* a drum part on a note track: the style's kit as DRUM MAP notes */

typedef struct {
    uint8_t gen, len, base, span, gate, vel, prob, hold;   /* prob: chance each step plays; hold: tied steps */
    uint8_t every, amount, kinds, pull, min_hits, max_hits;
    uint8_t shape;      /* SH_*; 0 = none */
    uint8_t setp;       /* bit k set: generator parameter k becomes p[k] (else the style default) */
    uint8_t p[4];
} TrackPreset;

static const struct { int bpm, scale, root, swing; TrackPreset t[SEQ_TRACKS]; } STYLE_SET[STYLE_COUNT] = {
    /* Ambient: slow, polymetric lengths (16, 13, 11, 23) so the tracks rarely line up,
     * long gates, soft velocities, and evolution that wanders but is pulled home. */
    [STYLE_AMBIENT] = {72, 8, 2, 0, {
        {GEN_TURING, 16, 38, 1, 100, 70, 90, 7, 8, 15, EVO_REGEN, 0, 1, 4,
         SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_ARP, 13, 50, 2, 100, 60, 80, 3, 4, 10, EVO_PITCH | EVO_PROB, 30, 0, 0,
         SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_MOTIF, 11, 62, 2, 100, 70, 70, 1, 4, 15, EVO_PITCH | EVO_RHYTHM | EVO_PROB, 25, 2, 6,
         SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_WEIGHTED, 23, 74, 2, 60, 50, 50, 0, 2, 20, EVO_PITCH | EVO_RHYTHM | EVO_VEL, 20, 1, 5,
         SH_NONE, 0, {0, 0, 0, 0}},
    }},
    /* Techno: 128 BPM, A phrygian. T1 four-on-the-floor kick (no evolution). T2
     * offbeat hats, locked, with ghost 16ths whose chance and level drift. T3 a
     * rolling bass in the kick's gaps (FOLLOW, GAPS): root and octave cells with
     * a turnaround, accents on the offbeat and varied gates. T4 a short looping
     * stab riff (TURING) that changes a bit every few bars. */
    [STYLE_TECHNO] = {128, 4, 9, 2, {
        {GEN_EUCLID, 16, 33, 1, 30, 120, 100, 0, 0, 0, 0, 0, 0, 0, SH_NONE, 7, {4, 0, 1, 0}},
        {GEN_EUCLID, 16, 54, 1, 15, 95, 100, 0, 2, 12, EVO_RHYTHM | EVO_PROB | EVO_VEL, 20, 4, 10,
         SH_TECHNO_HATS, 7, {4, 2, 1, 0}},
        {GEN_FOLLOW, 16, 33, 1, 25, 90, 100, 0, 4, 10, EVO_PITCH | EVO_VEL | EVO_PROB, 35, 0, 0,
         SH_TECHNO_BASS, 15, {1, 1, 7, 0}},
        {GEN_TURING, 16, 57, 1, 20, 90, 85, 0, 4, 10, EVO_REGEN | EVO_PROB, 0, 3, 7,
         SH_TECHNO_STAB, 15, {1, 3, 3, 16}},
    }},
    /* Melodic: 100 BPM, D dorian, all tracks 32 steps over a chord progression
     * (i-IV, i-VII, i-v, ...) with a chord every 8 or 16 steps. T1 bass on the
     * chord roots (locked on each change) with fifths and pickups. T2 a chord-tone
     * arpeggio. T3 a motif "call" in steps 1-8 and 17-24; T4 answers it in 9-16
     * and 25-32, moved or turned over and resolved onto the chord. The silent
     * halves of T3 and T4 are locked off so rolls and evolution keep the call and
     * response. Arp and bass use EUCLID with PITCH KEEP, so a roll changes the
     * rhythm but keeps the chord notes. Gentle evolution, pulled home. */
    [STYLE_MELODIC] = {100, 3, 2, 3, {
        {GEN_EUCLID, 32, 38, 1, 80, 95, 100, 0, 4, 6, EVO_RHYTHM | EVO_VEL, 30, 6, 14, SH_MEL_BASS, 7, {8, 0, 0, 0}},
        {GEN_EUCLID, 32, 50, 2, 45, 80, 100, 0, 4, 8, EVO_RHYTHM | EVO_VEL, 30, 10, 20, SH_MEL_ARP, 7, {16, 0, 0, 0}},
        {GEN_MOTIF, 32, 62, 1, 70, 90, 100, 0, 4, 10, EVO_PITCH | EVO_RHYTHM | EVO_VEL, 30, 4, 10,
         SH_MEL_CALL, 15, {4, 4, 7, 2}},
        {GEN_MOTIF, 32, 57, 2, 70, 80, 100, 0, 8, 10, EVO_PITCH | EVO_RHYTHM, 35, 3, 10, SH_MEL_ANSWER, 15, {4, 4, 7, 2}},
    }},
    /* Drone: A dorian at 66 BPM. A held bass (with its fifth) and a triad pad
     * that each move to another stable tone now and then (30% a roll, every
     * 32 and 16 bars), a slow drifting line, and sparse soft bells that rest
     * now and then. Lengths 32, 28, 20, 27 keep them out of step. */
    [STYLE_DRONE] = {66, 3, 9, 0, {
        {GEN_DRONE, 32, 38, 1, 100, 64, 100, 0, 32, 5, EVO_REGEN, 0, 0, 0, SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_DRONE, 28, 50, 2, 100, 56, 100, 0, 16, 5, EVO_REGEN, 0, 0, 0, SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_DRIFT, 20, 62, 2, 100, 60, 90, 0, 16, 10, EVO_REGEN | EVO_PROB, 0, 1, 4, SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_BELL, 27, 74, 2, 40, 55, 100, 2, 4, 10, EVO_PITCH | EVO_VEL, 25, 1, 3, SH_NONE, 0, {0, 0, 0, 0}},
    }},
    /* The drum parts (track 4) of the four newer styles: on a drum track the
     * style's kit; on a note track the kit's main hits as DRUM MAP notes
     * (SH_DRUM_NOTES), so an FM1 track on the DRUM engine plays them too. */
#define DRUM_PART(every, amount) \
        {GEN_EUCLID, 16, 36, 1, 20, 100, 100, 0, every, amount, EVO_RHYTHM | EVO_VEL | EVO_PROB, 40, 3, 14, SH_DRUM_NOTES, 7, {4, 0, 0, 0}}
    /* Hip hop: 88 BPM, G minor, swung. T1 bass on the chord roots (as MELODIC),
     * T2 keys: a seventh chord held on each chord change, T3 a sparse motif
     * lead, T4 boom bap drums. */
    [STYLE_HIPHOP] = {88, 2, 7, 6, {
        {GEN_EUCLID, 32, 31, 1, 80, 100, 100, 0, 4, 8, EVO_RHYTHM | EVO_VEL, 30, 6, 14, SH_MEL_BASS, 7, {6, 0, 0, 0}},
        {GEN_EUCLID, 32, 50, 1, 100, 70, 100, 5, 8, 10, EVO_VEL | EVO_PROB, 40, 3, 6, SH_CHORD_ROOTS, 7, {4, 0, 0, 0}},
        {GEN_MOTIF, 32, 62, 1, 60, 80, 80, 0, 4, 10, EVO_PITCH | EVO_RHYTHM, 30, 3, 8, SH_NONE, 15, {4, 4, 4, 2}},
        DRUM_PART(4, 10),
    }},
    /* Drum n bass: 172 BPM, A minor. T1 a sub bass that holds two notes a
     * pattern, T2 a slow triad pad that swells, T3 a short looping stab,
     * T4 two-step drums. */
    [STYLE_DNB] = {172, 2, 9, 0, {
        {GEN_DRONE, 32, 33, 1, 100, 100, 100, 0, 8, 10, EVO_REGEN, 0, 0, 0, SH_NONE, 3, {2, 4, 0, 0}},
        {GEN_DRONE, 32, 57, 1, 100, 55, 100, 0, 16, 5, EVO_REGEN, 0, 0, 0, SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_TURING, 16, 57, 1, 25, 85, 80, 0, 4, 10, EVO_REGEN | EVO_PROB, 0, 2, 5, SH_TECHNO_STAB, 15, {1, 3, 3, 16}},
        DRUM_PART(4, 10),
    }},
    /* House: 124 BPM, F dorian. T1 an offbeat bass, T2 syncopated seventh
     * chord stabs on the progression, T3 a light arpeggio, T4 four on the floor. */
    [STYLE_HOUSE] = {124, 3, 5, 2, {
        {GEN_EUCLID, 16, 29, 1, 35, 100, 100, 0, 4, 10, EVO_PITCH | EVO_VEL, 40, 0, 0, SH_TECHNO_STAB, 7, {4, 2, 1, 0}},
        {GEN_EUCLID, 32, 53, 1, 30, 85, 100, 0, 4, 8, EVO_RHYTHM | EVO_VEL, 40, 6, 12, SH_CHORD_ROOTS, 7, {8, 3, 0, 0}},
        {GEN_ARP, 16, 65, 2, 40, 70, 75, 0, 4, 10, EVO_PITCH | EVO_PROB, 30, 0, 0, SH_NONE, 15, {1, 2, 2, 2}},
        DRUM_PART(4, 8),
    }},
    /* Trip hop: 80 BPM, D minor, a little swing. T1 a slow drifting bass, T2
     * a dark seventh pad that swells, T3 sparse bells that rest now and then,
     * T4 half-time drums. */
    [STYLE_TRIPHOP] = {80, 2, 2, 3, {
        {GEN_DRIFT, 32, 26, 1, 100, 95, 100, 0, 8, 10, EVO_REGEN | EVO_PROB, 0, 1, 4, SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_DRONE, 32, 50, 2, 100, 55, 100, 0, 16, 5, EVO_REGEN, 0, 0, 0, SH_NONE, 0, {0, 0, 0, 0}},
        {GEN_BELL, 20, 69, 2, 40, 55, 100, 2, 4, 10, EVO_PITCH | EVO_VEL, 25, 1, 3, SH_NONE, 0, {0, 0, 0, 0}},
        DRUM_PART(8, 8),
    }},
#undef DRUM_PART
};

/* Part names, for M > NEW FROM STYLE's track rows. */
static const char *PART_NAMES[STYLE_COUNT][STYLE_PARTS] = {
    [STYLE_AMBIENT] = {"LOW", "ARP", "MOTIF", "HIGH"},
    [STYLE_TECHNO] = {"KICK", "HATS", "BASS", "STAB"},
    [STYLE_MELODIC] = {"BASS", "ARP", "CALL", "ANSWER"},
    [STYLE_BLANK] = {"", "", "", ""},
    [STYLE_DRONE] = {"BASS", "PAD", "DRIFT", "BELLS"},
    [STYLE_HIPHOP] = {"BASS", "KEYS", "LEAD", "DRUMS"},
    [STYLE_DNB] = {"SUB", "PAD", "STAB", "DRUMS"},
    [STYLE_HOUSE] = {"BASS", "CHORDS", "LEAD", "DRUMS"},
    [STYLE_TRIPHOP] = {"BASS", "PAD", "BELLS", "DRUMS"},
};

static int style_idx(int st) { return st < 0 || st >= STYLE_COUNT ? 0 : st; }
static int part_idx(int k) { return k < 0 || k >= STYLE_PARTS ? 0 : k; }
const char *style_part_name(int st, int k) { return PART_NAMES[style_idx(st)][part_idx(k)]; }
int style_part_is_drums(int st, int k) { return STYLE_SET[style_idx(st)].t[part_idx(k)].shape == SH_DRUM_NOTES; }

/* ---- style shapes ---- */

static int pick(Seq *s, int n);
static int chance(Seq *s, int pct);

#define OCT 99   /* in the cell tables: an octave above the tonic, whatever the scale size */

/* Scale degree of the first tonic at or above `base`. */
static int tonic_degree(const Seq *s, int base) {
    for (int n = base; n <= 127; n++)
        if (((n - s->root) % 12 + 12) % 12 == 0 && seq_in_scale(s, n)) return gen_note_degree(s, n);
    return gen_note_degree(s, gen_snap_note(s, base));
}

/* Tone k of the chord on scale degree `chord`, stacked in thirds from the
 * tonic `td`: 0 root, 1 third, 2 fifth, 3 seventh, 4 root an octave up, ... */
static int chord_tone(const Seq *s, int td, int chord, int k) {
    int deg = td + chord + (k % 4) * 2 + (k / 4) * scale_size(s);
    return gen_degree_note(s, deg < 0 ? 0 : deg);
}

/* The chord tone nearest `note` (ties go down). */
static int nearest_chord_tone(const Seq *s, int note, int td, int chord) {
    int best = note, bd = 1000, sz = scale_size(s);
    for (int o = -3; o <= 3; o++)
        for (int k = 0; k < 4; k++) {
            int deg = td + chord + 2 * k + o * sz;
            if (deg < 0) continue;
            int nn = gen_degree_note(s, deg), d = nn > note ? nn - note : note - nn;
            if (d < bd || (d == bd && nn < best)) { bd = d; best = nn; }
        }
    return best;
}

/* Chord progressions in scale degrees, one chord per 8 steps of a 32-step bar pair. */
static const int8_t PROGS[][4] = {{0, 0, 3, 3}, {0, 0, 6, 6}, {0, 0, 4, 4}, {0, 6, 2, 3}, {0, 0, 6, 3}};
#define NPROGS ((int)(sizeof PROGS / sizeof PROGS[0]))

static void style_shape(Seq *s, int t, int shape, const int8_t *prog) {
    SeqTrack *tr = &s->track[t];
    const TrackPreset *pr = &STYLE_SET[style_idx(tr->style)].t[part_idx(tr->part)];
    int len = tr->len, lo = pr->base, hi = pr->base + 12 * pr->span, sz = scale_size(s);
    int td = tonic_degree(s, lo);
    switch (shape) {
    case SH_TECHNO_HATS: {
        /* Offbeat hats (the generator's hits) are locked; others become quiet,
         * unlikely ghost 16ths, a few of them on to start. */
        for (int i = 0; i < len; i++) {
            SeqStep *st = &tr->step[i];
            if (st->on) { st->lock = 1; st->vel = 100; st->gate = 20; st->prob = 100; continue; }
            st->vel = (uint8_t)(38 + pick(s, 22));
            st->gate = 12;
            st->prob = (uint8_t)(30 + pick(s, 35));
            if (i % 2 == 1 && chance(s, 30)) st->on = 1;
        }
        break;
    }
    case SH_TECHNO_BASS: {
        /* Cells for the three 16ths after each kick: degrees above the tonic. */
        static const int8_t CELL[][3] = {{0, 0, 0}, {0, OCT, 0}, {0, 0, OCT}, {OCT, 0, 0}, {0, OCT, OCT}};
        static const int8_t TURN[][3] = {{0, 0, 1}, {0, 2, 0}, {0, 4, OCT}, {0, 6, OCT}, {2, 0, 4}};
        int c0 = pick(s, 5), c1 = pick(s, 5), tn = pick(s, 5);
        for (int i = 0; i < len; i++) {
            SeqStep *st = &tr->step[i];
            int beat = (i / 4) % 4, pos = i % 4;
            if (pos == 0) { st->note = (uint8_t)gen_degree_note(s, td); continue; }   /* under the kick */
            const int8_t *cell = beat == 3 ? TURN[tn] : beat == 1 ? CELL[c1] : CELL[c0];
            int off = cell[pos - 1] == OCT ? sz : cell[pos - 1];
            st->note = (uint8_t)gen_degree_note(s, td + off);
            st->vel = (uint8_t)(pos == 2 ? 118 : pos == 1 ? 86 : 78);
            st->gate = (uint8_t)(pos == 2 ? 40 : 22);
            if (chance(s, 15)) st->gate = 65;                  /* the odd longer note */
            st->prob = (uint8_t)(pos == 2 ? 100 : 94);
        }
        break;
    }
    case SH_TECHNO_STAB:
        for (int i = 0; i < len; i++) {
            SeqStep *st = &tr->step[i];
            st->vel = (uint8_t)(i % 4 == 2 ? 110 : 84);
            st->gate = (uint8_t)(chance(s, 30) ? 35 : 18);
        }
        break;
    case SH_CHORD_ROOTS:
        for (int i = 0; i < len; i++) {
            SeqStep *st = &tr->step[i];
            if (st->lock) continue;
            st->note = (uint8_t)fold_octave(chord_tone(s, td, prog[(i / 8) % 4], 0), lo, hi);
            if (i % 8 == 0 && st->vel < 117) st->vel = (uint8_t)(st->vel + 10);   /* lean on each chord change */
        }
        break;
    case SH_DRUM_NOTES:
        gen_drum_notes(s, t);
        break;
    case SH_MEL_BASS:
        for (int i = 0; i < len; i++) {
            SeqStep *st = &tr->step[i];
            int ch = prog[(i / 8) % 4], next = prog[(i / 8 + 1) % 4], pos = i % 8;
            int root = fold_octave(chord_tone(s, td, ch, 0), lo, hi);
            st->note = (uint8_t)root;
            if (pos == 0 && (i % 16 == 0 || ch != prog[(i / 8 + 3) % 4])) {
                st->on = 1; st->lock = 1; st->vel = 105; st->gate = 90; st->hold = 2;   /* chord change */
            } else if (pos == 4) {
                st->note = (uint8_t)fold_octave(chord_tone(s, td, ch, pick(s, 2) ? 2 : 0), lo, hi);
                st->vel = 82;
            } else if (pos == 6) {
                int nr = gen_note_degree(s, fold_octave(chord_tone(s, td, next, 0), lo, hi));
                st->note = (uint8_t)gen_degree_note(s, nr + (pick(s, 2) ? 1 : -1));   /* step into the next root */
                st->vel = 72; st->gate = 50;
                st->on = (uint8_t)chance(s, 40);
            } else {
                st->vel = 78;
            }
        }
        break;
    case SH_MEL_ARP: {
        /* Four 8ths per chord; the second chord of each pair climbs an inversion. */
        static const int8_t ORDER[][4] = {{0, 1, 2, 4}, {0, 2, 4, 2}, {0, 2, 1, 3}, {4, 2, 1, 0}};
        int o = pick(s, 4);
        for (int i = 0; i < len; i++) {
            SeqStep *st = &tr->step[i];
            int ch = prog[(i / 8) % 4], k = (i % 8) / 2, up = (i / 8) % 2;
            int tone = (i % 2 ? ORDER[o][(k + 1) % 4] : ORDER[o][k]) + up;   /* 16ths lean to the next tone */
            st->note = (uint8_t)fold_octave(chord_tone(s, td, ch, tone), lo, hi);
            st->vel = (uint8_t)(i % 4 == 0 ? 92 : i % 2 == 0 ? 76 : 60);
            if (i % 2 == 1 && !st->on && chance(s, 20)) { st->on = 1; st->prob = 70; }   /* a few 16th fills */
        }
        break;
    }
    case SH_MEL_CALL: {
        int last = -1;
        for (int i = 0; i < len; i++) {
            SeqStep *st = &tr->step[i];
            int ch = prog[(i / 8) % 4];
            if ((i / 8) % 2 == 1) { st->on = 0; st->lock = 1; continue; }   /* the answer's half: rest */
            if (i % 8 == 0) { if (last >= 0) tr->step[last].hold = 1; st->on = 1; last = -1; }
            if (i % 4 == 0 || i % 8 == 7) st->note = (uint8_t)nearest_chord_tone(s, st->note, td, ch);
            st->vel = (uint8_t)(i % 4 == 0 ? 100 : 84);
            if (st->on) last = i;
        }
        if (last >= 0) tr->step[last].hold = 1;
        break;
    }
    case SH_MEL_ANSWER: {
        const SeqTrack *call = &s->track[t > 0 ? t - 1 : 0];
        for (int half = 0; half < len / 8; half++) {
            int var = pick(s, 3), first = -1, last = -1;
            for (int k = 0; k < 8; k++) {
                int i = half * 8 + k;
                SeqStep *st = &tr->step[i];
                if (half % 2 == 0) { st->on = 0; st->lock = 1; continue; }   /* the call's half: rest */
                const SeqStep *cs = &call->step[(i - 8) % call->len];
                int ch = prog[(i / 8) % 4], d = gen_note_degree(s, gen_snap_note(s, cs->note));
                if (first < 0 && cs->on) first = d;
                if (var == 0) d -= 2;                                   /* a third lower */
                else if (var == 1 && first >= 0) d = 2 * first - d;     /* turned over */
                int nn = fold_octave(gen_degree_note(s, d < 0 ? 0 : d), lo, hi);
                if (k % 4 == 0) nn = nearest_chord_tone(s, nn, td, ch);
                st->on = cs->on;
                st->note = (uint8_t)nn;
                st->vel = (uint8_t)(cs->vel > 20 ? cs->vel - 10 : cs->vel);
                st->gate = cs->gate;
                st->hold = 0;
                if (st->on) last = i;
            }
            if (last >= 0 && half == len / 8 - 1) {                     /* the last answer comes home */
                SeqStep *st = &tr->step[last];
                st->note = (uint8_t)fold_octave(gen_degree_note(s, td), lo, hi);   /* the tonic, leading round */
                st->hold = 1;
            }
        }
        break;
    }
    default: break;
    }
}

/* Blank: an empty page. Bases match seq_init (C2, C3, C4, C5 an octave down). */
static void blank_track(Seq *s, int t) {
    static const uint8_t base[SEQ_TRACKS] = {24, 36, 48, 60};
    SeqTrack *tr = &s->track[t];
    tr->style = STYLE_BLANK;
    tr->part = (uint8_t)t;
    tr->len = SEQ_DEFAULT_LEN;
    tr->gen.base = base[t];
    tr->gen.span = 2;
    gen_set_type(s, t, GEN_NONE);
    int note = gen_snap_note(s, base[t] + 12);
    for (int i = 0; i < SEQ_STEPS; i++) tr->step[i] = (SeqStep){0, (uint8_t)note, 100, 50, 100, 0, 0, 0};
    tr->evo = (EvolveSettings){0, 20, EVO_PITCH | EVO_RHYTHM, 0, 0, 0};
    tr->has_anchor = 0;
    memset(tr->drum.vel, 0, sizeof tr->drum.vel);   /* a drum track stays one, with empty lanes */
    tr->drum_lock = 0;
    tr->chain_len = 0;
    tr->chain_pos = 0;
    tr->mute = 0;
    tr->solo = 0;
}

/* Playback colour per style and part (chord, swell bars, swell depth, rest
 * chance); styles left out here play plain. */
typedef struct { uint8_t chord, swell, swell_depth, rest; } TrackColour;
static const TrackColour STYLE_COLOUR[STYLE_COUNT][SEQ_TRACKS] = {
    [STYLE_DRONE] = {{CHORD_FIFTH, 16, 25, 0}, {CHORD_TRIAD, 8, 35, 0}, {CHORD_OFF, 4, 30, 10}, {CHORD_OFF, 0, 0, 15}},
    [STYLE_HIPHOP] = {{0, 0, 0, 0}, {CHORD_SEVENTH, 0, 0, 0}, {CHORD_OFF, 0, 0, 10}, {0, 0, 0, 0}},
    [STYLE_DNB] = {{0, 0, 0, 0}, {CHORD_TRIAD, 8, 40, 0}, {CHORD_FIFTH, 0, 0, 10}, {0, 0, 0, 0}},
    [STYLE_HOUSE] = {{0, 0, 0, 0}, {CHORD_SEVENTH, 0, 0, 0}, {CHORD_OFF, 0, 0, 10}, {0, 0, 0, 0}},
    [STYLE_TRIPHOP] = {{0, 0, 0, 0}, {CHORD_SEVENTH, 8, 35, 0}, {CHORD_OFF, 4, 30, 15}, {0, 0, 0, 0}},
};

int style_apply_tracks(Seq *s, int lead, const int style[SEQ_TRACKS], const int part[SEQ_TRACKS]) {
    lead = style_idx(lead);
    int st[SEQ_TRACKS], pt[SEQ_TRACKS], kept = 0, changed = 0, chords = 0;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        st[t] = style[t] == STYLE_KEEP ? STYLE_KEEP : style_idx(style[t]);
        pt[t] = part_idx(part[t]);
        kept += st[t] == STYLE_KEEP;
        chords |= st[t] == STYLE_MELODIC || st[t] == STYLE_HIPHOP || st[t] == STYLE_HOUSE;
    }
    s->style = lead;
    if (!kept && lead != STYLE_BLANK) {   /* a fresh start: the MUSIC STYLE's tempo and key */
        s->bpm = STYLE_SET[lead].bpm;
        s->swing = STYLE_SET[lead].swing;
        seq_set_scale(s, STYLE_SET[lead].scale, STYLE_SET[lead].root, 0);
    }
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        if (st[t] == STYLE_KEEP) continue;
        changed++;
        if (st[t] == STYLE_BLANK) { blank_track(s, t); continue; }
        const TrackPreset *pr = &STYLE_SET[st[t]].t[pt[t]];
        tr->style = (uint8_t)st[t];
        tr->part = (uint8_t)pt[t];
        tr->len = pr->len;
        tr->gen.base = pr->base;
        tr->gen.span = pr->span;
        for (int i = 0; i < SEQ_STEPS; i++)
            tr->step[i] = (SeqStep){0, (uint8_t)gen_snap_note(s, pr->base), pr->vel, pr->gate, pr->prob, 0, pr->hold, 0};
        gen_set_type(s, t, pr->gen);
        for (int k = 0; k < gen_param_count(pr->gen); k++)
            if (pr->setp & (1 << k))
                tr->gen.p[k] = (uint8_t)(pr->p[k] < gen_param_min(pr->gen, k) ? gen_param_min(pr->gen, k) :
                                         pr->p[k] > gen_param_max(pr->gen, k) ? gen_param_max(pr->gen, k) : pr->p[k]);
        tr->evo = (EvolveSettings){pr->every, pr->amount, pr->kinds, pr->pull, pr->min_hits, pr->max_hits};
        tr->chain_len = 0;
        tr->has_anchor = 0;
        const TrackColour *co = &STYLE_COLOUR[st[t]][pt[t]];
        tr->chord = co->chord;
        tr->swell = co->swell;
        tr->swell_depth = co->swell_depth;
        tr->rest = co->rest;
        tr->rest_left = 0;
    }
    /* Generate in order, so FOLLOW (techno bass) reads a finished kick and the
     * melodic answer reads a finished call. */
    const int8_t *prog = chords ? PROGS[pick(s, NPROGS)] : PROGS[0];   /* ambient's rolls stay as they were */
    for (int t = 0; t < SEQ_TRACKS; t++) {
        if (st[t] == STYLE_KEEP || st[t] == STYLE_BLANK) continue;
        gen_apply(s, t);
        style_shape(s, t, STYLE_SET[st[t]].t[pt[t]].shape, prog);
        if (s->track[t].type == TRACK_DRUMS) {   /* a drum track gets a kit pattern in the style */
            s->track[t].drum_busy = 50;
            s->track[t].drum_lock = 0;
            gen_drum_roll(s, t);
        }
        if (s->track[t].evo.pull) gen_set_anchor(s, t);
    }
    return changed;
}

int style_apply(Seq *s, int st, int keep_manual) {
    st = style_idx(st);
    int style[SEQ_TRACKS], part[SEQ_TRACKS], kept = 0;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        int keep = st != STYLE_BLANK && keep_manual && s->track[t].gen.type == GEN_NONE && gen_hits(s, t) > 0;
        style[t] = keep ? STYLE_KEEP : st;
        part[t] = t;
        kept += keep;
    }
    style_apply_tracks(s, st, style, part);
    return kept;
}

const char *gen_short_name(int type) {
    static const char *N[GEN_COUNT] = {"MANUAL", "EUCLID", "WALK", "MARKOV", "CHAOS", "CA", "WEIGHT",
                                       "FOLLOW", "ARP", "TURING", "MOTIF", "DRONE", "DRIFT", "BELL"};
    return N[tidx(type)];
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
    int on[SEQ_STEPS], idx[SEQ_STEPS], note[SEQ_STEPS], vel[SEQ_STEPS], hold[SEQ_STEPS], gate[SEQ_STEPS], prob[SEQ_STEPS];
    for (int i = 0; i < SEQ_STEPS; i++) {
        on[i] = tr->step[i].on;
        idx[i] = note[i] = vel[i] = hold[i] = gate[i] = prob[i] = -1;
    }
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
        /* The automaton's world is the track's length, wrapping round. */
        uint8_t cur[SEQ_STEPS] = {0}, prev[SEQ_STEPS];
        uint8_t rule = CA_RULES[p[0] % 12];
        if (p[2]) for (int i = 0; i < len; i++) cur[i] = seq_rand(s) & 1;
        else cur[len / 2] = 1;
        memcpy(prev, cur, sizeof prev);
        for (int gen = 0; gen < p[1]; gen++) {
            memcpy(prev, cur, sizeof prev);
            for (int i = 0; i < len; i++) {
                int nb = (prev[(i + len - 1) % len] << 2) | (prev[i] << 1) | prev[(i + 1) % len];
                cur[i] = (rule >> nb) & 1;
            }
        }
        for (int i = 0; i < len; i++) {
            int nb = (prev[(i + len - 1) % len] << 2) | (prev[i] << 1) | prev[(i + 1) % len];
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
        int L = p[3] < 2 ? 2 : p[3] > 32 ? 32 : p[3];
        uint32_t reg = g->state;
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
    case GEN_DRONE: {
        /* The drone tone is the track's current first note, so it stays put
         * between rolls; each roll it may move (MOVE) to another stable tone,
         * or go home to the root (HOME). A fresh, empty track starts on the root. */
        int cand[64], nc = 0, home = -1;
        for (int k = 0; k < n; k++) {
            int pc = ((pool[k] - s->root) % 12 + 12) % 12;
            if (pc == 0 || pc == 7 || pc == 5 || pc == 9 || pc == 3 || pc == 8) cand[nc++] = k;
            if (pc == 0 && home < 0) home = k;
        }
        if (nc == 0) cand[nc++] = 0;
        if (home < 0) home = cand[0];
        int first = -1;
        for (int i = 0; i < len && first < 0; i++) if (tr->step[i].on) first = i;
        int cur = first < 0 ? home : nearest(pool, n, tr->step[first].note);
        if (first >= 0 && chance(s, p[1] * 10)) {
            if (cur != home && chance(s, p[2] * 10)) cur = home;
            else {
                int near[64], nn = 0;   /* stable tones within a fifth of the current one */
                for (int c = 0; c < nc; c++) {
                    int d = pool[cand[c]] - pool[cur];
                    if (cand[c] != cur && d >= -7 && d <= 7) near[nn++] = cand[c];
                }
                if (nn) cur = near[pick(s, nn)];
            }
        }
        int notes = p[0] < 1 ? 1 : p[0] > len ? len : p[0];
        int fifth = nearest(pool, n, pool[cur] + 7);
        if (pool[fifth] <= pool[cur]) fifth = cur;
        for (int i = 0; i < len; i++) on[i] = 0;
        for (int j = 0; j < notes; j++) {
            int a = j * len / notes, b = (j + 1) * len / notes;
            on[a] = 1;
            idx[a] = (j & 1) ? fifth : cur;   /* more than one note: alternate with the fifth above */
            hold[a] = b - a - 1;
            gate[a] = 100;
        }
        break;
    }
    case GEN_DRIFT: {
        /* A few long notes, evenly spread with a little jitter; each moves at
         * most STEP scale degrees from the one before, starting from the
         * track's current first note, so the line carries on across rolls. */
        int notes = p[0] < 1 ? 1 : p[0] > len ? len : p[0], pos[SEQ_STEPS];
        for (int j = 0; j < notes; j++) {
            int a = j * len / notes, gap = (j + 1) * len / notes - a;
            pos[j] = a + (gap > 2 && chance(s, 50) ? pick(s, gap / 2) : 0);
        }
        int first = -1;
        for (int i = 0; i < len && first < 0; i++) if (tr->step[i].on) first = i;
        int cur = first < 0 ? n / 3 : nearest(pool, n, tr->step[first].note);
        for (int i = 0; i < len; i++) on[i] = 0;
        for (int j = 0; j < notes; j++) {
            int st = p[2] < 1 ? 1 : p[2];
            cur += pick(s, 2 * st + 1) - st;
            while (cur < 0 || cur >= n) cur = cur < 0 ? -cur : 2 * (n - 1) - cur;
            if (n == 1) cur = 0;
            int a = pos[j], b = j + 1 < notes ? pos[j + 1] : len + pos[0];   /* the last note can ring into the loop */
            int h = (b - a) * p[1] / 10;
            on[a] = 1;
            idx[a] = cur;
            hold[a] = h < 1 ? 0 : h - 1 > SEQ_MAX_HOLD ? SEQ_MAX_HOLD : h - 1;
            if (h >= 1) gate[a] = 100;
        }
        break;
    }
    case GEN_BELL: {
        /* Sparse notes, never next to each other, on weighted stable tones
         * (root, fifth, third...) HEIGHT octaves above the lowest note, each
         * played only CHANCE of the time and a little softer at random. */
        int lo = 0;
        while (lo < n - 1 && pool[lo] < g->base + 12 * p[2]) lo++;
        int w[64], total = 0;
        for (int k = lo; k < n; k++) {
            int pc = ((pool[k] - s->root) % 12 + 12) % 12;
            w[k] = pc == 0 ? 5 : pc == 7 ? 4 : (pc == 3 || pc == 4) ? 3 : (pc == 2 || pc == 9) ? 2 : 1;
            total += w[k];
        }
        int notes = p[0] > (len + 1) / 2 ? (len + 1) / 2 : p[0];
        for (int i = 0; i < len; i++) on[i] = 0;
        for (int j = 0, tries = 0; j < notes && tries < 200; tries++) {
            int i = pick(s, len);
            if (on[i] || (len > 2 && (on[(i + 1) % len] || on[(i + len - 1) % len]))) continue;
            int r = pick(s, total), k = lo;
            while (r >= w[k]) { r -= w[k]; k++; }
            on[i] = 1;
            idx[i] = k;
            prob[i] = p[1] * 10;
            int base_vel = tr->step[i].vel;
            vel[i] = base_vel - pick(s, p[3] * 4 + 1);
            if (vel[i] < 1) vel[i] = 1;
            j++;
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
        if (hold[i] >= 0) st->hold = (uint8_t)(hold[i] > SEQ_MAX_HOLD ? SEQ_MAX_HOLD : hold[i]);
        if (gate[i] >= 0) st->gate = (uint8_t)(gate[i] < 5 ? 5 : gate[i] > 100 ? 100 : gate[i]);
        if (prob[i] >= 0) st->prob = (uint8_t)(prob[i] > 100 ? 100 : prob[i]);
    }
}

void gen_roll(Seq *s, int t) {
    if (s->track[t].type == TRACK_DRUMS) { gen_drum_roll(s, t); return; }
    GenSettings *g = &s->track[t].gen;
    if (g->type == GEN_CELLULAR) g->p[1] = (uint8_t)((g->p[1] + 1) % 64);
    if (g->type == GEN_TURING) {
        /* One trip round the loop: each bit that wraps may flip. */
        int L = g->p[3] < 2 ? 2 : g->p[3] > 32 ? 32 : g->p[3];
        uint32_t mask = L == 32 ? 0xFFFFFFFFu : (1u << L) - 1;
        uint32_t reg = g->state & mask, top = g->state & ~mask;
        if (reg == 0) reg = 1;
        for (int k = 0; k < L; k++) {
            uint32_t bit = reg & 1;
            if (chance(s, g->p[0] * 5)) bit ^= 1;
            reg = (reg >> 1) | (bit << (L - 1));
        }
        g->state = top | reg;
    }
    gen_apply(s, t);
    /* A style's drum part on a note track rolls a new kit pattern. */
    const SeqTrack *tr = &s->track[t];
    const TrackPreset *pr = &STYLE_SET[style_idx(tr->style)].t[part_idx(tr->part)];
    if (pr->shape == SH_DRUM_NOTES && g->type == pr->gen) gen_drum_notes(s, t);
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
    if (tr->type == TRACK_DRUMS) { gen_drum_evolve(s, t); return; }

    /* Pull first, so this bar's new changes are heard before they can be undone. */
    if (tr->has_anchor && e->pull) gen_morph(s, t, tr->anchor, e->pull);

    if (e->kinds & EVO_REGEN) gen_roll(s, t);

    uint8_t pool[64];
    int n = gen_pool(s, t, pool), len = tr->len;
    int kinds[5], nk = 0;
    if (e->kinds & EVO_PITCH) kinds[nk++] = EVO_PITCH;
    if (e->kinds & EVO_RHYTHM) kinds[nk++] = EVO_RHYTHM;
    if (e->kinds & EVO_VEL) kinds[nk++] = EVO_VEL;
    if (e->kinds & EVO_PROB) kinds[nk++] = EVO_PROB;
    if (e->kinds & EVO_OFFSET) kinds[nk++] = EVO_OFFSET;

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
        case EVO_OFFSET: {   /* 1-3 ticks earlier or later; with NEVER ON GRID it hops over the grid */
            int d = (pick(s, 2) ? 1 : -1) * (1 + pick(s, 3));
            int v = st->nudge + d;
            if (tr->offgrid && v > -SEQ_OFFGRID_MIN && v < SEQ_OFFGRID_MIN) v = d > 0 ? SEQ_OFFGRID_MIN : -SEQ_OFFGRID_MIN;
            st->nudge = (int8_t)(v < -SEQ_MAX_NUDGE ? -SEQ_MAX_NUDGE : v > SEQ_MAX_NUDGE ? SEQ_MAX_NUDGE : v);
            break;
        }
        }
    }

    if ((e->kinds & EVO_ROTATE) && chance(s, e->amount)) gen_transform(s, t, XF_SHIFT, pick(s, 2) ? 1 : -1);

    keep_density(s, t);
}

void gen_morph(Seq *s, int t, const SeqStep *target, int pct) {
    SeqTrack *tr = &s->track[t];
    for (int i = 0; i < tr->len; i++) {
        SeqStep *st = &tr->step[i];
        const SeqStep *an = &target[i];
        if (st->lock) continue;
        int differs = st->on != an->on || st->note != an->note || st->vel != an->vel || st->prob != an->prob ||
                      st->gate != an->gate || st->hold != an->hold;
        if (differs && chance(s, pct)) { *st = *an; st->lock = 0; }
    }
}

void gen_set_anchor(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    memcpy(tr->anchor, tr->step, sizeof tr->anchor);
    for (int i = 0; i < SEQ_STEPS; i++) tr->anchor[i].lock = 0;
    tr->drum_anchor = tr->drum;
    tr->has_anchor = 1;
}

void gen_clear_anchor(Seq *s, int t) { s->track[t].has_anchor = 0; }

/* ---- transforms ---- */

/* ---- chords ---- */

const char *gen_chord_name(int chord) {
    static const char *N[CHORD_COUNT] = {"OFF", "FIFTH", "TRIAD", "SUS2", "SUS4", "SEVENTH", "OPEN"};
    return N[chord < 0 || chord >= CHORD_COUNT ? 0 : chord];
}

int gen_chord_notes(const Seq *s, int note, int chord, uint8_t out[SEQ_CHORD_MAX]) {
    /* Intervals in semitones above the note; the second choice is used when
     * the first is not in the scale (minor third for major third, and so on),
     * and failing both the nearest scale note is taken. */
    static const int8_t IV[CHORD_COUNT][SEQ_CHORD_MAX][2] = {
        [CHORD_FIFTH] = {{7, 0}},
        [CHORD_TRIAD] = {{4, 3}, {7, 0}},
        [CHORD_SUS2] = {{2, 0}, {7, 0}},
        [CHORD_SUS4] = {{5, 0}, {7, 0}},
        [CHORD_SEVENTH] = {{4, 3}, {7, 0}, {11, 10}},
        [CHORD_OPEN] = {{7, 0}, {16, 15}},   /* fifth and the third an octave up */
    };
    int n = 0, last = note;
    if (chord <= CHORD_OFF || chord >= CHORD_COUNT) return 0;
    for (int k = 0; k < SEQ_CHORD_MAX && IV[chord][k][0]; k++) {
        int a = note + IV[chord][k][0], b = IV[chord][k][1] ? note + IV[chord][k][1] : -1;
        int c = a <= 127 && seq_in_scale(s, a) ? a : b >= 0 && b <= 127 && seq_in_scale(s, b) ? b : gen_snap_note(s, a);
        if (c > 127 || c <= last) continue;   /* keep the chord ascending, no doubled notes */
        out[n++] = (uint8_t)c;
        last = c;
    }
    return n;
}

const char *gen_xform_name(int op) {
    static const char *N[XF_COUNT] = {"REVERSE", "INVERT", "TRANSPOSE", "SHIFT", "THIN", "FILL", "HUMANIZE", "CLEAR", "SPREAD", "ON GRID"};
    return op >= 0 && op < XF_COUNT ? N[op] : "?";
}

void gen_transform(Seq *s, int t, int op, int amount) {
    SeqTrack *tr = &s->track[t];
    if (tr->type == TRACK_DRUMS) { gen_drum_transform(s, t, op, amount); return; }
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
    case XF_CLEAR:
        for (int k = 0; k < np; k++) tr->step[pos[k]].on = 0;
        break;
    case XF_SPREAD: {
        /* Offsets off the grid: -a..-MIN and MIN..a. Deal them out in a shuffled
         * order, so no two notes share one until every offset is used. */
        int a = amount < 0 ? -amount : amount;
        a = a < SEQ_OFFGRID_MIN ? SEQ_MAX_NUDGE : a > SEQ_MAX_NUDGE ? SEQ_MAX_NUDGE : a;
        int8_t deck[2 * SEQ_MAX_NUDGE];
        int nd = 0, used = 0;
        for (int v = SEQ_OFFGRID_MIN; v <= a; v++) { deck[nd++] = (int8_t)v; deck[nd++] = (int8_t)-v; }
        for (int k = 0; k < np; k++) {
            SeqStep *st = &tr->step[pos[k]];
            if (!st->on) continue;
            if (used == 0)   /* (re)shuffle the deck */
                for (int i = nd - 1; i > 0; i--) { int j = pick(s, i + 1); int8_t x = deck[i]; deck[i] = deck[j]; deck[j] = x; }
            st->nudge = deck[used];
            used = (used + 1) % nd;
        }
        break;
    }
    case XF_GRID:
        for (int k = 0; k < np; k++) tr->step[pos[k]].nudge = 0;
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
