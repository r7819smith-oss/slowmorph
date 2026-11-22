#include "mod.h"

#include "engine.h"

#include <stdio.h>
#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- rates ---- */

/* Synced cycle lengths in ticks (96 per beat, 384 per 4/4 bar). */
static const uint32_t SYNC_TICKS[] = {24, 48, 96, 192, 384, 768, 1152, 1536, 2304, 3072, 4608, 6144, 9216, 12288, 18432, 24576};
#define SYNC_N (int)(sizeof SYNC_TICKS / sizeof SYNC_TICKS[0])
/* Free cycle lengths in tenths of a second: 0.1 s (10 Hz) to 10 minutes. */
static const int FREE_TENTHS[] = {1, 2, 3, 5, 7, 10, 15, 20, 30, 40, 50, 70, 100, 150, 200, 300, 450, 600, 900, 1200, 1800, 2400, 3600, 4800, 6000};
#define FREE_N (int)(sizeof FREE_TENTHS / sizeof FREE_TENTHS[0])

int mod_rate_count(int sync) { return sync ? SYNC_N : FREE_N; }
uint32_t mod_sync_ticks(int rate) { return SYNC_TICKS[clampi(rate, 0, SYNC_N - 1)]; }
int mod_free_tenths(int rate) { return FREE_TENTHS[clampi(rate, 0, FREE_N - 1)]; }

void mod_rate_name(int sync, int rate, char *out, int n) {
    if (sync) {
        uint32_t t = mod_sync_ticks(rate);
        if (t < 384) snprintf(out, (size_t)n, "1/%u", (unsigned)(384 / t));
        else if (t == 384) snprintf(out, (size_t)n, "1 BAR");
        else snprintf(out, (size_t)n, "%u BARS", (unsigned)(t / 384));
        return;
    }
    int d = mod_free_tenths(rate);
    if (d < 100 && d % 10) snprintf(out, (size_t)n, "%d.%d S", d / 10, d % 10);
    else if (d < 600) snprintf(out, (size_t)n, "%d S", d / 10);
    else if (d % 600) snprintf(out, (size_t)n, "%d.5 MIN", d / 600);
    else snprintf(out, (size_t)n, "%d MIN", d / 600);
}

const char *mod_wave_name(int w) {
    static const char *N[WAVE_COUNT] = {"SINE", "TRIANGLE", "RAMP UP", "RAMP DOWN", "SQUARE", "S+H RANDOM", "SMOOTH RAND"};
    return N[clampi(w, 0, WAVE_COUNT - 1)];
}

void mod_to_name(int to, char *out, int n) {
    if (to < TO_ALL) snprintf(out, (size_t)n, "T%d", to + 1);
    else if (to == TO_ALL) snprintf(out, (size_t)n, "T1-T4");
    else if (to == TO_ALL2) snprintf(out, (size_t)n, "T5-T8");
    else if (to == TO_ALL8) snprintf(out, (size_t)n, "T1-T8");
    else snprintf(out, (size_t)n, "CH %d", to - TO_CH1 + 1);
}

const char *mod_felucca_cc(int cc) {
    switch (cc) {   /* firmware/src/midi_control.c MIDI_CC_MAP; CC1 = the mod wheel (matrix MODW source) */
    case 1: return "MOD WHEEL";
    case 5: return "GLIDE";
    case 7: return "LEVEL";
    case 10: return "PAN";
    case 71: return "RESONANCE";
    case 72: return "RELEASE";
    case 73: return "ATTACK";
    case 74: return "CUTOFF";
    case 75: return "DECAY";
    case 91: return "REVERB";
    case 93: return "CHORUS";
    case 94: return "DELAY";
    default: return NULL;
    }
}

