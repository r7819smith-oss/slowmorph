#include "drum.h"

#include <stdio.h>
#include <string.h>

#include "gen.h"

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int pick(Seq *s, int n) { return n > 0 ? (int)(seq_rand(s) % (uint32_t)n) : 0; }
static int chance(Seq *s, int pct) { return (int)(seq_rand(s) % 100) < pct; }
static int range(Seq *s, int lo, int hi) { return lo + pick(s, hi - lo + 1); }

/* ---- DRUM MAP ---- */

/* Presets. FELUCCA: the notes Felucca's DRUM engine designs each of its 8
 * lanes on (firmware eng_drum.c DRUM_LANE_NOTE: KICK 36, SNARE 38, CLAP 39,
 * HAT CL 42, HAT OP 46, TOM 45, RIM 37, BELL 56), in seq4h's lane order.
 * GM: the General MIDI drum notes, with a crash (49) for the last lane. */
static const uint8_t PRESET_NOTE[DRUM_PRESETS][SEQ_LANES] = {
    {36, 38, 42, 46, 39, 37, 45, 56},
    {36, 38, 42, 46, 39, 37, 45, 49},
};
static const char *PRESET_LANE[DRUM_PRESETS][SEQ_LANES] = {
    {"KICK", "SNARE", "HAT CL", "HAT OP", "CLAP", "RIM", "TOM", "BELL"},
    {"KICK", "SNARE", "CL HAT", "OP HAT", "CLAP", "RIM", "TOM", "CRASH"},
};
static const char *PRESET_NAME[DRUM_PRESETS] = {"FELUCCA", "GM"};

const char *seq_lane_gm_name(int lane) { return PRESET_LANE[DRUM_FELUCCA][clampi(lane, 0, SEQ_LANES - 1)]; }
const char *seq_drum_preset_name(int k) { return PRESET_NAME[clampi(k, 0, DRUM_PRESETS - 1)]; }

void seq_drum_map_preset(Seq *s, int k) {
    k = clampi(k, 0, DRUM_PRESETS - 1);
    for (int l = 0; l < SEQ_LANES; l++) {
        s->lane_note[l] = PRESET_NOTE[k][l];
        snprintf(s->lane_name[l], sizeof s->lane_name[l], "%s", PRESET_LANE[k][l]);
    }
}

void seq_drum_map_gm(Seq *s) { seq_drum_map_preset(s, DRUM_FELUCCA); }

int seq_drum_map_is(const Seq *s, int k) {
    k = clampi(k, 0, DRUM_PRESETS - 1);
    for (int l = 0; l < SEQ_LANES; l++)
        if (s->lane_note[l] != PRESET_NOTE[k][l] || strcmp(s->lane_name[l], PRESET_LANE[k][l])) return 0;
    return 1;
}

/* What Felucca's DRUM engine (STD kit) plays for each note 35..81 (firmware
 * eng_drum.c DRUM_GM): the sound and its semitones from that sound's own
 * pitch. Other notes play as their octave of 36..47. */
enum { FS_KICK, FS_RIM, FS_SNARE, FS_CLAP, FS_HATC, FS_HATO, FS_TOM, FS_CYM, FS_BELL, FS_CLAVE, FS_CONGA };
static const char *FS_NAME[] = {"KICK", "RIM", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "CYMBAL", "BELL", "CLAVE", "CONGA"};
static const int8_t FELUCCA_GM[47][2] = {
    {FS_KICK, -2}, {FS_KICK, 0}, {FS_RIM, 0}, {FS_SNARE, 0}, {FS_CLAP, 0}, {FS_SNARE, 2},       /* 35 */
    {FS_TOM, -7}, {FS_HATC, 0}, {FS_TOM, -4}, {FS_HATC, -2}, {FS_TOM, 0}, {FS_HATO, 0},          /* 41 */
    {FS_TOM, 3}, {FS_TOM, 5}, {FS_CYM, 0}, {FS_TOM, 8}, {FS_CYM, -3}, {FS_CYM, 2},               /* 47 */
    {FS_BELL, 5}, {FS_HATC, 5}, {FS_CYM, 4}, {FS_BELL, 0}, {FS_CYM, 1}, {FS_CLAVE, -12},         /* 53 */
    {FS_CYM, -2}, {FS_CONGA, 5}, {FS_CONGA, 2}, {FS_CONGA, 0}, {FS_CONGA, 0}, {FS_CONGA, -5},    /* 59 */
    {FS_TOM, 10}, {FS_TOM, 7}, {FS_BELL, 7}, {FS_BELL, 3}, {FS_HATC, 3}, {FS_HATC, 7},           /* 65 */
    {FS_CLAVE, 7}, {FS_CLAVE, 5}, {FS_HATC, -4}, {FS_HATC, -6}, {FS_CLAVE, 0}, {FS_CLAVE, -4},   /* 71 */
    {FS_CLAVE, -7}, {FS_CONGA, 7}, {FS_CONGA, 3}, {FS_BELL, 12}, {FS_BELL, 12},                  /* 77 */
};

