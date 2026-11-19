#include "ui.h"

#include "battery.h"
#include "gen.h"
#include "input.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define W 640
#define H 480
#define X0 56
#define CELL 35
#define ROW_Y0 70
#define ROW_H 64
#define GRID_COLS 16   /* steps shown at once on the grid */

/* Classic 5x7 font, ASCII 32..95, one byte per column, LSB = top row. */
static const uint8_t FONT[64][5] = {
 {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
 {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0x00,0x08,0x07,0x03,0x00},
 {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
 {0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},{0x20,0x10,0x08,0x04,0x02},
 {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},
 {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
 {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},{0x00,0x40,0x34,0x00,0x00},
 {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},
 {0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
 {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x41,0x51,0x73},
 {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
 {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},
 {0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
 {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43},{0x00,0x7F,0x41,0x41,0x41},
 {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x41,0x7F},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
};

typedef struct { uint8_t r, g, b; } Col;

static const Col TRACK_COL[SEQ_TRACKS] = {{235, 90, 80}, {240, 180, 60}, {90, 200, 130}, {90, 150, 240}};
static const Col WHITE = {240, 240, 240}, DIM = {120, 125, 140}, YELLOW = {255, 230, 80};

static void color(SDL_Renderer *r, Col c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255); }

static void rect(SDL_Renderer *r, int x, int y, int w, int h, Col c) {
    SDL_Rect q = {x, y, w, h};
    color(r, c);
    SDL_RenderFillRect(r, &q);
}

static void frame(SDL_Renderer *r, int x, int y, int w, int h, int t, Col c) {
    rect(r, x, y, w, t, c);
    rect(r, x, y + h - t, w, t, c);
    rect(r, x, y, t, h, c);
    rect(r, x + w - t, y, t, h, c);
}

static void text(SDL_Renderer *r, int x, int y, int sc, Col c, const char *s) {
    color(r, c);
    for (; *s; s++, x += 6 * sc) {
        int ch = (unsigned char)*s;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 95) ch = '?';
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 8; row++)
                if (FONT[ch - 32][col] & (1 << row)) {
                    SDL_Rect q = {x + col * sc, y + row * sc, sc, sc};
                    SDL_RenderFillRect(r, &q);
                }
    }
}

static Col scale(Col c, int pct) { return (Col){(uint8_t)(c.r * pct / 100), (uint8_t)(c.g * pct / 100), (uint8_t)(c.b * pct / 100)}; }

/* Pages, in L1/R1 order. GRID shows all four tracks; the others are lists of
 * values for the current track: up/down picks a row, left/right changes it. */
typedef enum { P_GRID, P_STEP, P_TRACK, P_GEN, P_TABS,   /* P_TABS = pages on L1/R1 */
               P_MENU = P_TABS, P_COUNT } Page;
static const char *PAGE_NAMES[P_COUNT] = {"GRID", "STEP", "TRACK", "GENERATE", "MENU"};

typedef struct {
    Player *p;
    int track, step;
    int on_label;             /* grid cursor is on the track label (left of step 1): A mute, B solo */
    int held[B_COUNT];
    Uint32 next_rep[B_COUNT];
    int quit;
    int midi_up;              /* last MIDI poll result; -1 = not polled yet */
    int page;                 /* Page */
    int row[P_COUNT];         /* selected row on each list page */
    int menu_from;            /* page to return to when the main menu closes */
    SeqStep undo[SEQ_STEPS];  /* the track as it was before the last roll, transform or slot load */
    int undo_len;
    int undo_track;           /* -1 = nothing to undo */
    int xf_op, xf_amt;        /* transform chosen on the generate page, and its amount */
    int slot;                 /* slot chosen on the generate page */
    int snap;                 /* changing scale moves notes into it */
    int confirm_new;          /* NEW FROM STYLE was pressed once */
    const char *msg;
    Uint32 msg_until;
    char msg_buf[48];         /* for messages built on the fly */
    const InputMap *map;
    int show_input;
    char input_msg[48];       /* last raw input, shown with --show-input */
    Battery bat;              /* read every 10 s; pct -1 = no battery (desktop) */
    int bat_logged;           /* the battery has been logged at least once */
    int bat_log_charging;     /* charger state at the last log line */
    Uint32 next_bat_log;      /* next hourly battery line in seq4h.log */
} Ui;

/* Generate page rows. */
enum {
    GR_TRACK, GR_GEN, GR_P0, GR_P1, GR_P2, GR_P3, GR_BASE, GR_SPAN,
    GR_EVOLVE, GR_AMOUNT, GR_EVO0, GR_EVO_LAST = GR_EVO0 + 5, GR_MIN, GR_MAX, GR_ANCHOR, GR_PULL,
    GR_CHORD, GR_SWELL, GR_SWELL_DEPTH, GR_REST,
    GR_XFORM, GR_XAMOUNT, GR_SLOT, GR_STORE, GR_CHAIN, GR_CHAIN_BARS, GR_MORPH,
    GR_SCALE, GR_ROOT, GR_SNAP, GEN_ROWS
};
static const int EVERY[8] = {0, 1, 2, 4, 8, 16, 32, 64};
static const int SWELL_BARS[6] = {0, 2, 4, 8, 16, 32};
static const int CHAIN_BARS[5] = {1, 2, 4, 8, 16};
static const uint8_t EVO_BITS[6] = {EVO_PITCH, EVO_RHYTHM, EVO_VEL, EVO_ROTATE, EVO_REGEN, EVO_PROB};

static int wrap(int v, int n) { return (v % n + n) % n; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int gen_row_is_head(int row) {
    return row == GR_TRACK || row == GR_GEN || row == GR_EVOLVE || row == GR_ANCHOR || row == GR_CHORD || row == GR_XFORM ||
           row == GR_SLOT || row == GR_SCALE;
}

static void gen_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    int t = u->track;
    const SeqTrack *tr = &s->track[t];
    const GenSettings *g = &tr->gen;
    static const char *EVO_NAMES[6] = {"EVO PITCH", "EVO RHYTHM", "EVO VELOCITY", "EVO ROTATE", "EVO REGEN", "EVO PROB"};
    label[0] = val[0] = 0;
    if (row == GR_TRACK) { snprintf(label, 24, "TRACK"); snprintf(val, 24, "%d", t + 1); }
    else if (row == GR_GEN) { snprintf(label, 24, "GENERATOR"); snprintf(val, 24, "%s", gen_name(g->type)); }
    else if (row <= GR_P3) {
        int k = row - GR_P0;
        if (k < gen_param_count(g->type)) {
            snprintf(label, 24, "  %s", gen_param_name(g->type, k));
            char tmp[16];
            gen_param_text(g->type, k, g->p[k], tmp);
            snprintf(val, 24, "%s", tmp);
        } else {
            snprintf(label, 24, "  -");
        }
    }
    else if (row == GR_BASE) { snprintf(label, 24, "LOWEST NOTE"); char nn[8]; seq_note_name(g->base, nn); snprintf(val, 24, "%s", nn); }
    else if (row == GR_SPAN) { snprintf(label, 24, "RANGE"); snprintf(val, 24, "%d OCT", g->span); }
    else if (row == GR_EVOLVE) {
        snprintf(label, 24, "EVOLVE");
        if (tr->evo.every) snprintf(val, 24, "EVERY %d BAR%s", tr->evo.every, tr->evo.every > 1 ? "S" : "");
        else snprintf(val, 24, "OFF");
    }
    else if (row == GR_AMOUNT) { snprintf(label, 24, "  AMOUNT"); snprintf(val, 24, "%d%%", tr->evo.amount); }
    else if (row <= GR_EVO_LAST) {
        int k = row - GR_EVO0;
        snprintf(label, 24, "  %s", EVO_NAMES[k]);
        snprintf(val, 24, "%s", tr->evo.kinds & EVO_BITS[k] ? "ON" : "OFF");
    }
    else if (row == GR_MIN) { snprintf(label, 24, "  MIN HITS"); snprintf(val, 24, "%d", tr->evo.min_hits); }
    else if (row == GR_MAX) {
        snprintf(label, 24, "  MAX HITS");
        if (tr->evo.max_hits) snprintf(val, 24, "%d", tr->evo.max_hits); else snprintf(val, 24, "ANY");
    }
    else if (row == GR_ANCHOR) { snprintf(label, 24, "ANCHOR"); snprintf(val, 24, "%s", tr->has_anchor ? "SET (A: RESET)" : "NONE (A: SET)"); }
    else if (row == GR_PULL) { snprintf(label, 24, "  PULL HOME"); snprintf(val, 24, "%d%%", tr->evo.pull); }
    else if (row == GR_CHORD) { snprintf(label, 24, "CHORD"); snprintf(val, 24, "%s", gen_chord_name(tr->chord)); }
    else if (row == GR_SWELL) {
        snprintf(label, 24, "  SWELL");
        if (tr->swell) snprintf(val, 24, "OVER %d BARS", tr->swell); else snprintf(val, 24, "OFF");
    }
    else if (row == GR_SWELL_DEPTH) { snprintf(label, 24, "  SWELL DEPTH"); snprintf(val, 24, "%d%%", tr->swell_depth); }
    else if (row == GR_REST) {
        snprintf(label, 24, "  RESTS");
        if (tr->rest) snprintf(val, 24, "%d%% A BAR", tr->rest); else snprintf(val, 24, "OFF");
    }
    else if (row == GR_XFORM) { snprintf(label, 24, "TRANSFORM"); snprintf(val, 24, "%s (A)", gen_xform_name(u->xf_op)); }
    else if (row == GR_XAMOUNT) {
        snprintf(label, 24, "  BY");
        if (u->xf_op == XF_REVERSE || u->xf_op == XF_INVERT || u->xf_op == XF_CLEAR) snprintf(val, 24, "-");
        else snprintf(val, 24, "%+d", u->xf_amt);
    }
    else if (row == GR_SLOT) {
        snprintf(label, 24, "SLOT");
        snprintf(val, 24, "%d %s", u->slot + 1, tr->slot[u->slot].used ? "(A: LOAD)" : "EMPTY");
    }
    else if (row == GR_STORE) { snprintf(label, 24, "  SAVE TO SLOT"); snprintf(val, 24, "A: SAVE %d", u->slot + 1); }
    else if (row == GR_CHAIN) {
        snprintf(label, 24, "  CHAIN");
        if (tr->chain_len > 1) snprintf(val, 24, "SLOTS 1-%d", tr->chain_len); else snprintf(val, 24, "OFF");
    }
    else if (row == GR_CHAIN_BARS) { snprintf(label, 24, "  BARS PER SLOT"); snprintf(val, 24, "%d", tr->chain_bars); }
    else if (row == GR_MORPH) {
        snprintf(label, 24, "  CHANGE BY");
        if (tr->chain_morph) snprintf(val, 24, "MORPH %d%%", tr->chain_morph); else snprintf(val, 24, "JUMP");
    }
    else if (row == GR_SCALE) { snprintf(label, 24, "SCALE"); snprintf(val, 24, "%s", scale_name(s->scale)); }
    else if (row == GR_ROOT) { static const char *R[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
           snprintf(label, 24, "ROOT"); snprintf(val, 24, "%s", R[s->root % 12]); }
    else { snprintf(label, 24, "  SNAP NOTES"); snprintf(val, 24, "%s", u->snap ? "ON" : "OFF"); }
}

static void gen_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    SeqTrack *tr = &s->track[u->track];
    GenSettings *g = &tr->gen;
    EvolveSettings *e = &tr->evo;
    if (row == GR_TRACK) u->track = wrap(u->track + d, SEQ_TRACKS);
    else if (row == GR_GEN) gen_set_type(s, u->track, wrap(g->type + d, GEN_COUNT));
    else if (row <= GR_P3) {
        int k = row - GR_P0;
        if (k < gen_param_count(g->type))
            g->p[k] = (uint8_t)clampi(g->p[k] + d * (big ? 5 : 1), gen_param_min(g->type, k), gen_param_max(g->type, k));
    }
    else if (row == GR_BASE) g->base = (uint8_t)clampi(g->base + d * (big ? 12 : 1), 0, 115);
    else if (row == GR_SPAN) g->span = (uint8_t)clampi(g->span + d, 1, 4);
    else if (row == GR_EVOLVE) {
        int i = 0;
        while (i < 7 && EVERY[i] < e->every) i++;
        e->every = (uint8_t)EVERY[clampi(i + d, 0, 7)];
    }
    else if (row == GR_AMOUNT) {   /* 1% steps below 10%, for slow drifts; 5% above */
        int fine = !big && (e->amount < 10 || (e->amount == 10 && d < 0));
        e->amount = (uint8_t)clampi(e->amount + d * (fine ? 1 : big ? 10 : 5), 0, 100);
    }
    else if (row == GR_CHORD) tr->chord = (uint8_t)wrap(tr->chord + d, CHORD_COUNT);
    else if (row == GR_SWELL) {
        int i = 0;
        while (i < 5 && SWELL_BARS[i] < tr->swell) i++;
        tr->swell = (uint8_t)SWELL_BARS[clampi(i + d, 0, 5)];
        if (tr->swell && !tr->swell_depth) tr->swell_depth = 30;
    }
    else if (row == GR_SWELL_DEPTH) tr->swell_depth = (uint8_t)clampi(tr->swell_depth + 5 * d * (big ? 2 : 1), 0, 100);
    else if (row == GR_REST) tr->rest = (uint8_t)clampi(tr->rest + 5 * d * (big ? 2 : 1), 0, 100);
    else if (row <= GR_EVO_LAST) e->kinds ^= EVO_BITS[row - GR_EVO0];
    else if (row == GR_MIN) {
        e->min_hits = (uint8_t)clampi(e->min_hits + d * (big ? 4 : 1), 0, SEQ_STEPS);
        if (e->max_hits && e->max_hits < e->min_hits) e->max_hits = e->min_hits;
    }
    else if (row == GR_MAX) {   /* 0 = any; stepping down from 1 gives ANY */
        int v = clampi(e->max_hits + d * (big ? 4 : 1), 0, SEQ_STEPS);
        if (v && v < e->min_hits) v = d < 0 ? 0 : e->min_hits;
        e->max_hits = (uint8_t)v;
    }
    else if (row == GR_ANCHOR) { if (d > 0) gen_set_anchor(s, u->track); else gen_clear_anchor(s, u->track); }
    else if (row == GR_PULL) e->pull = (uint8_t)clampi(e->pull + 5 * d * (big ? 2 : 1), 0, 100);
    else if (row == GR_XFORM) u->xf_op = wrap(u->xf_op + d, XF_COUNT);
    else if (row == GR_XAMOUNT) u->xf_amt = clampi(u->xf_amt + d * (big ? 4 : 1), -16, 16);
    else if (row == GR_SLOT || row == GR_STORE) u->slot = wrap(u->slot + d, SEQ_SLOTS);
    else if (row == GR_CHAIN) {
        int n = clampi((tr->chain_len > 1 ? tr->chain_len : 1) + d, 1, SEQ_SLOTS);
        for (int i = 0; i < SEQ_SLOTS; i++) tr->chain[i] = (uint8_t)i;
        tr->chain_len = (uint8_t)(n > 1 ? n : 0);
        if (tr->chain_pos >= n) tr->chain_pos = 0;
    }
    else if (row == GR_CHAIN_BARS) {
        int i = 0;
        while (i < 4 && CHAIN_BARS[i] != tr->chain_bars) i++;
        tr->chain_bars = (uint8_t)CHAIN_BARS[clampi(i + d, 0, 4)];
    }
    else if (row == GR_MORPH) tr->chain_morph = (uint8_t)clampi(tr->chain_morph + 5 * d * (big ? 2 : 1), 0, 100);
    else if (row == GR_SCALE) seq_set_scale(s, wrap(s->scale + d, scale_count()), s->root, u->snap);
    else if (row == GR_ROOT) seq_set_scale(s, s->scale, wrap(s->root + d, 12), u->snap);
    else u->snap ^= 1;
}

static void say(Ui *u, const char *m) { u->msg = m; u->msg_until = SDL_GetTicks() + 2500; }

#define STEP_ROWS 8
#define TRACK_ROWS 7

static void step_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    const SeqStep *st = &s->track[u->track].step[u->step];
    static const char *L[STEP_ROWS] = {"STEP", "ON", "NOTE", "VELOCITY", "GATE", "HOLD", "PROBABILITY", "LOCK"};
    snprintf(label, 24, "%s", L[row]);
    switch (row) {
    case 0: snprintf(val, 24, "%d", u->step + 1); break;
    case 1: snprintf(val, 24, "%s", st->on ? "ON" : "OFF"); break;
    case 2: { char nn[8]; seq_note_name(st->note, nn); snprintf(val, 24, "%s", nn); break; }
    case 3: snprintf(val, 24, "%d", st->vel); break;
    case 4: snprintf(val, 24, "%d%%", st->gate); break;
    case 5: if (st->hold) snprintf(val, 24, "+%d STEP%s", st->hold, st->hold > 1 ? "S" : ""); else snprintf(val, 24, "OFF"); break;
    case 6: snprintf(val, 24, "%d%%", st->prob); break;
    default: snprintf(val, 24, "%s", st->lock ? "LOCKED" : "OFF"); break;
    }
}

static void step_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    SeqStep *st = &s->track[u->track].step[u->step];
    switch (row) {
    case 0: u->step = wrap(u->step + d, SEQ_STEPS); break;
    case 1: seq_step_toggle(s, u->track, u->step); break;
    case 2:
        if (big) { st->note = (uint8_t)clampi(st->note + 12 * d, 0, 127); st->on = 1; }   /* octave */
        else seq_step_adjust(s, u->track, u->step, d, 0, 0, 0);                            /* scale degree */
        break;
    case 3: seq_step_adjust(s, u->track, u->step, 0, d * (big ? 10 : 1), 0, 0); break;
    case 4: seq_step_adjust(s, u->track, u->step, 0, 0, d * (big ? 25 : 5), 0); break;
    case 5: seq_step_hold(s, u->track, u->step, d * (big ? 4 : 1)); break;
    case 6: seq_step_adjust(s, u->track, u->step, 0, 0, 0, d * (big ? 25 : 5)); break;
    default: st->lock ^= 1; break;
    }
}

static void track_row_text(const Seq *s, const Ui *u, int row, char *label, char *val, const char *midi_desc) {
    const SeqTrack *tr = &s->track[u->track];
    static const char *L[TRACK_ROWS] = {"TRACK", "MIDI CHANNEL", "LENGTH", "MUTE", "BPM", "SWING", "MIDI OUT"};
    snprintf(label, 24, "%s", L[row]);
    switch (row) {
    case 0: snprintf(val, 24, "%d", u->track + 1); break;
    case 1: snprintf(val, 24, "%d", tr->channel + 1); break;
    case 2: snprintf(val, 24, "%d STEPS", tr->len); break;
    case 3: snprintf(val, 24, "%s", tr->mute ? "MUTED" : tr->solo ? "OFF, SOLO" : seq_any_solo(s) ? "OFF, NOT SOLO" : "OFF"); break;
    case 4: snprintf(val, 24, "%d", s->bpm); break;
    case 5: snprintf(val, 24, "%d", s->swing); break;
    default: snprintf(val, 24, "%.14s", midi_desc ? midi_desc : "NONE"); break;
    }
}

static void track_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    switch (row) {
    case 0: u->track = wrap(u->track + d, SEQ_TRACKS); break;
    case 1: seq_track_channel(s, u->track, d); break;
    case 2: seq_track_len(s, u->track, d * (big ? 4 : 1)); break;
    case 3: seq_track_mute_toggle(s, u->track); break;
    case 4: seq_adjust_bpm(s, d * (big ? 10 : 1)); break;
    case 5: seq_adjust_swing(s, d); break;
    default: break;    /* MIDI OUT is information only */
    }
}