const char *mod_gm_cc(int cc) {
    switch (cc) {
    case 1: return "MOD WHEEL";
    case 2: return "BREATH";
    case 4: return "FOOT";
    case 5: return "PORTA TIME";
    case 7: return "VOLUME";
    case 8: return "BALANCE";
    case 10: return "PAN";
    case 11: return "EXPRESSION";
    case 64: return "SUSTAIN";
    case 71: return "RESONANCE";
    case 72: return "RELEASE";
    case 73: return "ATTACK";
    case 74: return "CUTOFF";
    case 75: return "DECAY";
    case 91: return "REVERB";
    case 93: return "CHORUS";
    default: return NULL;
    }
}

int mod_cc_reserved(int cc) {
    return cc == 0 || cc == 32 || cc == 6 || cc == 38 || (cc >= 96 && cc <= 101) || cc >= 120;
}

/* ---- setup ---- */

void mod_init(Mod *m) {
    memset(m, 0, sizeof *m);
    m->on = 1;
    m->rng = 0x6D2B79F5u;
    /* Slow shapes for ambient work; every destination starts off. */
    static const ModLfo L[MOD_LFOS] = {
        {.wave = WAVE_SINE, .sync = 1, .rate = 9},       /* 8 bars */
        {.wave = WAVE_SMOOTH, .sync = 0, .rate = 15},    /* 30 s */
        {.wave = WAVE_TRI, .sync = 1, .rate = 11},       /* 16 bars */
        {.wave = WAVE_SH, .sync = 1, .rate = 4},         /* 1 bar */
    };
    static const ModDest D[4] = {
        {0, 0, TO_T1 + 0, 74, 40, 90, 0, 0, 0},   /* filter on T1, never fully closed or open */
        {0, 1, TO_T1 + 1, 10, 32, 96, 0, 0, 0},   /* wandering pan on T2 */
        {0, 2, TO_T1 + 2, 94, 20, 70, 0, 0, 0},   /* delay on T3 */
        {0, 3, TO_T1 + 3, 91, 30, 80, 0, 0, 0},   /* reverb on T4 */
    };
    for (int l = 0; l < MOD_LFOS; l++) { m->lfo[l] = L[l]; m->lfo[l].r0 = m->lfo[l].r1 = 32768; m->lfo[l].out = 0; }
    for (int d = 0; d < MOD_DESTS; d++) {
        m->dest[d] = d < 4 ? D[d] : (ModDest){0, (uint8_t)(d % MOD_LFOS), (uint8_t)(TO_T1 + d % 4), 74, 32, 96, 0, 0, 0};
        m->dest[d].last = -1;
    }
    for (int l = 0; l < MOD_LFOS; l++) m->lfo[l].out = mod_wave_at(m->lfo[l].wave, m->lfo[l].phase / 100.0, 32768, 32768);
}

void mod_restart(Mod *m) {
    for (int l = 0; l < MOD_LFOS; l++)
        if (m->lfo[l].sync) m->lfo[l].count = 0;
}

void mod_touch(Mod *m, int d) {
    if (d >= 0 && d < MOD_DESTS) m->dest[d].last = -1;
}

void mod_set_lo(Mod *m, int d, int v) {
    ModDest *x = &m->dest[d];
    x->lo = (uint8_t)clampi(v, 0, 127);
    if (x->hi < x->lo) x->hi = x->lo;
}

void mod_set_hi(Mod *m, int d, int v) {
    ModDest *x = &m->dest[d];
    x->hi = (uint8_t)clampi(v, 0, 127);
    if (x->lo > x->hi) x->lo = x->hi;
}

void mod_shift(Mod *m, int d, int delta) {
    ModDest *x = &m->dest[d];
    int w = x->hi - x->lo, lo = clampi(x->lo + delta, 0, 127 - w);
    x->lo = (uint8_t)lo;
    x->hi = (uint8_t)(lo + w);
}

/* ---- the LFOs ---- */

static uint16_t rnd16(Mod *m) {
    uint32_t x = m->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    m->rng = x;
    return (uint16_t)(x >> 8);
}