static int felucca_index(int note) {
    note = clampi(note, 0, 127);
    return (note >= 35 && note <= 81 ? note : 36 + (note + 120 - 36) % 12) - 35;
}

const char *seq_felucca_sound(int note, int *semis) {
    const int8_t *e = FELUCCA_GM[felucca_index(note)];
    if (semis) *semis = e[1];
    return FS_NAME[(int)e[0]];
}

int seq_felucca_next_sound(int note, int dir) {
    int cur = FELUCCA_GM[felucca_index(note)][0];
    dir = dir < 0 ? -1 : 1;
    for (int n = clampi(note, 35, 81) + dir; n >= 35 && n <= 81; n += dir) {
        const int8_t *e = FELUCCA_GM[n - 35];
        if (e[1] == 0 && e[0] != cur) return n;   /* the next sound, at its own pitch */
    }
    return note;
}

void seq_lane_note(Seq *s, int lane, int d) {
    if (lane < 0 || lane >= SEQ_LANES) return;
    s->lane_note[lane] = (uint8_t)clampi(s->lane_note[lane] + d, 0, 127);
}

void seq_lane_set_name(Seq *s, int lane, const char *name) {
    if (lane < 0 || lane >= SEQ_LANES) return;
    char buf[SEQ_LANE_NAME + 1];
    int n = 0;
    for (; name && name[n] && n < SEQ_LANE_NAME; n++) {
        char c = name[n];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        buf[n] = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ? c : ' ';
    }
    while (n > 0 && buf[n - 1] == ' ') n--;
    buf[n] = 0;
    int lead = 0;
    while (buf[lead] == ' ') lead++;
    if (!buf[lead]) snprintf(buf, sizeof buf, "%s", seq_lane_gm_name(lane));
    memcpy(s->lane_name[lane], buf, sizeof buf);
}

/* ---- editing ---- */

void seq_track_type(Seq *s, int t, int type) {
    SeqTrack *tr = &s->track[t];
    type = type == TRACK_DRUMS ? TRACK_DRUMS : TRACK_NOTES;
    if (tr->type == type) return;
    /* stop whatever the old kind of track has sounding */
    if (tr->sounding >= 0) {
        uint8_t m[3] = {(uint8_t)(0x80 | tr->channel), (uint8_t)tr->sounding, 0};
        if (s->send) s->send(s->ctx, m, 3);
        for (int k = 0; k < tr->nvoice; k++) { m[1] = tr->voice[k]; if (s->send) s->send(s->ctx, m, 3); }
        tr->sounding = -1;
        tr->nvoice = 0;
    }
    for (int l = 0; l < SEQ_LANES; l++)
        if (tr->drum_down & (1 << l)) {
            uint8_t m[3] = {(uint8_t)(0x80 | tr->channel), tr->drum_note[l], 0};
            if (s->send) s->send(s->ctx, m, 3);
        }
    tr->drum_down = 0;
    tr->type = (uint8_t)type;
    seq_reshuffle(s, t);   /* drum steps have no OFFSET of their own */
}

int seq_drum_column(const SeqTrack *tr, int i) {
    for (int l = 0; l < SEQ_LANES; l++) if (tr->drum.vel[l][i]) return 1;
    return 0;
}

int seq_drum_hits(const SeqTrack *tr) {
    int n = 0;
    for (int l = 0; l < SEQ_LANES; l++)
        for (int i = 0; i < tr->len; i++) n += tr->drum.vel[l][i] != 0;
    return n;
}