/* Main menu, opened with M. Settings that apply to the whole session go here
 * (e.g. a music style). Add a row: bump MENU_ROWS, then handle it in
 * menu_row_text, menu_row_adjust (left/right) and menu_row_select (A). */
#define MENU_ROWS 5
enum { MENU_RESUME, MENU_STYLE, MENU_NEW, MENU_CODES, MENU_QUIT };

static void menu_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    val[0] = 0;
    if (row == MENU_RESUME) snprintf(label, 24, "BACK TO EDITING");
    else if (row == MENU_STYLE) { snprintf(label, 24, "MUSIC STYLE"); snprintf(val, 24, "%s", style_name(s->style)); }
    else if (row == MENU_NEW) {
        snprintf(label, 24, "NEW FROM STYLE");
        snprintf(val, 24, "%s", u->confirm_new ? "A AGAIN: REPLACE" : "A: START OVER");
    }
    else if (row == MENU_CODES) { snprintf(label, 24, "BUTTON CODES"); snprintf(val, 24, "%s", u->show_input ? "ON" : "OFF"); }
    else if (row == MENU_QUIT) snprintf(label, 24, "SAVE AND QUIT");
}

/* Button codes on screen: each press shows its raw code and what it does, so
 * seq4h.input can be fixed on the device without a terminal. */