/* sin(pi * t) for t in 0..1, Bhaskara's approximation (error under 0.2%,
 * well inside one CC step), so no maths library is needed. */
static double half_sine(double t) {
    double q = t * (1 - t);
    return 16 * q / (5 - 4 * q);
}

double mod_wave_at(int wave, double p, uint16_t r0, uint16_t r1) {
    p -= (int)p;
    if (p < 0) p += 1;
    switch (wave) {
    case WAVE_SINE: {   /* 0.5 - 0.5 cos(2 pi p): starts at LOW, top at half way */
        double c = p < 0.25 ? half_sine(p * 2 + 0.5) : p < 0.75 ? -half_sine(p * 2 - 0.5) : half_sine(p * 2 - 1.5);
        return 0.5 - 0.5 * c;
    }
    case WAVE_TRI: return p < 0.5 ? p * 2 : 2 - p * 2;
    case WAVE_RAMP_UP: return p;
    case WAVE_RAMP_DOWN: return 1 - p;
    case WAVE_SQUARE: return p < 0.5 ? 0 : 1;
    case WAVE_SH: return r1 / 65535.0;
    default: {   /* smooth random: an S-curve from the last value to the next */
        double s = p * p * (3 - 2 * p);
        return (r0 + (r1 - r0) * s) / 65535.0;
    }
    }
}

static double lfo_pos(const ModLfo *l) {
    double p = l->sync ? (double)(l->count % mod_sync_ticks(l->rate)) / mod_sync_ticks(l->rate) : l->fph;
    p += l->phase / 100.0;
    return p >= 1 ? p - 1 : p;
}

double mod_lfo_out(const Mod *m, int l) { return m->lfo[clampi(l, 0, MOD_LFOS - 1)].out; }

static void lfo_advance(Mod *m, ModLfo *l, int bpm, int playing) {
    double before = lfo_pos(l);
    if (!l->hold && !(l->stop_hold && !playing)) {
        if (l->sync) l->count++;
        else {
            l->fph += 60.0 / ((double)bpm * MOD_PPQN) / (mod_free_tenths(l->rate) / 10.0);
            while (l->fph >= 1) l->fph -= 1;
        }
    }
    double p = lfo_pos(l);
    if (p < before) {   /* a new cycle: the next random value */
        l->cycle++;
        l->r0 = l->r1;
        l->r1 = rnd16(m);
    }
    l->out = mod_wave_at(l->wave, p, l->r0, l->r1);
}

/* ---- destinations ---- */

int mod_value(const Mod *m, int d) {
    const ModDest *x = &m->dest[d];
    double u = m->lfo[x->lfo % MOD_LFOS].out;
    if (x->inv) u = 1 - u;
    int v = x->lo + (int)((x->hi - x->lo) * u + 0.5);
    return clampi(v, x->lo, x->hi);
}

int mod_channels(const Mod *m, int d, const uint8_t ch[MOD_TRACKS], uint8_t out[MOD_TRACKS]) {
    int to = m->dest[d].to;
    if (to < TO_ALL) { out[0] = ch[to] & 15; return 1; }
    if (to >= TO_CH1) { out[0] = (uint8_t)(to - TO_CH1); return 1; }
    int n = 0, t0 = to == TO_ALL2 ? 4 : 0, t1 = to == TO_ALL ? 4 : MOD_TRACKS;
    for (int t = t0; t < t1; t++) {
        int dup = 0;
        for (int k = 0; k < n; k++) dup |= out[k] == (ch[t] & 15);
        if (!dup) out[n++] = ch[t] & 15;
    }
    return n;
}

int mod_find(const Mod *m, int chan, int cc, const uint8_t ch[MOD_TRACKS], int skip) {
    for (int d = 0; d < MOD_DESTS; d++) {
        if (d == skip || !m->dest[d].on || m->dest[d].cc != cc) continue;
        uint8_t c[MOD_TRACKS];
        int n = mod_channels(m, d, ch, c);
        for (int k = 0; k < n; k++) if (c[k] == chan) return d;
    }
    return -1;
}