void seq_drum_toggle(Seq *s, int t, int lane, int i) {
    if (lane < 0 || lane >= SEQ_LANES || i < 0 || i >= SEQ_STEPS) return;
    SeqDrums *d = &s->track[t].drum;
    if (d->vel[lane][i]) d->vel[lane][i] = 0;
    else { d->vel[lane][i] = 100; d->prob[lane][i] = 100; }
}

void seq_drum_adjust(Seq *s, int t, int lane, int i, int dv, int dp) {
    if (lane < 0 || lane >= SEQ_LANES || i < 0 || i >= SEQ_STEPS) return;
    SeqDrums *d = &s->track[t].drum;
    if (!d->vel[lane][i]) { d->vel[lane][i] = 100; d->prob[lane][i] = 100; }
    d->vel[lane][i] = (uint8_t)clampi(d->vel[lane][i] + dv, 1, 127);
    d->prob[lane][i] = (uint8_t)clampi(d->prob[lane][i] + dp, 0, 100);
}

void seq_drum_lock(Seq *s, int t, int lane) {
    if (lane >= 0 && lane < SEQ_LANES) s->track[t].drum_lock ^= (uint8_t)(1 << lane);
}

void seq_audition(Seq *s, int ch, int note, int vel) {
    if (s->aud_left) {   /* end the last one first */
        uint8_t off[3] = {(uint8_t)(0x80 | s->aud_ch), (uint8_t)s->aud_note, 0};
        if (s->send) s->send(s->ctx, off, 3);
    }
    s->aud_ch = clampi(ch, 0, 15);
    s->aud_note = clampi(note, 0, 127);
    s->aud_left = SEQ_PPQN / 2;
    uint8_t on[3] = {(uint8_t)(0x90 | s->aud_ch), (uint8_t)s->aud_note, (uint8_t)clampi(vel, 1, 127)};
    if (s->send) s->send(s->ctx, on, 3);
}

/* ---- generator ---- */

/* A lane's part in a style's kit: `k` hits per 16 steps, evenly spread from
 * step `first` (-1 = anywhere, -2 = on an offbeat 8th), with velocity and
 * chance in the given ranges; `ghost` extra quiet hits anywhere free. */
typedef struct { uint8_t k; int8_t first; uint8_t vlo, vhi, plo, phi, ghost; } LanePart;

enum { KIT_AMBIENT, KIT_TECHNO, KIT_MELODIC, KIT_COUNT };
static const LanePart KIT[KIT_COUNT][SEQ_LANES] = {
    /* Ambient (also DRONE and BLANK): sparse and soft, lots left to chance. */
    [KIT_AMBIENT] = {
        {1, 0, 70, 90, 100, 100, 0},     /* kick on the one */
        {0, 0, 0, 0, 0, 0, 0},           /* snare */
        {4, -1, 35, 60, 50, 85, 0},      /* closed hat */
        {1, -1, 40, 55, 30, 50, 0},      /* open hat */
        {0, 0, 0, 0, 0, 0, 0},           /* clap */
        {2, -1, 40, 60, 40, 70, 0},      /* rim */
        {1, -1, 45, 60, 25, 40, 0},      /* tom */
        {1, 0, 40, 55, 15, 25, 0},       /* crash, now and then on the one */
    },
    /* Techno: four on the floor, clap on 2 and 4, offbeat hats with ghosts. */
    [KIT_TECHNO] = {
        {4, 0, 115, 125, 100, 100, 0},
        {0, 0, 0, 0, 0, 0, 0},
        {4, 2, 85, 100, 100, 100, 4},
        {1, -2, 70, 85, 50, 70, 0},
        {2, 4, 100, 110, 100, 100, 0},
        {2, -1, 50, 65, 40, 60, 0},
        {0, 0, 0, 0, 0, 0, 0},
        {1, 0, 70, 80, 20, 20, 0},
    },
    /* Melodic: a plain backbeat with 8th-note hats. */
    [KIT_MELODIC] = {
        {2, 0, 100, 115, 100, 100, 1},
        {2, 4, 90, 105, 100, 100, 0},
        {8, 0, 60, 80, 90, 100, 0},
        {1, -2, 60, 70, 50, 50, 0},
        {0, 0, 0, 0, 0, 0, 0},
        {1, -1, 50, 60, 50, 50, 0},
        {1, -1, 60, 70, 30, 30, 0},
        {1, 0, 70, 70, 25, 25, 0},
    },
};