static void toggle_codes(Ui *u) {
    u->show_input = !u->show_input;
    u->input_msg[0] = 0;
    say(u, u->show_input ? "BUTTON CODES ON: PRESS ANY BUTTON" : "BUTTON CODES OFF");
}

static void menu_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    (void)big;
    if (row == MENU_STYLE) { s->style = wrap(s->style + d, style_count()); u->confirm_new = 0; }
    else if (row == MENU_CODES) toggle_codes(u);
}

static void menu_row_select(Seq *s, Ui *u, int row) {
    if (row == MENU_RESUME) u->page = u->menu_from;
    else if (row == MENU_NEW) {
        /* Replaces all four tracks, so it takes a second press. */
        if (!u->confirm_new) { u->confirm_new = 1; say(u, "PRESS A AGAIN TO REPLACE ALL TRACKS"); return; }
        u->confirm_new = 0;
        u->undo_track = -1;
        int kept = style_apply(s, s->style, 1);
        u->page = P_GRID;
        say(u, kept ? "NEW PATTERNS; YOUR MANUAL TRACKS KEPT" : "NEW PATTERNS FROM STYLE");
    }
    else if (row == MENU_CODES) toggle_codes(u);
    else if (row == MENU_QUIT) u->quit = 1;
}

static int page_rows(int page) {
    return page == P_STEP ? STEP_ROWS : page == P_TRACK ? TRACK_ROWS : page == P_MENU ? MENU_ROWS : GEN_ROWS;
}