static int send_dest(Mod *m, int d, int v, const uint8_t ch[MOD_TRACKS], ModSend send, void *ctx) {
    uint8_t c[MOD_TRACKS];
    int n = mod_channels(m, d, ch, c);
    for (int k = 0; k < n; k++) {
        uint8_t msg[3] = {(uint8_t)(0xB0 | c[k]), (uint8_t)(m->dest[d].cc & 127), (uint8_t)clampi(v, 0, 127)};
        if (send) send(ctx, msg, 3);
    }
    m->dest[d].last = (int16_t)v;
    m->dest[d].last_at = m->ticks;
    m->sent += (uint32_t)n;
    return n;
}

int mod_send_once(Mod *m, int d, const uint8_t ch[MOD_TRACKS], ModSend send, void *ctx) {
    if (d < 0 || d >= MOD_DESTS) return 0;
    return send_dest(m, d, mod_value(m, d), ch, send, ctx);
}

void mod_tick(Mod *m, int bpm, int playing, const uint8_t ch[MOD_TRACKS], ModSend send, void *ctx) {
    m->ticks++;
    if (bpm < 1) bpm = 1;
    for (int l = 0; l < MOD_LFOS; l++) lfo_advance(m, &m->lfo[l], bpm, playing);
    if (!m->on) return;
    uint32_t gap = (uint32_t)(((long)bpm * MOD_PPQN * MOD_GAP_MS + 59999) / 60000);   /* rounded up: never under 20 ms */
    if (gap < 1) gap = 1;
    int budget = MOD_PER_TICK;
    for (int k = 0; k < MOD_DESTS && budget > 0; k++) {
        int d = (m->rr + k) % MOD_DESTS;
        ModDest *x = &m->dest[d];
        if (!x->on) continue;
        int v = mod_value(m, d);
        if (v == x->last) continue;                              /* never the same value twice */
        if (x->last >= 0 && m->ticks - x->last_at < gap) continue;  /* not too often */
        uint8_t c[MOD_TRACKS];
        /* next tick, unless this tick has its whole budget: a group of more
         * channels than MOD_PER_TICK (T1-T8) still goes out, in one burst */
        if (mod_channels(m, d, ch, c) > budget && budget < MOD_PER_TICK) break;
        budget -= send_dest(m, d, v, ch, send, ctx);
    }
    m->rr = (uint8_t)((m->rr + 1) % MOD_DESTS);
}

/* ---- saving ---- */

/* SEND TO as written in a pattern file: the 4-track numbers stay as they were
 * (0-3 T1-T4, 4 T1-T4 ALL, 5-20 CH 1-16); T5-T8 are 21-24, T5-T8 ALL 25,
 * T1-T8 ALL 26. So older builds read T1-T4 and channel destinations right. */
static int to_file(int to) {
    if (to < 4) return to;
    if (to < TO_ALL) return 21 + to - 4;
    if (to == TO_ALL) return 4;
    if (to == TO_ALL2) return 25;
    if (to == TO_ALL8) return 26;
    return 5 + to - TO_CH1;
}

static int to_mem(int v) {
    if (v >= 0 && v < 4) return v;
    if (v == 4) return TO_ALL;
    if (v >= 5 && v <= 20) return TO_CH1 + v - 5;
    if (v >= 21 && v <= 24) return 4 + v - 21;
    if (v == 25) return TO_ALL2;
    if (v == 26) return TO_ALL8;
    return v < 0 ? TO_T1 : TO_CH1 + 15;   /* out of range: clamped, as before */
}