static int kit_for(int style) { return style == STYLE_TECHNO ? KIT_TECHNO : style == STYLE_MELODIC ? KIT_MELODIC : KIT_AMBIENT; }

/* Hits scaled from 16 steps to len and by busy (0 = half, 50 = as written, 100 = 1.5x). */
static int scaled_hits(int k, int len, int busy) {
    if (!k) return 0;
    int v = (k * (busy + 50) * len + 800) / 1600;
    return clampi(v, 1, len);
}

void gen_drum_roll(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    int len = tr->len, kit = kit_for(s->style), busy = tr->drum_busy;
    for (int l = 0; l < SEQ_LANES; l++) {
        if (tr->drum_lock & (1 << l)) continue;
        const LanePart *p = &KIT[kit][l];
        for (int i = 0; i < SEQ_STEPS; i++) { tr->drum.vel[l][i] = 0; tr->drum.prob[l][i] = 100; }
        int k = scaled_hits(p->k, len, busy);
        int first = p->first >= 0 ? p->first % len : p->first == -2 ? (2 + 4 * pick(s, 4)) % len : pick(s, len);
        for (int i = 0; i < len && k; i++) {
            int j = (i - first + len) % len;
            if ((j * k) % len < k) {
                tr->drum.vel[l][i] = (uint8_t)range(s, p->vlo, p->vhi);
                tr->drum.prob[l][i] = (uint8_t)range(s, p->plo, p->phi);
            }
        }
        for (int g = scaled_hits(p->ghost, len, busy); g > 0; g--) {
            int i = pick(s, len);
            if (tr->drum.vel[l][i]) continue;   /* a ghost never replaces a main hit */
            tr->drum.vel[l][i] = (uint8_t)range(s, 35, 55);
            tr->drum.prob[l][i] = (uint8_t)range(s, 30, 50);
        }
    }
}

/* ---- evolution, morph, transforms ---- */

static void lane_rotate(SeqTrack *tr, int l, int by) {
    int len = tr->len;
    uint8_t v[SEQ_STEPS], p[SEQ_STEPS];
    by = ((by % len) + len) % len;
    for (int i = 0; i < len; i++) { v[(i + by) % len] = tr->drum.vel[l][i]; p[(i + by) % len] = tr->drum.prob[l][i]; }
    memcpy(tr->drum.vel[l], v, (size_t)len);
    memcpy(tr->drum.prob[l], p, (size_t)len);
}

void gen_drum_morph(Seq *s, int t, const SeqDrums *target, int pct) {
    SeqTrack *tr = &s->track[t];
    for (int l = 0; l < SEQ_LANES; l++) {
        if (tr->drum_lock & (1 << l)) continue;
        for (int i = 0; i < tr->len; i++) {
            int differs = tr->drum.vel[l][i] != target->vel[l][i] || (target->vel[l][i] && tr->drum.prob[l][i] != target->prob[l][i]);
            if (differs && chance(s, pct)) { tr->drum.vel[l][i] = target->vel[l][i]; tr->drum.prob[l][i] = target->prob[l][i]; }
        }
    }
}

void gen_drum_evolve(Seq *s, int t) {
    SeqTrack *tr = &s->track[t];
    const EvolveSettings *e = &tr->evo;
    int len = tr->len;
    if (tr->has_anchor && e->pull) gen_drum_morph(s, t, &tr->drum_anchor, e->pull);
    if (e->kinds & EVO_REGEN) gen_drum_roll(s, t);
    for (int l = 0; l < SEQ_LANES; l++) {
        if (tr->drum_lock & (1 << l)) continue;
        uint8_t *v = tr->drum.vel[l], *p = tr->drum.prob[l];
        if ((e->kinds & EVO_RHYTHM) && chance(s, e->amount)) {   /* move one hit to a free neighbour */
            int hits[SEQ_STEPS], n = 0;
            for (int i = 0; i < len; i++) if (v[i]) hits[n++] = i;
            if (n && n < len) {
                int i = hits[pick(s, n)], j = (i + (pick(s, 2) ? 1 : len - 1)) % len;
                if (!v[j]) { v[j] = v[i]; p[j] = p[i]; v[i] = 0; }
            }
        }
        if ((e->kinds & EVO_ROTATE) && chance(s, e->amount)) lane_rotate(tr, l, pick(s, 2) ? 1 : -1);
        for (int i = 0; i < len; i++) {
            if (!v[i]) continue;
            if ((e->kinds & EVO_VEL) && chance(s, e->amount))
                v[i] = (uint8_t)clampi(v[i] + (pick(s, 2) ? 1 : -1) * (8 + pick(s, 12)), 1, 127);
            if ((e->kinds & EVO_PROB) && chance(s, e->amount))
                p[i] = (uint8_t)clampi(p[i] + (pick(s, 2) ? 1 : -1) * (5 + pick(s, 11)), 10, 100);
        }
    }
}