/* The grid cursor covers 16 steps, or 32 once any track is longer than 16. */
static int grid_steps(const Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++) if (s->track[t].len > GRID_COLS) return SEQ_STEPS;
    return GRID_COLS;
}

static void remember(Seq *s, Ui *u) {
    memcpy(u->undo, s->track[u->track].step, sizeof u->undo);
    u->undo_len = s->track[u->track].len;
    u->undo_track = u->track;
}

static void roll(Seq *s, Ui *u) {
    if (s->track[u->track].gen.type == GEN_NONE) {
        say(u, "NO GENERATOR: PICK ONE ON GENERATE");
        return;
    }
    remember(s, u);
    gen_roll(s, u->track);
    say(u, "ROLLED");
}

static void undo(Seq *s, Ui *u) {
    if (u->undo_track < 0) { say(u, "NOTHING TO UNDO"); return; }
    memcpy(s->track[u->undo_track].step, u->undo, sizeof u->undo);
    s->track[u->undo_track].len = (uint8_t)u->undo_len;
    u->undo_track = -1;
    say(u, "UNDONE");
}

/* A on the generate page: the row decides what it does; elsewhere it rolls. */
static void gen_action(Seq *s, Ui *u, int row) {
    SeqTrack *tr = &s->track[u->track];
    if (row == GR_ANCHOR) { gen_set_anchor(s, u->track); say(u, "ANCHOR SET"); }
    else if (row == GR_XFORM || row == GR_XAMOUNT) {
        remember(s, u);
        gen_transform(s, u->track, u->xf_op, u->xf_amt);
        say(u, gen_xform_name(u->xf_op));
    }
    else if (row == GR_SLOT) {
        if (!tr->slot[u->slot].used) { say(u, "SLOT IS EMPTY"); return; }
        remember(s, u);
        seq_slot_recall(s, u->track, u->slot);
        say(u, "SLOT LOADED");
    }
    else if (row == GR_STORE) { seq_slot_store(s, u->track, u->slot); say(u, "SAVED TO SLOT"); }
    else if (row == GR_SNAP) {
        remember(s, u);
        seq_set_scale(s, s->scale, s->root, 1);
        say(u, "NOTES SNAPPED TO SCALE");
    }
    else roll(s, u);
}

/* A and B with the grid cursor on a track label. */
static void label_mute(Seq *s, Ui *u) {
    seq_track_mute_toggle(s, u->track);
    snprintf(u->msg_buf, sizeof u->msg_buf, "T%d %s", u->track + 1, s->track[u->track].mute ? "MUTED" : "UNMUTED");
    say(u, u->msg_buf);
}

static void label_solo(Seq *s, Ui *u) {
    seq_track_solo_toggle(s, u->track);
    snprintf(u->msg_buf, sizeof u->msg_buf, "T%d %s", u->track + 1, s->track[u->track].solo ? "SOLO ON" : "SOLO OFF");
    say(u, u->msg_buf);
}

/* Controls (same on every page unless noted):
 *   L1/R1 page   L2/R2 track   B back to grid   Start play/stop   Select+Start quit
 *   M main menu (B or M again closes it)
 *   GRID: d-pad moves the cursor, A step on/off, X edit the step, Y lock the step
 *         left from step 1 puts the cursor on the track label: A mute, B solo,
 *         up/down track, right back to step 1
 *   lists: up/down row, left/right value (hold Y for big steps)
 *          STEP A step on/off; GENERATE A rolls, or does the selected row's action
 *          (set anchor, apply transform, load/save slot, snap notes), X undoes */
static void press(Ui *u, Btn b) {
    Player *p = u->p;
    int dx = (b == B_RIGHT) - (b == B_LEFT), dy = (b == B_DOWN) - (b == B_UP);
    player_lock(p);
    Seq *s = &p->seq;
    if (b == B_QUIT || (b == B_START && u->held[B_SEL])) u->quit = 1;
    else if (b == B_START) { s->playing ? seq_stop(s) : seq_start(s); }
    else if (b == B_MENU) {
        if (u->page == P_MENU) u->page = u->menu_from;
        else { u->menu_from = u->page; u->page = P_MENU; }
    }
    else if (u->page == P_MENU && b == B_B) u->page = u->menu_from;
    else if (u->page == P_MENU && b == B_A) menu_row_select(s, u, u->row[P_MENU]);
    else if (b == B_L || b == B_R) u->page = wrap((u->page == P_MENU ? u->menu_from : u->page) + (b == B_R ? 1 : -1), P_TABS);
    else if (b == B_PAGE) u->page = wrap((u->page == P_MENU ? u->menu_from : u->page) + 1, P_TABS);
    else if (b == B_L2 || b == B_R2) u->track = wrap(u->track + (b == B_R2 ? 1 : -1), SEQ_TRACKS);
    else if (u->page == P_GRID && u->on_label && (b == B_A || b == B_B || b == B_X || b == B_Y || dx || dy)) {
        if (b == B_A) label_mute(s, u);
        else if (b == B_B) label_solo(s, u);
        else if (dx > 0) { u->on_label = 0; u->step = 0; }
        else if (dy) u->track = wrap(u->track + dy, SEQ_TRACKS);
        /* X and Y do nothing here: there is no step under the cursor */
    }
    else if (b == B_ROLL) roll(s, u);
    else if (b == B_UNDO) undo(s, u);
    else if (b == B_LOCK) s->track[u->track].step[u->step].lock ^= 1;
    else if (b == B_B) u->page = P_GRID;
    else if (u->page == P_GRID) {
        if (b == B_A) seq_step_toggle(s, u->track, u->step);
        else if (b == B_X) u->page = P_STEP;
        else if (b == B_Y) s->track[u->track].step[u->step].lock ^= 1;
        else if (dx < 0 && u->step == 0) u->on_label = 1;   /* left of step 1: the track label */
        else if (dx || dy) { u->step = wrap(u->step + dx, grid_steps(s)); u->track = wrap(u->track + dy, SEQ_TRACKS); }
    } else {
        int *row = &u->row[u->page], big = u->held[B_Y];
        if (dy) { *row = wrap(*row + dy, page_rows(u->page)); u->confirm_new = 0; }
        else if (dx && u->page == P_STEP) step_row_adjust(s, u, *row, dx, big);
        else if (dx && u->page == P_TRACK) track_row_adjust(s, u, *row, dx, big);
        else if (dx && u->page == P_MENU) menu_row_adjust(s, u, *row, dx, big);
        else if (dx) gen_row_adjust(s, u, *row, dx, big);
        else if (b == B_A && u->page == P_STEP) seq_step_toggle(s, u->track, u->step);
        else if (b == B_A && u->page == P_GEN) gen_action(s, u, *row);
        else if (b == B_X && u->page == P_GEN) undo(s, u);
    }
    player_unlock(p);
}