void mod_save(const Mod *m, FILE *f) {
    fprintf(f, "mo %d\n", m->on);
    for (int l = 0; l < MOD_LFOS; l++) {
        const ModLfo *x = &m->lfo[l];
        fprintf(f, "lf %d %d %d %d %d %d %d\n", l, x->wave, x->sync, x->rate, x->phase, x->hold, x->stop_hold);
    }
    for (int d = 0; d < MOD_DESTS; d++) {
        const ModDest *x = &m->dest[d];
        fprintf(f, "md %d %d %d %d %d %d %d %d\n", d, x->on, x->lfo, to_file(x->to), x->cc, x->lo, x->hi, x->inv);
    }
}

int mod_load_line(Mod *m, const char *line) {
    int a, b, c, d, e, g, h, k;
    if (line[0] == 'm' && line[1] == 'o' && line[2] == ' ') {
        if (sscanf(line, "mo %d", &a) == 1) m->on = a != 0;
        return 1;
    }
    if (line[0] == 'l' && line[1] == 'f' && line[2] == ' ') {
        if (sscanf(line, "lf %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h) == 7 && a >= 0 && a < MOD_LFOS) {
            ModLfo *x = &m->lfo[a];
            x->wave = (uint8_t)clampi(b, 0, WAVE_COUNT - 1);
            x->sync = c != 0;
            x->rate = (uint8_t)clampi(d, 0, mod_rate_count(x->sync) - 1);
            x->phase = (uint8_t)(clampi(e, 0, 95) / 5 * 5);
            x->hold = g != 0;
            x->stop_hold = h != 0;
        }
        return 1;
    }
    if (line[0] == 'm' && line[1] == 'd' && line[2] == ' ') {
        if (sscanf(line, "md %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &h, &k) == 8 && a >= 0 && a < MOD_DESTS) {
            ModDest *x = &m->dest[a];
            x->on = b != 0;
            x->lfo = (uint8_t)clampi(c, 0, MOD_LFOS - 1);
            x->to = (uint8_t)to_mem(d);
            x->cc = (uint8_t)clampi(e, 0, 119);
            x->lo = (uint8_t)clampi(g, 0, 127);
            x->hi = (uint8_t)clampi(h, x->lo, 127);
            x->inv = k != 0;
            x->last = -1;
        }
        return 1;
    }
    return 0;
}

/* ---- MIDI input ---- */

int midi_parse_byte(MidiParse *p, uint8_t b, uint8_t out[3]) {
    if (b >= 0xF8) { out[0] = b; return 1; }   /* real time: any time, even inside another message */
    if (b == 0xF0) { p->sysex = 1; p->status = 0; return 0; }
    if (b == 0xF7) { p->sysex = 0; p->status = 0; return 0; }
    if (b & 0x80) {
        p->sysex = 0;
        p->n = 0;
        if (b < 0xF0) { p->status = b; p->need = (b & 0xF0) == 0xC0 || (b & 0xF0) == 0xD0 ? 1 : 2; return 0; }
        p->status = 0;   /* system common cancels running status */
        if (b == 0xF1 || b == 0xF3) { p->status = b; p->need = 1; return 0; }
        if (b == 0xF2) { p->status = b; p->need = 2; return 0; }
        if (b == 0xF6) { out[0] = b; return 1; }
        return 0;   /* 0xF4, 0xF5: undefined */
    }
    if (p->sysex || !p->status) return 0;
    p->data[p->n++] = b;
    if (p->n < p->need) return 0;
    out[0] = p->status;
    out[1] = p->data[0];
    out[2] = p->need > 1 ? p->data[1] : 0;
    int len = 1 + p->need;
    p->n = 0;
    if (p->status >= 0xF0) p->status = 0;
    return len;
}

void mon_clear(MidiMon *mon) { memset(mon, 0, sizeof *mon); }

void mon_add(MidiMon *mon, const uint8_t *msg, int len, int echo) {
    if (len <= 0) return;
    uint8_t st = msg[0] < 0xF0 ? msg[0] & 0xF0 : msg[0];
    if (st == 0xF8) { mon->clocks++; return; }
    if (st == 0xFE) { mon->other++; return; }   /* active sensing: counted, not listed */
    if (st == 0x80 || st == 0x90) mon->notes++;
    else if (st == 0xB0) mon->ccs++;
    else mon->other++;
    int i = mon->head;
    memcpy(mon->msg[i], msg, (size_t)(len > 3 ? 3 : len));
    mon->len[i] = (uint8_t)(len > 3 ? 3 : len);
    mon->echo[i] = (uint8_t)(echo != 0);
    mon->head = (i + 1) % MON_LINES;
    if (mon->nlog < MON_LINES) mon->nlog++;
}

int mon_get(const MidiMon *mon, int k, uint8_t msg[3], int *echo) {
    if (k < 0 || k >= mon->nlog) return 0;
    int i = ((mon->head - 1 - k) % MON_LINES + MON_LINES) % MON_LINES;
    memcpy(msg, mon->msg[i], 3);
    if (echo) *echo = mon->echo[i];
    return mon->len[i];
}

void mon_text(const uint8_t *msg, int len, char *out, int n) {
    size_t sz = (size_t)n;
    if (len <= 0) { snprintf(out, sz, "-"); return; }
    uint8_t st = msg[0] < 0xF0 ? msg[0] & 0xF0 : msg[0];
    int ch = (msg[0] & 15) + 1;
    char nn[8];
    switch (st) {
    case 0x80: seq_note_name(msg[1], nn); snprintf(out, sz, "CH %-2d NOTE OFF %s", ch, nn); break;
    case 0x90:
        seq_note_name(msg[1], nn);
        if (msg[2]) snprintf(out, sz, "CH %-2d NOTE ON %s VEL %d", ch, nn, msg[2]);
        else snprintf(out, sz, "CH %-2d NOTE OFF %s", ch, nn);
        break;
    case 0xA0: seq_note_name(msg[1], nn); snprintf(out, sz, "CH %-2d KEY PRESSURE %s %d", ch, nn, msg[2]); break;
    case 0xB0: snprintf(out, sz, "CH %-2d CC %d = %d", ch, msg[1], msg[2]); break;
    case 0xC0: snprintf(out, sz, "CH %-2d PROGRAM %d", ch, msg[1] + 1); break;
    case 0xD0: snprintf(out, sz, "CH %-2d PRESSURE %d", ch, msg[1]); break;
    case 0xE0: snprintf(out, sz, "CH %-2d PITCH BEND %+d", ch, (msg[1] | msg[2] << 7) - 8192); break;
    case 0xF1: snprintf(out, sz, "TIME CODE"); break;
    case 0xF2: snprintf(out, sz, "SONG POSITION %d", msg[1] | msg[2] << 7); break;
    case 0xF3: snprintf(out, sz, "SONG SELECT %d", msg[1]); break;
    case 0xF6: snprintf(out, sz, "TUNE REQUEST"); break;
    case 0xFA: snprintf(out, sz, "START"); break;
    case 0xFB: snprintf(out, sz, "CONTINUE"); break;
    case 0xFC: snprintf(out, sz, "STOP"); break;
    case 0xFF: snprintf(out, sz, "RESET"); break;
    default: snprintf(out, sz, "%02X", msg[0]); break;
    }
}

int mod_learn(Mod *m, int d, const uint8_t *msg, int len, const uint8_t ch[MOD_TRACKS]) {
    if (d < 0 || d >= MOD_DESTS || len < 3 || (msg[0] & 0xF0) != 0xB0 || msg[1] >= 120) return 0;
    int chan = msg[0] & 15, to = TO_CH1 + chan;
    for (int t = 0; t < MOD_TRACKS; t++)
        if ((ch[t] & 15) == chan) { to = TO_T1 + t; break; }
    m->dest[d].cc = msg[1];
    m->dest[d].to = (uint8_t)to;
    mod_touch(m, d);
    return 1;
}