int gen_drum_transform(Seq *s, int t, int op, int amount) {
    SeqTrack *tr = &s->track[t];
    int len = tr->len;
    switch (op) {
    case XF_REVERSE:
        for (int l = 0; l < SEQ_LANES; l++) {
            if (tr->drum_lock & (1 << l)) continue;
            for (int i = 0, j = len - 1; i < j; i++, j--) {
                uint8_t a = tr->drum.vel[l][i]; tr->drum.vel[l][i] = tr->drum.vel[l][j]; tr->drum.vel[l][j] = a;
                a = tr->drum.prob[l][i]; tr->drum.prob[l][i] = tr->drum.prob[l][j]; tr->drum.prob[l][j] = a;
            }
        }
        return 0;
    case XF_SHIFT:
        for (int l = 0; l < SEQ_LANES; l++) if (!(tr->drum_lock & (1 << l))) lane_rotate(tr, l, amount);
        return 0;
    case XF_THIN:   /* the quietest offbeat hits go first */
        for (int r = 0; r < amount; r++) {
            int bl = -1, bi = 0, best = 1 << 30;
            for (int l = 0; l < SEQ_LANES; l++) {
                if (tr->drum_lock & (1 << l)) continue;
                for (int i = 0; i < len; i++) {
                    if (!tr->drum.vel[l][i]) continue;
                    int score = tr->drum.vel[l][i] * 4 + (i % 4 == 0 ? 1000 : i % 2 == 0 ? 500 : 0) + pick(s, 64);
                    if (score < best) { best = score; bl = l; bi = i; }
                }
            }
            if (bl < 0) break;
            tr->drum.vel[bl][bi] = 0;
        }
        return 0;
    case XF_FILL:   /* quiet extra hits on lanes that already play */
        for (int r = 0; r < amount; r++) {
            int lanes[SEQ_LANES], nl = 0;
            for (int l = 0; l < SEQ_LANES; l++) {
                if (tr->drum_lock & (1 << l)) continue;
                int any = 0, free = 0;
                for (int i = 0; i < len; i++) { any |= tr->drum.vel[l][i] != 0; free |= tr->drum.vel[l][i] == 0; }
                if (any && free) lanes[nl++] = l;
            }
            if (!nl) break;
            int l = lanes[pick(s, nl)], cand[SEQ_STEPS], nc = 0, sum = 0, n = 0;
            for (int i = 0; i < len; i++) { if (!tr->drum.vel[l][i]) cand[nc++] = i; else { sum += tr->drum.vel[l][i]; n++; } }
            int i = cand[pick(s, nc)];
            tr->drum.vel[l][i] = (uint8_t)clampi(sum / n * 3 / 4, 1, 127);
            tr->drum.prob[l][i] = 100;
        }
        return 0;
    case XF_HUMANIZE:
        if (amount <= 0) return 0;
        for (int l = 0; l < SEQ_LANES; l++) {
            if (tr->drum_lock & (1 << l)) continue;
            for (int i = 0; i < len; i++)
                if (tr->drum.vel[l][i]) tr->drum.vel[l][i] = (uint8_t)clampi(tr->drum.vel[l][i] + pick(s, 2 * amount + 1) - amount, 1, 127);
        }
        return 0;
    case XF_CLEAR:
        for (int l = 0; l < SEQ_LANES; l++)
            if (!(tr->drum_lock & (1 << l))) memset(tr->drum.vel[l], 0, sizeof tr->drum.vel[l]);
        return 0;
    default:
        return -1;
    }
}