static int repeats(Btn b) { return b <= B_RIGHT; }  /* the d-pad auto-repeats */

static void set_button(Ui *u, Btn b, int down) {
    if (down && !u->held[b]) {
        u->held[b] = 1;
        u->next_rep[b] = SDL_GetTicks() + 350;
        press(u, b);
    } else if (!down) {
        u->held[b] = 0;
    }
}

/* One opened joystick: raw (joy set, gc NULL) or SDL game controller (gc set). */
typedef struct {
    SDL_JoystickID id;
    SDL_Joystick *joy;
    SDL_GameController *gc;
    Uint8 hat[4];
    Uint8 axis[IN_JOY_AXES];                  /* bit 0 = negative held, bit 1 = positive held */
    Uint8 pad_axis[SDL_CONTROLLER_AXIS_MAX];
} Dev;

#define MAX_DEVS 8

static Dev *find_dev(Dev *d, SDL_JoystickID id) {
    for (int i = 0; i < MAX_DEVS; i++) if ((d[i].joy || d[i].gc) && d[i].id == id) return &d[i];
    return NULL;
}

static void open_dev(Dev *devs, int index, const InputMap *m, int verbose) {
    Dev *d = NULL;
    for (int i = 0; i < MAX_DEVS && !d; i++) if (!devs[i].joy && !devs[i].gc) d = &devs[i];
    if (!d || find_dev(devs, SDL_JoystickGetDeviceInstanceID(index))) return;
    memset(d, 0, sizeof *d);
    int as_pad = m->mode != INPUT_JOYSTICK && SDL_IsGameController(index);
    if (as_pad) d->gc = SDL_GameControllerOpen(index);
    else if (m->mode != INPUT_CONTROLLER) d->joy = SDL_JoystickOpen(index);
    if (!d->gc && !d->joy) return;
    d->id = SDL_JoystickGetDeviceInstanceID(index);
    if (verbose) {
        char guid[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof guid);
        SDL_Joystick *j = d->gc ? SDL_GameControllerGetJoystick(d->gc) : d->joy;
        fprintf(stderr, "seq4h: input \"%s\" guid %s: %s, %d buttons, %d hats, %d axes\n",
                SDL_JoystickNameForIndex(index), guid, d->gc ? "game controller" : "raw joystick",
                SDL_JoystickNumButtons(j), SDL_JoystickNumHats(j), SDL_JoystickNumAxes(j));
    }
}

static void close_dev(Dev *d) {
    if (d->gc) SDL_GameControllerClose(d->gc);
    else if (d->joy) SDL_JoystickClose(d->joy);
    d->gc = NULL; d->joy = NULL;
}

/* Records what arrived and where it went, for --show-input. */
static void note_input(Ui *u, const char *what, Btn b) {
    if (!u->show_input) return;
    snprintf(u->input_msg, sizeof u->input_msg, "%.28s = %s", what, input_btn_name(b));
    fprintf(stderr, "seq4h: input %s -> %s\n", what, input_btn_name(b));
}

/* Axis held state as a 2-bit mask (bit 0 = negative, bit 1 = positive). */
static Uint8 axis_bits(int v, int thr) { return (Uint8)((v <= -thr) | ((v >= thr) << 1)); }

static void axis_change(Ui *u, Uint8 *state, Uint8 now, const Btn dir[2], const char *label) {
    for (int s = 0; s < 2; s++) {
        int was = (*state >> s) & 1, is = (now >> s) & 1;
        if (was == is) continue;
        if (is) {
            char what[32];
            snprintf(what, sizeof what, "%s%c", label, s ? '+' : '-');
            note_input(u, what, dir[s]);
        }
        if (dir[s] != B_NONE) set_button(u, dir[s], is);
    }
    *state = now;
}

static void handle_input(Ui *u, Dev *devs, const SDL_Event *e, int verbose) {
    const InputMap *m = u->map;
    char what[32];
    Dev *d;
    Btn b;
    switch (e->type) {
    case SDL_KEYDOWN: case SDL_KEYUP:
        if (e->key.repeat) break;
        b = input_key(m, e->key.keysym.sym);
        if (e->type == SDL_KEYDOWN) {
            snprintf(what, sizeof what, "KEY %s", SDL_GetKeyName(e->key.keysym.sym));
            note_input(u, what, b);
        }
        if (b != B_NONE) set_button(u, b, e->type == SDL_KEYDOWN);
        break;
    case SDL_JOYDEVICEADDED:
        open_dev(devs, e->jdevice.which, m, verbose);
        break;
    case SDL_JOYDEVICEREMOVED:
        if ((d = find_dev(devs, e->jdevice.which))) close_dev(d);
        break;
    case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
        if (!(d = find_dev(devs, e->jbutton.which)) || d->gc) break;
        b = e->jbutton.button < IN_JOY_BUTTONS ? m->joy[e->jbutton.button] : B_NONE;
        if (e->type == SDL_JOYBUTTONDOWN) {
            snprintf(what, sizeof what, "JOY %d", e->jbutton.button);
            note_input(u, what, b);
        }
        if (b != B_NONE) set_button(u, b, e->type == SDL_JOYBUTTONDOWN);
        break;
    case SDL_JOYHATMOTION: {
        static const char *HAT[4] = {"UP", "RIGHT", "DOWN", "LEFT"};
        if (!(d = find_dev(devs, e->jhat.which)) || d->gc || e->jhat.hat >= 4) break;
        Uint8 *old = &d->hat[e->jhat.hat], now = e->jhat.value;
        for (int i = 0; i < 4; i++) {           /* SDL_HAT_UP/RIGHT/DOWN/LEFT are bits 0..3 */
            int was = (*old >> i) & 1, is = (now >> i) & 1;
            if (was == is) continue;
            if (is) {
                snprintf(what, sizeof what, "HAT%d %s", e->jhat.hat, HAT[i]);
                note_input(u, what, m->hat[i]);
            }
            if (m->hat[i] != B_NONE) set_button(u, m->hat[i], is);
        }
        *old = now;
        break;
    }
    case SDL_JOYAXISMOTION:
        if (!(d = find_dev(devs, e->jaxis.which)) || d->gc || e->jaxis.axis >= IN_JOY_AXES) break;
        snprintf(what, sizeof what, "AXIS %d", e->jaxis.axis);
        axis_change(u, &d->axis[e->jaxis.axis], axis_bits(e->jaxis.value, m->axis_threshold), m->axis[e->jaxis.axis], what);
        break;
    case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
        if (!(d = find_dev(devs, e->cbutton.which)) || !d->gc || e->cbutton.button >= SDL_CONTROLLER_BUTTON_MAX) break;
        b = m->pad[e->cbutton.button];
        if (e->type == SDL_CONTROLLERBUTTONDOWN) {
            snprintf(what, sizeof what, "PAD %s", SDL_GameControllerGetStringForButton((SDL_GameControllerButton)e->cbutton.button));
            note_input(u, what, b);
        }
        if (b != B_NONE) set_button(u, b, e->type == SDL_CONTROLLERBUTTONDOWN);
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (!(d = find_dev(devs, e->caxis.which)) || !d->gc || e->caxis.axis >= SDL_CONTROLLER_AXIS_MAX) break;
        snprintf(what, sizeof what, "PAD %s", SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)e->caxis.axis));
        axis_change(u, &d->pad_axis[e->caxis.axis], axis_bits(e->caxis.value, m->axis_threshold), m->pad_axis[e->caxis.axis], what);
        break;
    }
}

static void draw_header(SDL_Renderer *r, const Seq *s, const Ui *u, const char *midi_desc) {
    char buf[160];
    text(r, 20, 14, 3, WHITE, "SEQ4H");
    snprintf(buf, sizeof buf, "BPM %d", s->bpm);
    text(r, 150, 18, 2, WHITE, buf);
    snprintf(buf, sizeof buf, "SWING %d", s->swing);
    text(r, 270, 18, 2, DIM, buf);
    {   /* battery, between SWING and play state: amber below 20%, red below 10%, "+" = charging */
        char bt[16];
        battery_text(&u->bat, bt, sizeof bt);
        int lv = battery_level(&u->bat);
        if (bt[0]) text(r, 376, 22, 1, lv == 2 ? (Col){240, 70, 60} : lv == 1 ? (Col){255, 170, 40} : DIM, bt);
    }
    text(r, 440, 18, 2, s->playing ? (Col){90, 220, 120} : DIM, s->playing ? "PLAYING" : "STOPPED");
    {   /* MIDI connection status, top right: green = connected, yellow = waiting, dim = none */
        const char *d = midi_desc ? midi_desc : "NONE";
        int waiting = !strncmp(d, "WAITING", 7), none = !strcmp(d, "NONE");
        Col c = none ? DIM : waiting ? YELLOW : (Col){90, 220, 120};
        text(r, 540, 14, 1, c, none ? "MIDI: NONE" : waiting ? "MIDI: WAITING" : "MIDI: OK");
        if (!none) {
            snprintf(buf, sizeof buf, "%.16s", waiting && strlen(d) > 12 ? d + 12 : d);   /* drop "WAITING FOR " */
            text(r, 540, 26, 1, c, buf);
        }
    }
    for (int i = 0, x = 20; i < P_TABS; x += 6 * ((int)strlen(PAGE_NAMES[i]) + 2), i++)   /* page tabs */
        text(r, x, 46, 1, i == u->page ? YELLOW : DIM, PAGE_NAMES[i]);
    snprintf(buf, sizeof buf, "T%d", u->track + 1);
    text(r, 200, 46, 1, TRACK_COL[u->track], buf);
    static const char *R[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    snprintf(buf, sizeof buf, "%s %s", R[s->root % 12], scale_name(s->scale));
    text(r, 260, 46, 1, DIM, buf);
    if (u->msg && SDL_GetTicks() < u->msg_until) text(r, 400, 46, 1, YELLOW, u->msg);
    if (u->show_input) {   /* e.g. "JOY 10 = START": raw code, then the seq4h button it is mapped to */
        const char *m = u->input_msg[0] ? u->input_msg : "PRESS A BUTTON";
        int x = W - 8 - 12 * (int)strlen(m);
        text(r, x < 320 ? 320 : x, 0, 2, u->input_msg[0] ? YELLOW : DIM, m);
    }
}

static void draw_grid(SDL_Renderer *r, const Seq *sp, const Ui *u) {
    const Seq s = *sp;
    char buf[160];
    int view = (u->step / GRID_COLS) * GRID_COLS;   /* 16 steps on screen; the cursor picks which 16 */
    if (view) {
        snprintf(buf, sizeof buf, "STEPS %d-%d", view + 1, view + GRID_COLS);
        text(r, 560, 46, 1, YELLOW, buf);
    }
    const Col BG = {16, 17, 24}, MUTE_COL = {90, 90, 95};
    for (int t = 0; t < SEQ_TRACKS; t++) {
        const SeqTrack *tr = &s.track[t];
        int y = ROW_Y0 + t * ROW_H;
        /* grey = muted; dimmed = silent because another track is soloed */
        Col base = tr->mute ? MUTE_COL : seq_track_audible(&s, t) ? TRACK_COL[t] : scale(TRACK_COL[t], 40);
        if (u->on_label && t == u->track) {   /* label cursor */
            rect(r, 6, y - 2, 46, ROW_H - 4, (Col){40, 44, 64});
            frame(r, 6, y - 2, 46, ROW_H - 4, 2, YELLOW);
        }
        snprintf(buf, sizeof buf, "T%d", t + 1);
        text(r, 12, y + 4, 2, t == u->track ? YELLOW : base, buf);
        snprintf(buf, sizeof buf, "C%d", tr->channel + 1);
        text(r, 12, y + 24, 1, DIM, buf);
        if (tr->mute) { rect(r, 29, y + 22, 9, 11, (Col){200, 200, 205}); text(r, 31, y + 24, 1, BG, "M"); }   /* badges */
        if (tr->solo) { rect(r, 40, y + 22, 9, 11, YELLOW); text(r, 42, y + 24, 1, BG, "S"); }
        if (tr->mute) text(r, 12, y + 38, 1, DIM, "MUTE");
        else if (tr->solo) text(r, 12, y + 38, 1, YELLOW, "SOLO");
        else text(r, 12, y + 38, 1, scale(base, tr->evo.every ? 100 : 60), gen_short_name(tr->gen.type));   /* bright = evolving */
        int head = seq_playhead(&s, t);
        for (int i = view; i < view + GRID_COLS; i++) {
            int x = X0 + (i - view) * CELL, w = CELL - 4, h = ROW_H - 8;
            if (i >= tr->len) { frame(r, x, y, w, h, 1, (Col){40, 42, 52}); continue; }
            rect(r, x, y, w, h, (i / 4) % 2 ? (Col){38, 40, 52} : (Col){30, 32, 42});
            const SeqStep *st = &tr->step[i];
            if (st->on) {
                rect(r, x + 3, y + 3, w - 6, h - 6, scale(base, 35 + 65 * st->vel / 127));
                if (st->prob < 100) rect(r, x + 3, y + 3, (w - 6) * st->prob / 100, 3, WHITE);
                rect(r, x + 3, y + h - 6, (w - 6) * st->gate / 100, 3, scale(WHITE, 70));
                if (st->hold) {   /* tied over more steps */
                    snprintf(buf, sizeof buf, "+%d", st->hold);
                    text(r, x + 4, y + h - 16, 1, WHITE, buf);
                }
            }
            if (st->lock) rect(r, x + w - 9, y + 2, 6, 6, YELLOW);
            if (i == head) frame(r, x - 1, y - 1, w + 2, h + 2, 2, WHITE);
            if (t == u->track && i == u->step && !u->on_label) frame(r, x - 2, y - 2, w + 4, h + 4, 2, YELLOW);
        }
    }

    const SeqTrack *tr = &s.track[u->track];
    const SeqStep *st = &tr->step[u->step];
    char nn[8];
    seq_note_name(st->note, nn);
    int y = ROW_Y0 + SEQ_TRACKS * ROW_H + 10;
    if (u->on_label) {
        snprintf(buf, sizeof buf, "T%d  MUTE %s  SOLO %s  CH %d", u->track + 1, tr->mute ? "ON" : "OFF", tr->solo ? "ON" : "OFF", tr->channel + 1);
        text(r, 20, y, 2, WHITE, buf);
        if (seq_any_solo(&s)) {
            int n = snprintf(buf, sizeof buf, "SOLO:");
            for (int t = 0; t < SEQ_TRACKS; t++) if (s.track[t].solo) n += snprintf(buf + n, sizeof buf - n, " T%d", t + 1);
            snprintf(buf + n, sizeof buf - n, "  (OTHERS SILENT)");
        } else snprintf(buf, sizeof buf, "NO TRACK SOLOED");
        text(r, 20, y + 22, 2, WHITE, buf);
        snprintf(buf, sizeof buf, "T%d IS %s", u->track + 1, seq_track_audible(&s, u->track) ? "PLAYING" : "SILENT");
        text(r, 20, y + 44, 2, DIM, buf);
        text(r, 20, H - 34, 1, DIM, "UP/DOWN TRACK  A MUTE  B SOLO  RIGHT BACK TO STEP 1  START PLAY/STOP");
        text(r, 20, H - 20, 1, DIM, "L1/R1 PAGE  L2/R2 TRACK  M MENU  SELECT+START QUIT");
        return;
    }
    snprintf(buf, sizeof buf, "T%d STEP %02d %s%s  LEN %d  CH %d", u->track + 1, u->step + 1, st->on ? "ON" : "OFF", st->lock ? " LOCKED" : "", tr->len, tr->channel + 1);
    text(r, 20, y, 2, WHITE, buf);
    snprintf(buf, sizeof buf, "NOTE %s  VEL %d  GATE %d  PROB %d", nn, st->vel, st->gate, st->prob);
    text(r, 20, y + 22, 2, WHITE, buf);
    if (tr->evo.every) snprintf(buf, sizeof buf, "GEN %s  EVOLVE EVERY %d BAR%s %d%%", gen_name(tr->gen.type), tr->evo.every, tr->evo.every > 1 ? "S" : "", tr->evo.amount);
    else snprintf(buf, sizeof buf, "GEN %s  EVOLVE OFF", gen_name(tr->gen.type));
    text(r, 20, y + 44, 2, DIM, buf);

    text(r, 20, H - 34, 1, DIM, "D-PAD MOVE  A STEP ON/OFF  X EDIT STEP  Y LOCK STEP  START PLAY/STOP  LEFT OF STEP 1: MUTE/SOLO");
    text(r, 20, H - 20, 1, DIM, "L1/R1 PAGE  L2/R2 TRACK  M MENU  B BACK TO GRID  SELECT+START QUIT");
}

/* Pattern preview of the current track: height = pitch, brightness = velocity.
 * On the STEP page the step being edited is outlined. */
static void draw_preview(SDL_Renderer *r, const Seq *s, const Ui *u) {
    const SeqTrack *tr = &s->track[u->track];
    Col base = TRACK_COL[u->track];
    int px = 392, py = 70, pw = tr->len > 16 ? 240 / tr->len : 15, ph = 150;
    text(r, px, py - 14, 1, DIM, "PATTERN");
    rect(r, px - 4, py - 2, 16 * pw + 6, ph + 4, (Col){24, 26, 36});
    int lo = tr->gen.base, hi = tr->gen.base + 12 * tr->gen.span;
    for (int i = 0; i < tr->len; i++) {
        const SeqStep *st = &tr->step[i];
        if (st->on) { if (st->note < lo) lo = st->note; if (st->note > hi) hi = st->note; }
    }
    int head = seq_playhead(s, u->track);
    for (int i = 0; i < tr->len; i++) {
        const SeqStep *st = &tr->step[i];
        if (st->on) {
            int yy = py + ph - 8 - (st->note - lo) * (ph - 8) / (hi > lo ? hi - lo : 1);
            rect(r, px + i * pw, yy, pw - 3, 8, scale(base, 35 + 65 * st->vel / 127));
        }
        if (st->lock) rect(r, px + i * pw, py + ph - 2, pw - 3, 2, YELLOW);
        if (i == head) frame(r, px + i * pw - 1, py, pw - 1, ph, 1, WHITE);
        if (u->page == P_STEP && i == u->step) frame(r, px + i * pw - 2, py - 1, pw + 1, ph + 2, 2, YELLOW);
    }
}

static void draw_list(SDL_Renderer *r, const Seq *sp, const Ui *u, const char *midi_desc) {
    const Seq s = *sp;
    char label[24], val[24];
    int n = page_rows(u->page), pitch = n > 12 ? 22 : 30, vis = (H - 40 - 62) / pitch;
    int first = n <= vis ? 0 : clampi(u->row[u->page] - vis / 2, 0, n - vis);   /* scroll long lists */
    if (first > 0) text(r, 360, 52, 1, DIM, "^");
    if (first + vis < n) text(r, 360, 62 + vis * pitch - 8, 1, DIM, "v");
    for (int row = first; row < n && row < first + vis; row++) {
        int y = 62 + (row - first) * pitch, head = 1;
        label[0] = val[0] = 0;
        if (u->page == P_STEP) step_row_text(&s, u, row, label, val);
        else if (u->page == P_MENU) menu_row_text(&s, u, row, label, val);
        else if (u->page == P_TRACK) { track_row_text(&s, u, row, label, val, midi_desc); head = row == 0 || row == 4; }
        else { gen_row_text(&s, u, row, label, val); head = gen_row_is_head(row); }
        int sel = row == u->row[u->page];
        if (sel) rect(r, 14, y - 3, 352, 20, (Col){40, 44, 64});
        text(r, 22, y, 2, sel ? YELLOW : head ? WHITE : DIM, label);
        text(r, 215, y, 2, sel ? YELLOW : WHITE, val);
    }
    if (u->page != P_MENU) draw_preview(r, &s, u);
    else text(r, 392, 62, 2, YELLOW, "MAIN MENU");
    if (u->page == P_GEN) {
        text(r, 392, 240, 1, DIM, "GENERATORS WRITE INTO THE GRID, SO");
        text(r, 392, 252, 1, DIM, "YOU CAN EDIT THE RESULT. LOCKED STEPS");
        text(r, 392, 264, 1, DIM, "(Y ON THE GRID) ARE NEVER CHANGED");
        text(r, 392, 276, 1, DIM, "BY ROLL OR EVOLVE.");
        text(r, 392, 300, 1, DIM, "EVOLVE CHANGES THE PATTERN EVERY N");
        text(r, 392, 312, 1, DIM, "BARS: AMOUNT = CHANCE PER STEP.");
        text(r, 392, 324, 1, DIM, "REGEN RE-RUNS THE GENERATOR.");
    } else if (u->page == P_STEP) {
        text(r, 392, 240, 1, DIM, "NOTE MOVES BY SCALE DEGREE;");
        text(r, 392, 252, 1, DIM, "HOLD Y TO MOVE BY OCTAVE.");
        text(r, 392, 276, 1, DIM, "LOCKED STEPS ARE NEVER CHANGED");
        text(r, 392, 288, 1, DIM, "BY ROLL OR EVOLVE.");
    }
    if (u->page == P_MENU) {
        text(r, 20, H - 34, 1, DIM, "UP/DOWN ROW  LEFT/RIGHT CHANGE  A SELECT");
        text(r, 20, H - 20, 1, DIM, "M OR B CLOSE THE MENU  START PLAY/STOP");
        return;
    }
    text(r, 20, H - 34, 1, DIM, u->page == P_GEN ? "UP/DOWN ROW  LEFT/RIGHT CHANGE (HOLD Y = BIG)  A ROLL OR DO ROW  X UNDO"
                              : u->page == P_STEP ? "UP/DOWN ROW  LEFT/RIGHT CHANGE (HOLD Y = BIG STEP)  A STEP ON/OFF"
                              : "UP/DOWN ROW  LEFT/RIGHT CHANGE (HOLD Y = BIG STEP)");
    text(r, 20, H - 20, 1, DIM, "L1/R1 PAGE  L2/R2 TRACK  M MENU  B BACK TO GRID  START PLAY/STOP");
}

static void draw(SDL_Renderer *r, Player *p, const Ui *u) {
    Seq s;
    char midi_desc[128];
    player_lock(p);
    s = p->seq;
    snprintf(midi_desc, sizeof midi_desc, "%s", p->midi ? p->midi->desc : "NONE");
    player_unlock(p);
    color(r, (Col){16, 17, 24});
    SDL_RenderClear(r);
    draw_header(r, &s, u, midi_desc);
    if (u->page == P_GRID) draw_grid(r, &s, u);
    else draw_list(r, &s, u, midi_desc);
}

/* Battery line in seq4h.log, e.g. "seq4h: 2026-11-19 21:00:05 battery 78% Discharging, 1h00m since start". */
static void log_battery(const Battery *b, Uint32 now, const char *why) {
    char when[32] = "";
    time_t t = time(NULL);
    struct tm tm;
    if (localtime_r(&t, &tm)) strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S ", &tm);
    unsigned mins = now / 60000;
    fprintf(stderr, "seq4h: %sbattery %d%% %s (kernel gauge %d%%), %uh%02um since start%s%s\n", when, b->pct,
            b->status[0] ? b->status : "(no status)", b->kernel_pct, mins / 60, mins % 60, why ? ", " : "", why ? why : "");
    fflush(stderr);
}

/* Every 10 s: read the battery. Log it at start, every hour, when the charger
 * is plugged in or out, and (from ui_run) at quit. */
static void poll_battery(Ui *u, Uint32 now) {
    if (battery_read(&u->bat, NULL, NULL) < 0) return;
    if (!u->bat_logged) {
        battery_log_supplies(stderr, NULL, NULL);
        log_battery(&u->bat, now, "start");
        u->next_bat_log = now + 3600000;
    } else if (u->bat.charging != u->bat_log_charging) {
        log_battery(&u->bat, now, u->bat.charging ? "charger in" : "charger out");
    } else if ((Sint32)(now - u->next_bat_log) >= 0) {
        log_battery(&u->bat, now, NULL);
        u->next_bat_log += 3600000;
    } else return;
    u->bat_logged = 1;
    u->bat_log_charging = u->bat.charging;
}

int ui_run(Player *p, const UiOptions *opt) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    Uint32 wflags = opt->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    SDL_Window *win = SDL_CreateWindow("seq4h", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, wflags);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *r = SDL_CreateRenderer(win, -1, opt->screenshot ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED);
    if (!r) r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!r) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }
    SDL_RenderSetLogicalSize(r, W, H);
    SDL_ShowCursor(SDL_DISABLE);

    InputMap defaults;
    if (!opt->input) input_defaults(&defaults);
    Dev devs[MAX_DEVS] = {0};   /* opened as SDL_JOYDEVICEADDED arrives, which includes devices present at start */
    Ui u = {.p = p, .undo_track = -1, .xf_amt = 1, .snap = 1, .map = opt->input ? opt->input : &defaults, .show_input = opt->show_input,
           .midi_up = -1};
    if (opt->keys) {
        char tmp[256];
        snprintf(tmp, sizeof tmp, "%s", opt->keys);
        for (char *tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
            int down = 1, up = 1;
            if (*tok == '+') { up = 0; tok++; } else if (*tok == '-') { down = 0; tok++; }
            SDL_Keycode k = SDL_GetKeyFromName(tok);
            for (int phase = 0; phase < 2; phase++) {
                if (phase == 0 ? !down : !up) continue;
                SDL_Event ev = {0};
                ev.type = phase == 0 ? SDL_KEYDOWN : SDL_KEYUP;
                ev.key.keysym.sym = k;
                SDL_PushEvent(&ev);
            }
        }
    }
    int frames = 0;
    Uint32 next_poll = 0, next_bat = 0;
    while (!u.quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) u.quit = 1;
            else handle_input(&u, devs, &e, opt->verbose);
        }
        Uint32 now = SDL_GetTicks();
        for (int b = 0; b < B_COUNT; b++)
            if (u.held[b] && repeats((Btn)b) && now >= u.next_rep[b]) {
                u.next_rep[b] = now + 80;
                press(&u, (Btn)b);
            }

        if (now >= next_poll) {   /* MIDI hot-plug: reconnect if unplugged or plugged in late */
            next_poll = now + 1000;
            player_lock(p);
            if (p->midi && p->midi->poll) {
                int up = p->midi->poll(p->midi);
                /* Reconnected: the FM1 may still hold a note whose note-off was
                 * lost while unplugged, and it missed Start. Silence it now and
                 * re-send Start at the next bar line, without jumping back. */
                if (up && u.midi_up == 0) seq_reconnected(&p->seq);
                u.midi_up = up;
            }
            player_unlock(p);
        }
        if (now >= next_bat) {   /* outside the player lock: MIDI timing is never held up */
            next_bat = now + 10000;
            poll_battery(&u, now);
        }
        draw(r, p, &u);
        SDL_RenderPresent(r);

        if (opt->screenshot && ++frames >= 3) {
            SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
            SDL_SaveBMP(sf, opt->screenshot);
            SDL_FreeSurface(sf);
            break;
        }
        SDL_Delay(16);
    }

    if (u.bat_logged && battery_read(&u.bat, NULL, NULL) >= 0) log_battery(&u.bat, SDL_GetTicks(), "quit");
    for (int i = 0; i < MAX_DEVS; i++) close_dev(&devs[i]);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
