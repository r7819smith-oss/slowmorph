#include "ui.h"

#include "battery.h"
#include "drum.h"
#include "gen.h"
#include "input.h"
#include "usbinfo.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
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
               P_MENU = P_TABS, P_SLOTS, P_DRUMMAP, P_MIDI,     /* SLOTS, DRUM MAP and MIDI open from the main menu */
               P_ROLL, P_DRUMS,                                 /* Select on the GRID: piano roll or drum grid */
               P_ALLSTEPS,                                           /* Select on the GRID's track box: ALL STEPS */
               P_COUNT } Page;
static const char *PAGE_NAMES[P_COUNT] = {"GRID", "STEP", "TRACK", "GENERATE", "MENU", "SLOTS", "DRUM MAP", "MIDI", "ROLL", "DRUMS", "ALL STEPS"};
#define MAX_PORTS 8   /* M > MIDI lists this many output ports */
#define MAX_USB 8     /* ...and this many USB devices */
#define MAP_ROWS (SEQ_LANES + 1)   /* DRUM MAP: the 8 lanes, then RESET TO GM */

/* SLOTS page: a second press confirms anything that overwrites or empties a slot. */
enum { CF_NONE, CF_SAVE, CF_COPY, CF_CLEAR };
enum { MORE_COPY, MORE_CLEAR, MORE_BACK, MORE_ROWS };   /* rows of the Select panel */
/* Letters for slot names, in up/down order. */
static const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";

typedef struct {
    Player *p;
    int track, step;
    int on_label;             /* grid cursor is on the track label (left of step 1): A mute, B solo, X roll, Y lock */
    int x_armed;              /* X went down on the track label: its release rolls, holding it undoes */
    Uint32 x_hold_at;         /* ...at this time the hold counts as an undo */
    int held[B_COUNT];
    Uint32 next_rep[B_COUNT];
    int quit;
    int midi_up;              /* last MIDI poll result; -1 = not polled yet */
    int page;                 /* Page */
    int row[P_COUNT];         /* selected row on each list page */
    int menu_from;            /* page to return to when the main menu closes */
    SeqStep undo[SEQ_STEPS];  /* the track as it was before the last roll, transform or slot load */
    SeqDrums undo_drum;       /* ...and its drum lanes */
    int undo_len;
    int undo_track;           /* -1 = nothing to undo */
    int xf_op, xf_amt;        /* transform chosen on the generate page, and its amount */
    int slot;                 /* slot chosen on the generate page */
    int snap;                 /* changing scale moves notes into it */
    int confirm_new;          /* NEW FROM STYLE was pressed once */
    int slot_confirm;         /* SLOTS page: CF_* waiting for a second press */
    int slot_more;            /* SLOTS page: the Select panel (copy / clear) is open */
    int more_row;             /* selected row of that panel (MORE_*) */
    int copy_to;              /* slot to copy into */
    int naming;               /* SLOTS page: the name editor is open */
    char name_buf[SEQ_SLOT_NAME + 1];
    int name_pos;             /* character being edited */
    int lane;                 /* drum grid: lane under the cursor (0 = top) */
    int drum_label;           /* drum grid cursor is on the lane name (left of step 1) */
    int roll_note;            /* piano roll: pitch under the cursor (a scale note) */
    int roll_top;             /* piano roll: note of the top row */
    int sel_used;             /* another button was pressed while Select was held (so its release does nothing) */
    int map_confirm;          /* DRUM MAP: the preset row's A was pressed once */
    int map_preset;           /* DRUM MAP: preset picked on the last row (DRUM_FELUCCA / DRUM_GM) */
    int roll_len;             /* piano roll: length new notes get, hundredths of a step (gate + 100 per held step) */
    int step_back;            /* page B returns to from STEP (the piano roll, if STEP was opened from it), 0 = the grid */
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
    Uint32 save_due;          /* autosave after a button press at this time; 0 = none pending */
    char *saved;              /* contents of the pattern file as last written */
    size_t saved_len;
    int save_failed;          /* the last autosave failed (logged once) */
    const char *midi_file;    /* where the port chosen on M > MIDI is kept */
    char ports[MAX_PORTS][MIDI_PORT_NAME];   /* M > MIDI: output ports, refreshed every second */
    int nports;
    UsbDev usb[MAX_USB];      /* USB devices, refreshed every second */
    int nusb;
    char usb_sig[512];        /* the USB list last written to the log */
} Ui;

/* Generate page rows. */
enum {
    GR_TRACK, GR_GEN, GR_P0, GR_P1, GR_P2, GR_P3, GR_BASE, GR_SPAN,
    GR_EVOLVE, GR_AMOUNT, GR_EVO0, GR_EVO_LAST = GR_EVO0 + 6, GR_MIN, GR_MAX, GR_ANCHOR, GR_PULL,
    GR_XFORM, GR_XAMOUNT, GR_SLOT, GR_STORE, GR_CHAIN, GR_CHAIN_BARS, GR_MORPH,
    GR_SCALE, GR_ROOT, GR_SNAP, GR_CHORD, GR_SWELL, GR_SWELL_DEPTH, GR_REST, GEN_ROWS
};
static const int EVERY[8] = {0, 1, 2, 4, 8, 16, 32, 64};
static const int SWELL_BARS[6] = {0, 2, 4, 8, 16, 32};
static const int CHAIN_BARS[5] = {1, 2, 4, 8, 16};
static const uint8_t EVO_BITS[7] = {EVO_PITCH, EVO_RHYTHM, EVO_VEL, EVO_ROTATE, EVO_REGEN, EVO_PROB, EVO_OFFSET};

static int wrap(int v, int n) { return (v % n + n) % n; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void say(Ui *u, const char *m);
static int is_drums(const Seq *s, int t) { return s->track[t].type == TRACK_DRUMS; }
/* ALL STEPS rows: VELOCITY GATE HOLD OFFSET PROBABILITY STEP LOCKS (drums: VELOCITY PROBABILITY STEP LOCKS) */
static int all_rows(const Seq *s, const Ui *u) { return is_drums(s, u->track) ? 3 : 6; }

static int gen_row_is_head(int row) {
    return row == GR_TRACK || row == GR_GEN || row == GR_EVOLVE || row == GR_ANCHOR || row == GR_CHORD || row == GR_XFORM ||
           row == GR_SLOT || row == GR_SCALE;
}

static void gen_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    int t = u->track;
    const SeqTrack *tr = &s->track[t];
    const GenSettings *g = &tr->gen;
    static const char *EVO_NAMES[7] = {"EVO PITCH", "EVO RHYTHM", "EVO VELOCITY", "EVO ROTATE", "EVO REGEN", "EVO PROB", "EVO OFFSET"};
    label[0] = val[0] = 0;
    if (row == GR_TRACK) { snprintf(label, 24, "TRACK"); snprintf(val, 24, "%d", t + 1); }
    else if (row == GR_GEN) { snprintf(label, 24, "GENERATOR"); snprintf(val, 24, "%s", tr->type == TRACK_DRUMS ? "DRUM KIT" : gen_name(g->type)); }
    else if (row <= GR_P3 && tr->type == TRACK_DRUMS) {   /* the drum generator has one setting */
        snprintf(label, 24, row == GR_P0 ? "  BUSY" : "  -");
        if (row == GR_P0) snprintf(val, 24, "%d%%", tr->drum_busy);
    }
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
        if (u->xf_op == XF_REVERSE || u->xf_op == XF_INVERT || u->xf_op == XF_CLEAR || u->xf_op == XF_GRID) snprintf(val, 24, "-");
        else if (u->xf_op == XF_SPREAD) {   /* range in ticks, shown as a share of a step; below 2 = the most */
            int a = abs(u->xf_amt) < SEQ_OFFGRID_MIN ? SEQ_MAX_NUDGE : abs(u->xf_amt) > SEQ_MAX_NUDGE ? SEQ_MAX_NUDGE : abs(u->xf_amt);
            snprintf(val, 24, "UP TO %d%%", (a * 100 + 12) / SEQ_TICKS_PER_STEP);
        }
        else snprintf(val, 24, "%+d", u->xf_amt);
    }
    else if (row == GR_SLOT) {
        snprintf(label, 24, "SLOT");
        char nm[SEQ_SLOT_NAME + 1];
        seq_slot_name(s, t, u->slot, nm);
        if (tr->slot[u->slot].used) snprintf(val, 24, "%s", nm); else snprintf(val, 24, "%d EMPTY", u->slot + 1);
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
    else if (row == GR_SNAP) { snprintf(label, 24, "  SNAP NOTES"); snprintf(val, 24, "%s", u->snap ? "ON" : "OFF"); }
}

static void gen_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    SeqTrack *tr = &s->track[u->track];
    GenSettings *g = &tr->gen;
    EvolveSettings *e = &tr->evo;
    if (row == GR_TRACK) u->track = wrap(u->track + d, SEQ_TRACKS);
    else if (tr->type == TRACK_DRUMS && row == GR_GEN) say(u, "DRUM TRACK: TYPE IS ON THE TRACK PAGE");
    else if (tr->type == TRACK_DRUMS && row <= GR_P3) {
        if (row == GR_P0) tr->drum_busy = (uint8_t)clampi(tr->drum_busy + 5 * d * (big ? 2 : 1), 0, 100);
    }
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
    else if (row == GR_SNAP) u->snap ^= 1;
}

static void say(Ui *u, const char *m) { u->msg = m; u->msg_until = SDL_GetTicks() + 2500; }

/* The track the DRUM MAP plays its notes on: the current track if it is a
 * drum track, else the first drum track, else the current track. */
static int audition_track(const Seq *s, const Ui *u) {
    if (is_drums(s, u->track)) return u->track;
    for (int t = 0; t < SEQ_TRACKS; t++) if (is_drums(s, t)) return t;
    return u->track;
}

#define STEP_ROWS 9
#define TRACK_ROWS 10
#define TRACK_BPM_ROW 7

static void step_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    const SeqStep *st = &s->track[u->track].step[u->step];
    if (is_drums(s, u->track) && row > 0) {   /* a drum step: one row per lane */
        const SeqDrums *d = &s->track[u->track].drum;
        int l = row - 1;
        snprintf(label, 24, "%s", s->lane_name[l]);
        if (d->vel[l][u->step]) snprintf(val, 24, "VEL %d  %d%%", d->vel[l][u->step], d->prob[l][u->step]);
        else snprintf(val, 24, "-");
        return;
    }
    static const char *L[STEP_ROWS] = {"STEP", "ON", "NOTE", "VELOCITY", "GATE", "HOLD", "OFFSET", "PROBABILITY", "LOCK"};
    snprintf(label, 24, "%s", L[row]);
    switch (row) {
    case 0: snprintf(val, 24, "%d", u->step + 1); break;
    case 1: snprintf(val, 24, "%s", st->on ? "ON" : "OFF"); break;
    case 2: { char nn[8]; seq_note_name(st->note, nn); snprintf(val, 24, "%s", nn); break; }
    case 3: snprintf(val, 24, "%d", st->vel); break;
    case 4: snprintf(val, 24, "%d%%", st->gate); break;
    case 5: if (st->hold) snprintf(val, 24, "+%d STEP%s", st->hold, st->hold > 1 ? "S" : ""); else snprintf(val, 24, "OFF"); break;
    case 6:   /* shown as a share of a step: 1 tick = 1/24 step */
        if (st->nudge) snprintf(val, 24, "%s %d%%", st->nudge < 0 ? "EARLY" : "LATE", (abs(st->nudge) * 100 + 12) / SEQ_TICKS_PER_STEP);
        else snprintf(val, 24, "ON GRID");
        break;
    case 7: snprintf(val, 24, "%d%%", st->prob); break;
    default: snprintf(val, 24, "%s", st->lock ? "LOCKED" : "OFF"); break;
    }
}

static void step_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    SeqStep *st = &s->track[u->track].step[u->step];
    if (is_drums(s, u->track) && row > 0) {   /* left/right velocity; below 1 the hit goes */
        uint8_t *v = &s->track[u->track].drum.vel[row - 1][u->step];
        if (!*v) { if (d > 0) seq_drum_toggle(s, u->track, row - 1, u->step); }
        else if (*v + d * (big ? 25 : 5) < 1) *v = 0;
        else seq_drum_adjust(s, u->track, row - 1, u->step, d * (big ? 25 : 5), 0);
        return;
    }
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
    case 6: seq_step_nudge(s, u->track, u->step, d * (big ? 4 : 1)); break;
    case 7: seq_step_adjust(s, u->track, u->step, 0, 0, 0, d * (big ? 25 : 5)); break;
    default: st->lock ^= 1; break;
    }
}

static void track_row_text(const Seq *s, const Ui *u, int row, char *label, char *val, const char *midi_desc) {
    const SeqTrack *tr = &s->track[u->track];
    static const char *L[TRACK_ROWS] = {"TRACK", "TYPE", "MIDI CHANNEL", "LENGTH", "MUTE", "SHUFFLE", "NEVER ON GRID", "BPM", "SWING", "MIDI OUT"};
    snprintf(label, 24, "%s", L[row]);
    if (row >= 2) row--;   /* the rows below TYPE keep their old numbers */
    else if (row == 1) { snprintf(val, 24, "%s", tr->type == TRACK_DRUMS ? "DRUMS (8 LANES)" : "NOTES"); return; }
    switch (row) {
    case 0: snprintf(val, 24, "%d", u->track + 1); break;
    case 1: snprintf(val, 24, "%d", tr->channel + 1); break;
    case 2: snprintf(val, 24, "%d STEPS", tr->len); break;
    case 3: snprintf(val, 24, "%s", tr->mute ? "MUTED" : tr->solo ? "OFF, SOLO" : seq_any_solo(s) ? "OFF, NOT SOLO" : "OFF"); break;
    case 4:   /* live shuffle range, as a share of a step */
        if (tr->shuffle) snprintf(val, 24, "UP TO %d%%", (tr->shuffle * 100 + 12) / SEQ_TICKS_PER_STEP);
        else snprintf(val, 24, "OFF");
        break;
    case 5: snprintf(val, 24, "%s", tr->offgrid ? "ON" : "OFF"); break;
    case 6: snprintf(val, 24, "%d", s->bpm); break;
    case 7: snprintf(val, 24, "%d", s->swing); break;
    default: snprintf(val, 24, "%.14s", midi_desc ? midi_desc : "NONE"); break;
    }
}

static void track_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    if (row == 1) {
        seq_track_type(s, u->track, is_drums(s, u->track) ? TRACK_NOTES : TRACK_DRUMS);
        say(u, is_drums(s, u->track) ? "DRUMS: SELECT ON THE GRID OPENS THE LANES" : "NOTES: SELECT ON THE GRID OPENS THE ROLL");
        return;
    }
    if (row >= 2) row--;
    switch (row) {
    case 0: u->track = wrap(u->track + d, SEQ_TRACKS); break;
    case 1: seq_track_channel(s, u->track, d); break;
    case 2: seq_track_len(s, u->track, d * (big ? 4 : 1)); break;
    case 3: seq_track_mute_toggle(s, u->track); break;
    case 4: seq_track_shuffle(s, u->track, d * (big ? 4 : 1)); break;
    case 5: seq_track_offgrid(s, u->track, !s->track[u->track].offgrid); break;
    case 6: seq_adjust_bpm(s, d * (big ? 10 : 1)); break;
    case 7: seq_adjust_swing(s, d); break;
    default: break;    /* MIDI OUT is information only */
    }
}

/* Main menu, opened with M. Settings that apply to the whole session go here
 * (e.g. a music style). Add a row: bump MENU_ROWS, then handle it in
 * menu_row_text, menu_row_adjust (left/right) and menu_row_select (A). */
#define MENU_ROWS 8
enum { MENU_RESUME, MENU_STYLE, MENU_NEW, MENU_SLOTS, MENU_DRUMMAP, MENU_MIDI, MENU_CODES, MENU_QUIT };

static void menu_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    val[0] = 0;
    if (row == MENU_RESUME) snprintf(label, 24, "BACK TO EDITING");
    else if (row == MENU_STYLE) { snprintf(label, 24, "MUSIC STYLE"); snprintf(val, 24, "%s", style_name(s->style)); }
    else if (row == MENU_NEW) {
        snprintf(label, 24, "NEW FROM STYLE");
        snprintf(val, 24, "%s", u->confirm_new ? "A AGAIN: REPLACE" : "A: START OVER");
    }
    else if (row == MENU_SLOTS) { snprintf(label, 24, "SLOTS"); snprintf(val, 24, "A: OPEN"); }
    else if (row == MENU_DRUMMAP) { snprintf(label, 24, "DRUM MAP"); snprintf(val, 24, "A: OPEN"); }
    else if (row == MENU_MIDI) { snprintf(label, 24, "MIDI OUTPUT"); snprintf(val, 24, "A: OPEN"); }
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
        say(u, s->style == STYLE_BLANK ? "BLANK: FOUR EMPTY MANUAL TRACKS"
               : kept ? "NEW PATTERNS; YOUR MANUAL TRACKS KEPT" : "NEW PATTERNS FROM STYLE");
    }
    else if (row == MENU_SLOTS) {
        u->page = P_SLOTS;
        u->row[P_SLOTS] = s->track[u->track].cur_slot;
        u->slot_confirm = u->slot_more = u->naming = 0;
    }
    else if (row == MENU_DRUMMAP) {
        u->page = P_DRUMMAP;
        u->naming = u->map_confirm = 0;
    }
    else if (row == MENU_MIDI) {
        u->page = P_MIDI;
        u->row[P_MIDI] = 0;
    }
    else if (row == MENU_CODES) toggle_codes(u);
    else if (row == MENU_QUIT) u->quit = 1;
}

static int page_rows(const Ui *u, int page) {
    if (page == P_ALLSTEPS) return all_rows(&u->p->seq, u);
    return page == P_STEP ? STEP_ROWS : page == P_TRACK ? TRACK_ROWS : page == P_MENU ? MENU_ROWS
         : page == P_SLOTS ? SEQ_SLOTS : page == P_DRUMMAP ? MAP_ROWS : page == P_MIDI ? 1 + u->nports : GEN_ROWS;
}

/* The grid cursor covers 16 steps, or 32 once any track is longer than 16. */
static int grid_steps(const Seq *s) {
    for (int t = 0; t < SEQ_TRACKS; t++) if (s->track[t].len > GRID_COLS) return SEQ_STEPS;
    return GRID_COLS;
}

static void remember(Seq *s, Ui *u) {
    memcpy(u->undo, s->track[u->track].step, sizeof u->undo);
    u->undo_drum = s->track[u->track].drum;
    u->undo_len = s->track[u->track].len;
    u->undo_track = u->track;
}

static void say_locked(Ui *u) {
    snprintf(u->msg_buf, sizeof u->msg_buf, "T%d IS LOCKED: Y ON ITS TRACK BOX", u->track + 1);
    say(u, u->msg_buf);
}

static void roll(Seq *s, Ui *u) {
    if (s->track[u->track].locked) { say_locked(u); return; }
    if (!is_drums(s, u->track) && s->track[u->track].gen.type == GEN_NONE) {
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
    s->track[u->undo_track].drum = u->undo_drum;
    s->track[u->undo_track].len = (uint8_t)u->undo_len;
    u->undo_track = -1;
    say(u, "UNDONE");
}

/* A on the generate page: the row decides what it does; elsewhere it rolls. */
static void gen_action(Seq *s, Ui *u, int row) {
    SeqTrack *tr = &s->track[u->track];
    if (row == GR_ANCHOR) { gen_set_anchor(s, u->track); say(u, "ANCHOR SET"); }
    else if (row == GR_XFORM || row == GR_XAMOUNT) {
        if (tr->locked) { say_locked(u); return; }
        if (is_drums(s, u->track) && (u->xf_op == XF_INVERT || u->xf_op == XF_TRANSPOSE || u->xf_op == XF_SPREAD || u->xf_op == XF_GRID)) {
            say(u, "NOT FOR DRUM TRACKS");
            return;
        }
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

/* SLOTS page (M > SLOTS): the current track's 8 slots. A loads, X saves the
 * live pattern into the slot, Y renames it, Select opens copy / clear. */
/* X on a track label: tap rolls the track (on release), hold undoes. */
#define HOLD_UNDO_MS 500
static void label_x_down(Ui *u) {
    u->x_armed = 1;
    u->x_hold_at = SDL_GetTicks() + HOLD_UNDO_MS;
}

/* X released before the hold time: roll. Called without the player lock. */
static void label_x_release(Ui *u) {
    if (!u->x_armed) return;
    u->x_armed = 0;
    player_lock(u->p);
    Seq *s = &u->p->seq;
    if (u->page == P_GRID && u->on_label) {
        roll(s, u);
        if (u->msg && !strcmp(u->msg, "ROLLED")) {
            snprintf(u->msg_buf, sizeof u->msg_buf, "T%d ROLLED (HOLD X TO UNDO)", u->track + 1);
            say(u, u->msg_buf);
        }
    }
    player_unlock(u->p);
}

/* X still held after HOLD_UNDO_MS: undo instead of rolling. */
static void label_x_hold(Ui *u, Uint32 now) {
    if (!u->x_armed || !u->held[B_X] || (Sint32)(now - u->x_hold_at) < 0) return;
    if (u->page != P_GRID || !u->on_label) { u->x_armed = 0; return; }
    u->x_armed = 0;
    player_lock(u->p);
    undo(&u->p->seq, u);
    player_unlock(u->p);
    if (!u->save_due) u->save_due = now + 2000;
}

static void label_lock(Seq *s, Ui *u) {
    seq_track_lock_toggle(s, u->track);
    snprintf(u->msg_buf, sizeof u->msg_buf, "T%d %s", u->track + 1, s->track[u->track].locked ? "LOCKED: ROLL AND EVOLVE SKIP IT" : "UNLOCKED");
    say(u, u->msg_buf);
}

/* ---- ALL STEPS (Select on the track label): one setting for every step ---- */
#define ALL_LOCKS ALL_KINDS   /* the last row: lock / unlock every step */
static const int ALL_NOTE_ROWS[] = {ALL_VEL, ALL_GATE, ALL_HOLD, ALL_NUDGE, ALL_PROB, ALL_LOCKS};
static const int ALL_DRUM_ROWS[] = {ALL_VEL, ALL_PROB, ALL_LOCKS};
static int all_what(const Seq *s, const Ui *u, int row) {
    int n = all_rows(s, u);
    row = clampi(row, 0, n - 1);
    return is_drums(s, u->track) ? ALL_DRUM_ROWS[row] : ALL_NOTE_ROWS[row];
}

static void all_value(int what, int v, char *out, int n) {
    if (what == ALL_GATE) snprintf(out, (size_t)n, "%d%%", v);
    else if (what == ALL_PROB) snprintf(out, (size_t)n, "%d%%", v);
    else if (what == ALL_HOLD) snprintf(out, (size_t)n, "+%d", v);
    else if (what == ALL_NUDGE) {   /* as a share of a step, like the STEP page */
        int pct = (abs(v) * 100 + 12) / SEQ_TICKS_PER_STEP;
        snprintf(out, (size_t)n, "%s%d%%", v < 0 ? "-" : v > 0 ? "+" : "", pct);
    }
    else snprintf(out, (size_t)n, "%d", v);
}

static const char *all_name(int what) {
    static const char *N[ALL_KINDS + 1] = {"VELOCITY", "GATE", "HOLD", "OFFSET", "PROBABILITY", "STEP LOCKS"};
    return N[clampi(what, 0, ALL_KINDS)];
}

static void all_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    int what = all_what(s, u, row), lo, hi, avg;
    snprintf(label, 24, "%s", all_name(what));
    if (what == ALL_LOCKS) {
        int n = seq_locked_steps(s, u->track);
        if (n) snprintf(val, 24, "%d LOCKED", n);
        else snprintf(val, 24, "NONE");
        return;
    }
    if (!seq_all_range(s, u->track, what, &lo, &hi, &avg)) { snprintf(val, 24, is_drums(s, u->track) ? "NO HITS" : "NO NOTES"); return; }
    char a[12], b[12];
    all_value(what, lo, a, sizeof a);
    all_value(what, hi, b, sizeof b);
    if (lo == hi) snprintf(val, 24, "ALL %.12s", a);
    else snprintf(val, 24, "%.8s TO %.8s", a, b);
}

static void all_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    int what = all_what(s, u, row);
    static const int SMALL[ALL_KINDS] = {1, 5, 1, 1, 5}, BIG[ALL_KINDS] = {10, 25, 4, 4, 25};
    if (what == ALL_LOCKS) return;
    seq_all_adjust(s, u->track, what, d * (big ? BIG[what] : SMALL[what]));
}

/* A: every step gets the average (locks: lock all, or unlock all). */
static void all_row_action(Seq *s, Ui *u, int row) {
    int what = all_what(s, u, row), lo, hi, avg;
    char v[12];
    if (what == ALL_LOCKS) {
        int now = seq_all_lock_toggle(s, u->track);
        snprintf(u->msg_buf, sizeof u->msg_buf, "T%d %s", u->track + 1, now ? "EVERY STEP LOCKED" : "STEP LOCKS CLEARED");
        say(u, u->msg_buf);
        return;
    }
    if (!seq_all_range(s, u->track, what, &lo, &hi, &avg)) { say(u, is_drums(s, u->track) ? "NO HITS" : "NO NOTES"); return; }
    seq_all_set(s, u->track, what, avg);
    all_value(what, avg, v, sizeof v);
    snprintf(u->msg_buf, sizeof u->msg_buf, "%s: ALL %s", all_name(what), v);
    say(u, u->msg_buf);
}

static void open_all(Ui *u) {
    u->page = P_ALLSTEPS;
    u->on_label = 0;
    u->row[P_ALLSTEPS] = 0;
}

static void slots_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    const SeqTrack *tr = &s->track[u->track];
    char nm[SEQ_SLOT_NAME + 1];
    seq_slot_name(s, u->track, row, nm);
    snprintf(label, 24, "%d %s", row + 1, nm);
    if (tr->slot[row].used) snprintf(val, 24, "%d STEPS%s", tr->slot[row].len, row == tr->cur_slot ? " <" : "");
    else snprintf(val, 24, "EMPTY");
}

static void name_slot_start(const Seq *s, Ui *u, int k) {
    char nm[SEQ_SLOT_NAME + 1];
    seq_slot_name(s, u->track, k, nm);
    snprintf(u->name_buf, sizeof u->name_buf, "%-*s", SEQ_SLOT_NAME, nm);   /* pad with spaces to 8 */
    u->name_pos = 0;
    u->naming = 1;
}

/* The name editor: up/down letter, left/right move, X space, A keep, B cancel. */
static void name_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    int k = u->row[u->page];
    char *c = &u->name_buf[u->name_pos];
    if (dy) {
        const char *at = strchr(NAME_CHARS, *c);
        int i = at && *c ? (int)(at - NAME_CHARS) : 0, n = (int)strlen(NAME_CHARS);
        *c = NAME_CHARS[wrap(i - dy, n)];   /* up = next letter */
    }
    else if (dx) u->name_pos = clampi(u->name_pos + dx, 0, SEQ_SLOT_NAME - 1);
    else if (b == B_X) *c = ' ';
    else if (b == B_A && u->page == P_DRUMMAP) {   /* naming a drum lane */
        seq_lane_set_name(s, k, u->name_buf);
        u->naming = 0;
        snprintf(u->msg_buf, sizeof u->msg_buf, "LANE %d IS %s", k + 1, s->lane_name[k]);
        say(u, u->msg_buf);
    }
    else if (b == B_A) {
        seq_slot_set_name(s, u->track, k, u->name_buf);
        u->naming = 0;
        char nm[SEQ_SLOT_NAME + 1];
        seq_slot_name(s, u->track, k, nm);
        snprintf(u->msg_buf, sizeof u->msg_buf, "NAMED %s", nm);
        say(u, u->msg_buf);
    }
    else if (b == B_B) { u->naming = 0; say(u, "NAME NOT CHANGED"); }
}

static void slot_say(Ui *u, const char *what, const Seq *s, int k) {
    char nm[SEQ_SLOT_NAME + 1];
    seq_slot_name(s, u->track, k, nm);
    snprintf(u->msg_buf, sizeof u->msg_buf, "%s %s", what, nm);
    say(u, u->msg_buf);
}

static void slots_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    SeqTrack *tr = &s->track[u->track];
    int *row = &u->row[P_SLOTS], k = *row;
    int confirm = u->slot_confirm;
    u->slot_confirm = CF_NONE;   /* any other press cancels a pending confirm */
    if (u->naming) { name_press(s, u, b, dx, dy); return; }
    if (u->slot_more) {
        if (dy) u->more_row = wrap(u->more_row + dy, MORE_ROWS);
        else if (dx && u->more_row == MORE_COPY) u->copy_to = wrap(u->copy_to + dx, SEQ_SLOTS);
        else if (b == B_B || b == B_SEL || (b == B_A && u->more_row == MORE_BACK)) u->slot_more = 0;
        else if (b == B_A && u->more_row == MORE_COPY) {
            if (!tr->slot[k].used) { say(u, "SLOT IS EMPTY: NOTHING TO COPY"); return; }
            if (u->copy_to == k) { say(u, "PICK ANOTHER SLOT TO COPY TO"); return; }
            if (tr->slot[u->copy_to].used && confirm != CF_COPY) {
                u->slot_confirm = CF_COPY;
                snprintf(u->msg_buf, sizeof u->msg_buf, "A AGAIN: REPLACE SLOT %d", u->copy_to + 1);
                say(u, u->msg_buf);
                return;
            }
            seq_slot_copy(s, u->track, k, u->copy_to);
            snprintf(u->msg_buf, sizeof u->msg_buf, "COPIED TO SLOT %d", u->copy_to + 1);
            say(u, u->msg_buf);
            *row = u->copy_to;
            u->slot_more = 0;
        }
        else if (b == B_A && u->more_row == MORE_CLEAR) {
            if (!tr->slot[k].used && !tr->slot[k].name[0]) { say(u, "SLOT IS ALREADY EMPTY"); return; }
            if (confirm != CF_CLEAR) { u->slot_confirm = CF_CLEAR; say(u, "A AGAIN: CLEAR THIS SLOT"); return; }
            seq_slot_clear(s, u->track, k);
            snprintf(u->msg_buf, sizeof u->msg_buf, "SLOT %d CLEARED", k + 1);
            say(u, u->msg_buf);
            u->slot_more = 0;
        }
        return;
    }
    if (dy) *row = wrap(*row + dy, SEQ_SLOTS);
    else if (b == B_B) u->page = P_MENU;
    else if (b == B_A) {
        if (!tr->slot[k].used) { say(u, "SLOT IS EMPTY"); return; }
        remember(s, u);                  /* X on GENERATE undoes a load */
        seq_slot_recall(s, u->track, k);
        slot_say(u, "LOADED", s, k);
    }
    else if (b == B_X) {
        if (tr->slot[k].used && confirm != CF_SAVE) { u->slot_confirm = CF_SAVE; say(u, "X AGAIN: REPLACE THIS SLOT"); return; }
        seq_slot_store(s, u->track, k);
        slot_say(u, "SAVED TO", s, k);
    }
    else if (b == B_Y) name_slot_start(s, u, k);
    else if (b == B_SEL) { u->slot_more = 1; u->more_row = MORE_COPY; u->copy_to = (k + 1) % SEQ_SLOTS; }
}

/* ---- piano roll (Select on the GRID, note tracks) ----
 * One track full screen: steps across, the scale's notes down the side, 14
 * rows at a time. A track still plays one note per step, so each column holds
 * at most one note (chord notes are drawn faintly; CHORD sets them). */
#define ROLL_ROWS 14
#define ROLL_RH 24
#define ROLL_Y0 62

static int roll_top_max(const Seq *s) { return gen_degree_note(s, gen_note_degree(s, 127)); }

/* Scroll so the cursor's row is on screen. */
static void roll_follow(const Seq *s, Ui *u) {
    u->roll_note = gen_snap_note(s, u->roll_note);
    u->roll_top = gen_snap_note(s, u->roll_top);
    int dn = gen_note_degree(s, u->roll_note), dt = gen_note_degree(s, u->roll_top);
    if (dn > dt) dt = dn;
    if (dn < dt - (ROLL_ROWS - 1)) dt = dn + ROLL_ROWS - 1;
    if (dt < ROLL_ROWS - 1) dt = ROLL_ROWS - 1;   /* the bottom row is never below the lowest note */
    u->roll_top = gen_degree_note(s, dt);
    if (u->roll_top > roll_top_max(s)) u->roll_top = roll_top_max(s);
}

static void open_roll(const Seq *s, Ui *u) {
    const SeqTrack *tr = &s->track[u->track];
    const SeqStep *st = &tr->step[u->step];
    u->page = P_ROLL;
    u->on_label = 0;
    if (u->step >= tr->len) u->step = 0;
    u->roll_note = gen_snap_note(s, st->on ? st->note : tr->gen.base + 12);
    u->roll_top = gen_degree_note(s, gen_note_degree(s, u->roll_note) + ROLL_ROWS / 2);   /* the cursor in the middle */
    roll_follow(s, u);
}

static void open_drums(const Seq *s, Ui *u) {
    u->page = P_DRUMS;
    u->on_label = u->drum_label = 0;
    if (u->step >= s->track[u->track].len) u->step = 0;
}

/* After L2/R2: the roll or the drum grid, whichever fits the new track. */
static void sub_page_for_track(const Seq *s, Ui *u) {
    if (u->page != P_ROLL && u->page != P_DRUMS) return;
    if (is_drums(s, u->track)) open_drums(s, u);
    else open_roll(s, u);
}

/* Length as hundredths of a step: gate (5..100) plus 100 per held step.
 * Up to a step it moves in quarters, then a whole step at a time. */
static int length_step(int L, int d) {
    if (d > 0) L = L < 100 ? (L / 25 + 1) * 25 : L + 100;
    else L = L > 100 ? L - 100 : L > 25 ? ((L + 24) / 25 - 1) * 25 : 5;
    return clampi(L, 5, 100 + 100 * SEQ_MAX_HOLD);
}

static void set_length(SeqStep *st, int L) {
    st->hold = (uint8_t)(L > 100 ? (L - 1) / 100 : 0);
    st->gate = (uint8_t)(L - st->hold * 100);
}

static int step_length(const SeqStep *st) { return st->hold * 100 + st->gate; }

static void roll_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    SeqTrack *tr = &s->track[u->track];
    SeqStep *st = &tr->step[u->step];
    roll_follow(s, u);
    if (u->held[B_Y] && dx) {   /* length: of the note here, and of the notes A writes from now on */
        u->roll_len = length_step(st->on ? step_length(st) : u->roll_len, dx);
        if (st->on) set_length(st, u->roll_len);
    }
    else if (u->held[B_Y] && dy) {   /* move the step's note by a scale degree; the cursor goes with it */
        if (!st->on) { say(u, "NO NOTE ON THIS STEP"); return; }
        st->note = (uint8_t)gen_step_note(s, gen_snap_note(s, st->note), -dy);
        u->roll_note = st->note;
        if (!s->playing) seq_audition(s, tr->channel, st->note, st->vel);
    }
    else if (u->held[B_X] && dx) { if (st->on) seq_step_nudge(s, u->track, u->step, dx); else say(u, "NO NOTE ON THIS STEP"); }
    else if (u->held[B_X] && dy) { if (st->on) seq_step_adjust(s, u->track, u->step, 0, -dy * 8, 0, 0); else say(u, "NO NOTE ON THIS STEP"); }
    else if (dx) u->step = wrap(u->step + dx, tr->len);
    else if (dy) u->roll_note = gen_step_note(s, u->roll_note, -dy);
    else if (b == B_A) {
        if (st->on && st->note == u->roll_note) st->on = 0;
        else {
            if (!st->on) set_length(st, u->roll_len);   /* a new note gets the length last set */
            st->note = (uint8_t)u->roll_note;
            st->on = 1;
            if (!s->playing) seq_audition(s, tr->channel, st->note, st->vel);
        }
    }
    roll_follow(s, u);
}

/* ---- drum grid (Select on the GRID, drum tracks) ---- */
#define DRUM_RH 40
#define DRUM_Y0 62

static void drums_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    SeqTrack *tr = &s->track[u->track];
    int l = u->lane, i = u->step;
    if (u->drum_label) {   /* on the lane name: A mute, B solo, X hear it, Y lock it */
        if (dy) u->lane = wrap(u->lane + dy, SEQ_LANES);
        else if (dx > 0) { u->drum_label = 0; u->step = 0; }
        else if (b == B_A) {
            seq_lane_mute_toggle(s, u->track, l);
            snprintf(u->msg_buf, sizeof u->msg_buf, "%s %s", s->lane_name[l], tr->drum_mute & (1 << l) ? "MUTED" : "UNMUTED");
            say(u, u->msg_buf);
        }
        else if (b == B_B) {
            seq_lane_solo_toggle(s, u->track, l);
            snprintf(u->msg_buf, sizeof u->msg_buf, "%s %s", s->lane_name[l], tr->drum_solo & (1 << l) ? "SOLO ON" : "SOLO OFF");
            say(u, u->msg_buf);
        }
        else if (b == B_X) seq_audition(s, tr->channel, s->lane_note[l], 100);
        else if (b == B_Y || b == B_LOCK) seq_drum_lock(s, u->track, l);
        return;
    }
    if (u->held[B_X] && dy) seq_drum_adjust(s, u->track, l, i, -dy * 10, 0);
    else if (u->held[B_X] && dx) seq_drum_adjust(s, u->track, l, i, 0, dx * 10);
    else if (dx < 0 && i == 0) u->drum_label = 1;
    else if (dx) u->step = wrap(u->step + dx, tr->len);
    else if (dy) u->lane = wrap(u->lane + dy, SEQ_LANES);
    else if (b == B_A) {
        seq_drum_toggle(s, u->track, l, i);
        if (tr->drum.vel[l][i] && !s->playing) seq_audition(s, tr->channel, s->lane_note[l], tr->drum.vel[l][i]);
    }
    else if (b == B_Y || b == B_LOCK) {
        seq_drum_lock(s, u->track, l);
        snprintf(u->msg_buf, sizeof u->msg_buf, "%s %s", s->lane_name[l], tr->drum_lock & (1 << l) ? "LOCKED" : "UNLOCKED");
        say(u, u->msg_buf);
    }
}

/* ---- DRUM MAP (M > DRUM MAP): the note and name of each lane ---- */
/* A lane's value: its note and the sound Felucca's DRUM engine plays for it
 * ("41 TOM -7": a tom, 7 semitones below the tom's own pitch). */
static void map_row_text(const Seq *s, const Ui *u, int row, char *label, char *val) {
    if (row == SEQ_LANES) {
        snprintf(label, 24, "PRESET");
        if (u->map_confirm) snprintf(val, 24, "A AGAIN: %s", seq_drum_preset_name(u->map_preset));
        else snprintf(val, 24, "%s%s", seq_drum_preset_name(u->map_preset), seq_drum_map_is(s, u->map_preset) ? " (IN USE)" : " (A)");
        return;
    }
    int st;
    const char *snd = seq_felucca_sound(s->lane_note[row], &st);
    snprintf(label, 24, "%d %s", row + 1, s->lane_name[row]);
    if (st) snprintf(val, 24, "%3d %s %+d", s->lane_note[row], snd, st);
    else snprintf(val, 24, "%3d %s", s->lane_note[row], snd);
}

static void map_press(Seq *s, Ui *u, Btn b, int dx, int dy) {
    int *row = &u->row[P_DRUMMAP], k = *row, ch = s->track[audition_track(s, u)].channel;
    int confirm = u->map_confirm;
    u->map_confirm = 0;
    if (u->naming) { name_press(s, u, b, dx, dy); return; }
    if (dy) *row = wrap(*row + dy, MAP_ROWS);
    else if (b == B_B) u->page = P_MENU;
    else if (k == SEQ_LANES) {   /* PRESET: left/right picks one, A twice puts all 8 lanes on it */
        if (dx) { u->map_preset = wrap(u->map_preset + dx, DRUM_PRESETS); return; }
        if (b != B_A) return;
        if (!confirm) { u->map_confirm = 1; say(u, "A AGAIN: ALL 8 LANES TO THIS PRESET"); return; }
        seq_drum_map_preset(s, u->map_preset);
        snprintf(u->msg_buf, sizeof u->msg_buf, "DRUM MAP: %s", seq_drum_preset_name(u->map_preset));
        say(u, u->msg_buf);
    }
    else if (dx) {   /* change the note and hear it straight away; hold Y: the next Felucca sound */
        if (u->held[B_Y]) s->lane_note[k] = (uint8_t)seq_felucca_next_sound(s->lane_note[k], dx);
        else seq_lane_note(s, k, dx);
        seq_audition(s, ch, s->lane_note[k], 100);
    }
    else if (b == B_A) seq_audition(s, ch, s->lane_note[k], 100);
    else if (b == B_X) {   /* rename (X, so that holding Y for the next sound never opens the editor) */
        snprintf(u->name_buf, sizeof u->name_buf, "%-*s", SEQ_LANE_NAME, s->lane_name[k]);
        u->name_pos = 0;
        u->naming = 1;
    }
}

/* ---- MIDI (M > MIDI OUTPUT): pick the output port, see what is on USB ---- */

/* Is port name the one the output is connected to now? (raw ports add " (raw)") */
static int port_in_use(const char *desc, const char *name) {
    size_t n = strlen(name);
    return n && !strncmp(desc, name, n) && (!desc[n] || !strcmp(desc + n, " (raw)"));
}

static void midi_row_text(const Ui *u, int row, const char *desc, char *label, char *val) {
    val[0] = 0;
    if (row == 0) {
        snprintf(label, 24, "%sAUTO", u->p->midi && !u->p->midi->want[0] ? "* " : "  ");
        snprintf(val, 24, "FIRST FOUND");
        return;
    }
    const char *nm = u->ports[row - 1];
    snprintf(label, 24, "%s%.21s", port_in_use(desc, nm) ? "* " : "  ", nm);
}

/* Keeps the choice for the next start (seq4h.midi; empty = AUTO). */
static void midi_keep(Ui *u, const char *name) {
    if (!u->midi_file) return;
    FILE *f = fopen(u->midi_file, "w");
    if (!f) { fprintf(stderr, "seq4h: cannot write %s\n", u->midi_file); return; }
    fprintf(f, "%s\n", name);
    fclose(f);
}

static void midi_press(Ui *u, Btn b, int dy) {
    int *row = &u->row[P_MIDI];
    Midi *m = u->p->midi;
    if (dy) *row = wrap(*row + dy, 1 + u->nports);
    else if (b == B_B) u->page = P_MENU;
    else if (b == B_A) {
        if (!m || !m->choose) { say(u, "MIDI OUTPUT IS OFF (--MIDI NONE)"); return; }
        const char *name = *row == 0 ? "" : u->ports[*row - 1];
        m->choose(m, name);
        u->midi_up = 0;   /* the next poll re-sends Start and clears held notes */
        midi_keep(u, name);
        say(u, *row == 0 ? "MIDI: FIRST PORT FOUND" : "MIDI PORT CHOSEN AND KEPT");
    }
}

/* Every second: USB devices (logged when they change) and, on M > MIDI, the
 * output ports. Runs outside the player lock: neither touches the open port. */
static void scan_midi(Ui *u) {
    u->nusb = usb_scan(NULL, u->usb, MAX_USB);
    char sig[sizeof u->usb_sig] = "", line[160];
    for (int i = 0; i < u->nusb; i++) {
        usb_describe(&u->usb[i], line, sizeof line);
        size_t l = strlen(sig);
        snprintf(sig + l, sizeof sig - l, "%s;", line);
    }
    if (strcmp(sig, u->usb_sig) || !u->usb_sig[0]) {
        if (strcmp(sig, u->usb_sig)) {
            if (!u->nusb) fprintf(stderr, "seq4h: USB: no device seen\n");
            else fprintf(stderr, "seq4h: USB devices seen (%d):\n", u->nusb);
            for (int i = 0; i < u->nusb; i++) {
                usb_describe(&u->usb[i], line, sizeof line);
                fprintf(stderr, "seq4h: USB   %s\n", line);
            }
        }
        snprintf(u->usb_sig, sizeof u->usb_sig, "%s", sig[0] ? sig : "-");
    }
    if (u->page == P_MIDI) {
        Midi *m = u->p->midi;
        u->nports = m && m->ports ? m->ports(m, u->ports, MAX_PORTS) : 0;
        if (u->row[P_MIDI] > u->nports) u->row[P_MIDI] = u->nports;
    }
}

/* The stick, up (d = 1) or down (-1): velocity of what is under the cursor
 * on the GRID, piano roll and drum grid; on list pages the selected row's
 * value, as right / left. */
#define STICK_VEL 4
static void stick_vertical(Seq *s, Ui *u, int d) {
    SeqTrack *tr = &s->track[u->track];
    int big = u->held[B_Y], i = u->step;
    if (u->page == P_GRID && u->on_label) {   /* the track box: every step's velocity */
        int lo, hi, avg;
        seq_all_adjust(s, u->track, ALL_VEL, d * STICK_VEL);
        if (seq_all_range(s, u->track, ALL_VEL, &lo, &hi, &avg)) {
            snprintf(u->msg_buf, sizeof u->msg_buf, "T%d VELOCITY %d TO %d", u->track + 1, lo, hi);
            say(u, u->msg_buf);
        }
    } else if ((u->page == P_GRID && is_drums(s, u->track)) || (u->page == P_DRUMS && u->drum_label)) {
        /* every hit on this step (drum grid lane name: every hit of the lane) */
        int n = 0, v = 0;
        for (int l = 0; l < SEQ_LANES; l++) {
            if (u->page == P_DRUMS && l != u->lane) continue;
            for (int k = 0; k < tr->len; k++) {
                if ((u->page == P_GRID && k != i) || !tr->drum.vel[l][k]) continue;
                seq_drum_adjust(s, u->track, l, k, d * STICK_VEL, 0);
                v = tr->drum.vel[l][k];
                n++;
            }
        }
        if (!n) { say(u, "NO HITS HERE"); return; }
        if (n == 1) snprintf(u->msg_buf, sizeof u->msg_buf, "VELOCITY %d", v);
        else snprintf(u->msg_buf, sizeof u->msg_buf, "%d HITS %s", n, d > 0 ? "LOUDER" : "SOFTER");
        say(u, u->msg_buf);
    } else if (u->page == P_DRUMS) {
        if (!tr->drum.vel[u->lane][i]) { say(u, "NO HIT HERE"); return; }
        seq_drum_adjust(s, u->track, u->lane, i, d * STICK_VEL, 0);
        snprintf(u->msg_buf, sizeof u->msg_buf, "VELOCITY %d", tr->drum.vel[u->lane][i]);
        say(u, u->msg_buf);
    } else if (u->page == P_GRID || u->page == P_ROLL) {
        seq_step_adjust(s, u->track, i, 0, d * STICK_VEL, 0, 0);
        snprintf(u->msg_buf, sizeof u->msg_buf, "STEP %d VELOCITY %d%s", i + 1, tr->step[i].vel, tr->step[i].on ? "" : " (OFF)");
        say(u, u->msg_buf);
    }
    else if (u->page == P_STEP) step_row_adjust(s, u, u->row[P_STEP], d, big);
    else if (u->page == P_TRACK) track_row_adjust(s, u, u->row[P_TRACK], d, big);
    else if (u->page == P_GEN) gen_row_adjust(s, u, u->row[P_GEN], d, big);
    else if (u->page == P_ALLSTEPS) all_row_adjust(s, u, u->row[P_ALLSTEPS], d, big);
}

/* Hold Select and press: A roll, X undo, Y lock the track, stick up/down tempo. */
static void select_combo(Seq *s, Ui *u, Btn b) {
    if (b == B_A) roll(s, u);
    else if (b == B_X) undo(s, u);
    else if (b == B_Y) label_lock(s, u);
    else {
        seq_adjust_bpm(s, b == B_VUP ? 1 : -1);
        snprintf(u->msg_buf, sizeof u->msg_buf, "BPM %d", s->bpm);
        say(u, u->msg_buf);
    }
}

/* Select pressed and released on its own: the GRID opens the piano roll (note
 * track) or drum grid (drum track); from either, back to the GRID. */
static void select_tap(Ui *u) {
    player_lock(u->p);
    Seq *s = &u->p->seq;
    if (u->page == P_GRID && u->on_label) open_all(u);   /* the track box: ALL STEPS */
    else if (u->page == P_ALLSTEPS) { u->page = P_GRID; u->on_label = 1; }
    else if (u->page == P_GRID) {
        u->on_label = 0;
        if (is_drums(s, u->track)) open_drums(s, u);
        else open_roll(s, u);
    }
    else if (u->page == P_ROLL || u->page == P_DRUMS) u->page = P_GRID;
    player_unlock(u->p);
}

/* Controls (same on every page unless noted):
 *   L1/R1 page   L2/R2 track   B back to grid   Start play/stop   Select+Start quit
 *   M main menu (B or M again closes it)
 *   GRID: d-pad moves the cursor, A step on/off, X edit the step, Y lock the step
 *         left from step 1 puts the cursor on the track label: A mute, B solo,
 *         X roll the track (on release; hold X undoes), Y lock the track,
 *         Select ALL STEPS, up/down track, right back to step 1
 *   ALL STEPS: up/down row, left/right moves every step's value, A sets every
 *         step to the average (STEP LOCKS row: lock all / clear), B back
 *   DRUM GRID: the same left of step 1 on a lane name: A mute the lane, B solo it,
 *         X hear its note, Y lock it (B or Select elsewhere: back to the GRID)
 *   stick up/down: velocity (GRID step, piano roll note, drum hit; on the track
 *         box every step), on list pages the selected row's value; stick press: roll
 *   hold Select + A roll, + X undo, + Y lock the track, + stick up/down tempo
 *         (Select on its own still opens the roll / drum grid; Select+Start quits)
 *   lists: up/down row, left/right value (hold Y for big steps)
 *          STEP A step on/off; GENERATE A rolls, or does the selected row's action
 *          (set anchor, apply transform, load/save slot, snap notes), X undoes
 *   SLOTS (M > SLOTS): up/down slot, A load, X save here, Y rename, Select copy/clear,
 *          B back to the menu; renaming: up/down letter, left/right move, X space,
 *          A keep, B cancel */
static void press(Ui *u, Btn b) {
    Player *p = u->p;
    if (!u->save_due) u->save_due = SDL_GetTicks() + 2000;   /* autosave shortly after edits */
    int dx = (b == B_RIGHT) - (b == B_LEFT), dy = (b == B_DOWN) - (b == B_UP);
    player_lock(p);
    Seq *s = &p->seq;
    int in_menu = u->page == P_MENU || u->page == P_SLOTS || u->page == P_DRUMMAP || u->page == P_MIDI;
    int was_midi = u->page == P_MIDI;
    int sub = u->page == P_ROLL || u->page == P_DRUMS || u->page == P_ALLSTEPS;   /* opened from the GRID */
    int from = in_menu ? u->menu_from : sub ? P_GRID : u->page;   /* where L1/R1 count from */
    if (b == B_QUIT || (b == B_START && u->held[B_SEL])) u->quit = 1;
    else if (b == B_START) { s->playing ? seq_stop(s) : seq_start(s); }
    else if (b == B_MENU) {
        u->naming = u->slot_more = u->slot_confirm = u->map_confirm = 0;
        if (in_menu) u->page = u->menu_from;
        else { u->menu_from = u->page; u->page = P_MENU; }
    }
    else if (u->held[B_SEL] && !in_menu && (b == B_A || b == B_X || b == B_Y || b == B_VUP || b == B_VDOWN)) select_combo(s, u, b);
    else if (b == B_ROLL && !u->naming) roll(s, u);   /* the stick press: quick roll, on every page */
    else if ((b == B_VUP || b == B_VDOWN) && !in_menu) stick_vertical(s, u, b == B_VUP ? 1 : -1);
    else if (u->page == P_SLOTS && u->naming) slots_press(s, u, b, dx, dy);   /* the editor takes every button */
    else if (u->page == P_DRUMMAP && u->naming) map_press(s, u, b, dx, dy);
    else if (u->page == P_DRUMMAP && b != B_L && b != B_R && b != B_PAGE && b != B_L2 && b != B_R2) map_press(s, u, b, dx, dy);
    else if (u->page == P_MIDI && b != B_L && b != B_R && b != B_PAGE && b != B_L2 && b != B_R2) midi_press(u, b, dy);
    else if (u->page == P_SLOTS && (b == B_L2 || b == B_R2)) {
        u->track = wrap(u->track + (b == B_R2 ? 1 : -1), SEQ_TRACKS);
        u->slot_more = u->slot_confirm = 0;
    }
    else if (u->page == P_SLOTS && b != B_L && b != B_R && b != B_PAGE) slots_press(s, u, b, dx, dy);
    else if (u->page == P_MENU && b == B_B) u->page = u->menu_from;
    else if (u->page == P_MENU && b == B_A) menu_row_select(s, u, u->row[P_MENU]);
    else if (b == B_L || b == B_R || b == B_PAGE) {
        int back = sub ? u->page : 0;   /* from the roll or drum grid: B on STEP comes back here */
        u->page = wrap(from + (b == B_L ? -1 : 1), P_TABS);
        u->step_back = u->page == P_STEP ? back : 0;
    }
    else if ((b == B_PREV || b == B_NEXT) && !in_menu) {   /* the stick: previous / next step, on every page */
        int n = u->page == P_GRID ? grid_steps(s) : s->track[u->track].len;
        u->step = wrap(u->step + (b == B_NEXT ? 1 : -1), n);
        u->on_label = u->drum_label = 0;
    }
    else if (b == B_L2 || b == B_R2) { u->track = wrap(u->track + (b == B_R2 ? 1 : -1), SEQ_TRACKS); sub_page_for_track(s, u); }
    else if (u->page == P_ROLL && (b == B_A || b == B_X || b == B_Y || dx || dy)) roll_press(s, u, b, dx, dy);
    else if (u->page == P_DRUMS && (b == B_A || b == B_X || b == B_Y || b == B_LOCK || dx || dy || (b == B_B && u->drum_label))) drums_press(s, u, b, dx, dy);
    else if (u->page == P_GRID && u->on_label && (b == B_A || b == B_B || b == B_X || b == B_Y || b == B_LOCK || dx || dy)) {
        if (b == B_A) label_mute(s, u);
        else if (b == B_B) label_solo(s, u);
        else if (b == B_X) label_x_down(u);           /* rolls on release; held, it undoes */
        else if (b == B_Y || b == B_LOCK) label_lock(s, u);
        else if (dx > 0) { u->on_label = 0; u->step = 0; }
        else if (dy) { u->track = wrap(u->track + dy, SEQ_TRACKS); u->x_armed = 0; }
    }
    else if (b == B_ROLL) roll(s, u);
    else if (b == B_UNDO) undo(s, u);
    else if (b == B_LOCK) seq_step_lock_toggle(s, u->track, u->step);
    else if (b == B_B && u->page == P_ALLSTEPS) { u->page = P_GRID; u->on_label = 1; }   /* back to the track box */
    else if (b == B_B) {
        u->page = u->page == P_STEP && u->step_back ? u->step_back : P_GRID;
        u->step_back = 0;
    }
    else if (u->page == P_GRID && is_drums(s, u->track) && (b == B_A || b == B_X)) open_drums(s, u);   /* a drum step has 8 lanes */
    else if (u->page == P_GRID && is_drums(s, u->track) && b == B_Y) {   /* lock the step: every lane's hit on it */
        seq_step_lock_toggle(s, u->track, u->step);
        snprintf(u->msg_buf, sizeof u->msg_buf, "STEP %d %s", u->step + 1, seq_step_locked(s, u->track, u->step) ? "LOCKED (ALL LANES)" : "UNLOCKED");
        say(u, u->msg_buf);
    }
    else if (u->page == P_GRID) {
        if (b == B_A) seq_step_toggle(s, u->track, u->step);
        else if (b == B_X) { u->page = P_STEP; u->step_back = 0; }
        else if (b == B_Y) s->track[u->track].step[u->step].lock ^= 1;
        else if (dx < 0 && u->step == 0) u->on_label = 1;   /* left of step 1: the track label */
        else if (dx || dy) { u->step = wrap(u->step + dx, grid_steps(s)); u->track = wrap(u->track + dy, SEQ_TRACKS); }
    } else {
        int *row = &u->row[u->page], big = u->held[B_Y];
        if (dy) { *row = wrap(*row + dy, page_rows(u, u->page)); u->confirm_new = 0; }
        else if (dx && u->page == P_STEP && u->held[B_X])   /* hold X: previous / next step, the row stays */
            u->step = wrap(u->step + dx, s->track[u->track].len);
        else if (dx && u->page == P_STEP) step_row_adjust(s, u, *row, dx, big);
        else if (dx && u->page == P_TRACK) track_row_adjust(s, u, *row, dx, big);
        else if (dx && u->page == P_MENU) menu_row_adjust(s, u, *row, dx, big);
        else if (dx && u->page == P_ALLSTEPS) all_row_adjust(s, u, *row, dx, big);
        else if (b == B_A && u->page == P_ALLSTEPS) all_row_action(s, u, *row);
        else if (u->page == P_ALLSTEPS) { /* X does nothing here */ }
        else if (dx) gen_row_adjust(s, u, *row, dx, big);
        else if (b == B_A && u->page == P_STEP && is_drums(s, u->track)) { if (*row > 0) seq_drum_toggle(s, u->track, *row - 1, u->step); }
        else if (b == B_A && u->page == P_STEP) seq_step_toggle(s, u->track, u->step);
        else if (b == B_A && u->page == P_GEN) gen_action(s, u, *row);
        else if (b == B_X && u->page == P_GEN) undo(s, u);
    }
    player_unlock(p);
    if (u->page == P_MIDI && !was_midi) scan_midi(u);   /* list the ports as the page opens */
}

static int repeats(Btn b) { return b <= B_RIGHT || b == B_PREV || b == B_NEXT || b == B_VUP || b == B_VDOWN; }  /* the d-pad and the stick auto-repeat */

static void set_button(Ui *u, Btn b, int down) {
    if (down && !u->held[b]) {
        if (b == B_SEL) u->sel_used = 0;
        else if (u->held[B_SEL]) u->sel_used = 1;   /* Select+Start quits, so Select's release must not also open a page */
        u->held[b] = 1;
        u->next_rep[b] = SDL_GetTicks() + 350;
        press(u, b);
    } else if (!down) {
        int was = u->held[b];
        u->held[b] = 0;
        if (b == B_SEL && was && !u->sel_used && !u->quit) select_tap(u);
        if (b == B_X && was) label_x_release(u);
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
        text(r, x, 46, 1, i == u->page || (i == P_GRID && (u->page == P_ROLL || u->page == P_DRUMS || u->page == P_ALLSTEPS)) ? YELLOW : DIM, PAGE_NAMES[i]);
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
        if (tr->locked) { rect(r, 40, y + 5, 9, 11, (Col){255, 150, 40}); text(r, 42, y + 7, 1, BG, "L"); }   /* track lock */
        if (tr->mute) text(r, 12, y + 38, 1, DIM, "MUTE");
        else if (tr->solo) text(r, 12, y + 38, 1, YELLOW, "SOLO");
        else text(r, 12, y + 38, 1, scale(base, tr->evo.every ? 100 : 60), tr->type == TRACK_DRUMS ? "DRUMS" : gen_short_name(tr->gen.type));   /* bright = evolving */
        int head = seq_playhead(&s, t);
        /* How much of each step a held note from an earlier step covers: 100 =
         * all of it, less = the note ends part-way (its gate), 0 = none. A held
         * note runs on until its hold ends or the next note cuts it off, and
         * wraps round the end of the pattern as it does when playing. */
        uint8_t tail[SEQ_STEPS] = {0};
        for (int i = 0; i < tr->len && tr->type == TRACK_NOTES; i++) {
            const SeqStep *st = &tr->step[i];
            if (!st->on || !st->hold) continue;
            for (int k = 1; k <= st->hold && k < tr->len; k++) {
                int j = (i + k) % tr->len;
                if (tr->step[j].on) break;
                int pct = k < st->hold ? 100 : st->gate;
                if (pct > tail[j]) tail[j] = (uint8_t)pct;
            }
        }
        for (int i = view; i < view + GRID_COLS; i++) {
            int x = X0 + (i - view) * CELL, w = CELL - 4, h = ROW_H - 8;
            if (i >= tr->len) { frame(r, x, y, w, h, 1, (Col){40, 42, 52}); continue; }
            rect(r, x, y, w, h, (i / 4) % 2 ? (Col){38, 40, 52} : (Col){30, 32, 42});
            const SeqStep *st = &tr->step[i];
            if (tr->type == TRACK_DRUMS) {   /* a drum step: one thin bar per lane that hits, kick at the top */
                for (int l = 0; l < SEQ_LANES; l++) {
                    int v = tr->drum.vel[l][i];
                    if (v) rect(r, x + 3, y + 3 + l * 6, w - 6, 5, scale(seq_lane_audible(tr, l) ? base : scale(base, 30), 35 + 65 * v / 127));
                }
                if (tr->drum_step_lock & (1u << i)) rect(r, x + w - 9, y + h - 8, 6, 6, YELLOW);   /* step lock */
                if (i == head) frame(r, x - 1, y - 1, w + 2, h + 2, 2, WHITE);
                if (t == u->track && i == u->step && !u->on_label) frame(r, x - 2, y - 2, w + 4, h + 4, 2, YELLOW);
                continue;
            }
            int band_y = y + h / 2 - 6;   /* held notes: a bar through the middle of the steps they cover */
            if (!st->on && tail[i]) {
                Col tc = scale(base, 55);
                rect(r, x - 4, band_y, 4, 12, tc);   /* bridge the gap from the step before */
                rect(r, x, band_y, w * tail[i] / 100, 12, tc);
            }
            if (st->on) {
                /* An offset moves the block left (early) or right (late) inside its box. */
                int sh = st->nudge * CELL / SEQ_TICKS_PER_STEP;
                int bx = x + 3 + (sh > 0 ? sh : 0), bw = w - 6 - (sh < 0 ? -sh : sh);
                if (st->hold) rect(r, bx + bw, band_y, x + w - bx - bw, 12, scale(base, 55));   /* into the held bar */
                rect(r, bx, y + 3, bw, h - 6, scale(base, 35 + 65 * st->vel / 127));
                if (st->prob < 100) rect(r, bx, y + 3, bw * st->prob / 100, 3, WHITE);
                /* gate: the length within this step; a held note plays all of it */
                rect(r, bx, y + h - 6, st->hold ? bw : bw * st->gate / 100, 3, scale(WHITE, 70));
                if (st->hold) {   /* tied over more steps */
                    snprintf(buf, sizeof buf, "+%d", st->hold);
                    text(r, bx + 1, y + h - 16, 1, WHITE, buf);
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
        snprintf(buf, sizeof buf, "T%d  MUTE %s  SOLO %s  LOCK %s  CH %d", u->track + 1, tr->mute ? "ON" : "OFF", tr->solo ? "ON" : "OFF",
                 tr->locked ? "ON" : "OFF", tr->channel + 1);
        text(r, 20, y, 2, WHITE, buf);
        if (seq_any_solo(&s)) {
            int n = snprintf(buf, sizeof buf, "SOLO:");
            for (int t = 0; t < SEQ_TRACKS; t++) if (s.track[t].solo) n += snprintf(buf + n, sizeof buf - n, " T%d", t + 1);
            snprintf(buf + n, sizeof buf - n, "  (OTHERS SILENT)");
        } else snprintf(buf, sizeof buf, "NO TRACK SOLOED");
        text(r, 20, y + 22, 2, WHITE, buf);
        snprintf(buf, sizeof buf, "T%d IS %s%s", u->track + 1, seq_track_audible(&s, u->track) ? "PLAYING" : "SILENT",
                 tr->locked ? ", LOCKED (ROLL/EVOLVE SKIP IT)" : "");
        text(r, 20, y + 44, 2, DIM, buf);
        text(r, 20, H - 48, 1, DIM, "A MUTE  B SOLO  X ROLL THIS TRACK (HOLD X: UNDO)  Y LOCK TRACK  SELECT ALL STEPS");
        text(r, 20, H - 34, 1, DIM, "UP/DOWN TRACK  RIGHT BACK TO STEP 1  START PLAY/STOP");
        text(r, 20, H - 20, 1, DIM, "L1/R1 PAGE  L2/R2 TRACK  M MENU  SELECT+START QUIT");
        return;
    }
    if (tr->type == TRACK_DRUMS) {   /* which lanes hit on this step */
        snprintf(buf, sizeof buf, "T%d STEP %02d DRUMS%s  LEN %d  CH %d", u->track + 1, u->step + 1,
                 tr->drum_step_lock & (1u << u->step) ? " LOCKED" : "", tr->len, tr->channel + 1);
        text(r, 20, y, 2, WHITE, buf);
        int n = 0;
        buf[0] = 0;
        for (int l = 0; l < SEQ_LANES; l++)
            if (tr->drum.vel[l][u->step] && n < 120) n += snprintf(buf + n, sizeof buf - n, "%s%s", n ? " " : "", s.lane_name[l]);
        text(r, 20, y + 22, n > 40 ? 1 : 2, WHITE, n ? buf : "NO HITS");
        text(r, 20, y + 44, 2, DIM, "SELECT OR A: OPEN THE DRUM GRID");
        text(r, 20, H - 34, 1, DIM, "D-PAD MOVE  A/X/SELECT DRUM GRID  Y LOCK STEP  START PLAY/STOP  LEFT OF STEP 1: TRACK BOX");
        text(r, 20, H - 20, 1, DIM, "L1/R1 PAGE  L2/R2 TRACK  M MENU  B BACK TO GRID  SELECT+START QUIT");
        return;
    }
    {
        char ob[16] = "";
        if (st->nudge) snprintf(ob, sizeof ob, "  %s %d%%", st->nudge < 0 ? "EARLY" : "LATE", (abs(st->nudge) * 100 + 12) / SEQ_TICKS_PER_STEP);
        snprintf(buf, sizeof buf, "T%d STEP %02d %s%s%s  LEN %d  CH %d", u->track + 1, u->step + 1, st->on ? "ON" : "OFF",
                 st->lock ? " LOCKED" : "", ob, tr->len, tr->channel + 1);
    }
    text(r, 20, y, 2, WHITE, buf);
    {   /* hold rides on the gate ("GATE 50 +2"); the offset goes on the line above */
        char hb[8] = "";
        if (st->hold) snprintf(hb, sizeof hb, " +%d", st->hold);
        snprintf(buf, sizeof buf, "NOTE %s  VEL %d  GATE %d%s  PROB %d", nn, st->vel, st->gate, hb, st->prob);
    }
    text(r, 20, y + 22, 2, WHITE, buf);
    if (tr->evo.every) snprintf(buf, sizeof buf, "GEN %s  EVOLVE EVERY %d BAR%s %d%%", gen_name(tr->gen.type), tr->evo.every, tr->evo.every > 1 ? "S" : "", tr->evo.amount);
    else snprintf(buf, sizeof buf, "GEN %s  EVOLVE OFF", gen_name(tr->gen.type));
    text(r, 20, y + 44, 2, DIM, buf);

    text(r, 20, H - 34, 1, DIM, "D-PAD MOVE  A ON/OFF  X EDIT  Y LOCK  SELECT PIANO ROLL  START PLAY/STOP  LEFT OF 1: TRACK BOX");
    text(r, 20, H - 20, 1, DIM, "L1/R1 PAGE  L2/R2 TRACK  M MENU  B BACK TO GRID  SELECT+START QUIT");
}

/* Pattern preview of the current track: height = pitch, brightness = velocity.
 * On the STEP page the step being edited is outlined. */
static void draw_preview(SDL_Renderer *r, const Seq *s, const Ui *u) {
    const SeqTrack *tr = &s->track[u->track];
    Col base = TRACK_COL[u->track];
    int px = 392, py = 70, pw = tr->len > 16 ? 240 / tr->len : 15, ph = 150;
    text(r, px, py - 14, 1, DIM, tr->type == TRACK_DRUMS ? "DRUM LANES" : "PATTERN");
    rect(r, px - 4, py - 2, 16 * pw + 6, ph + 4, (Col){24, 26, 36});
    if (tr->type == TRACK_DRUMS) {
        int head = seq_playhead(s, u->track), lh = ph / SEQ_LANES;
        for (int i = 0; i < tr->len; i++) {
            for (int l = 0; l < SEQ_LANES; l++)
                if (tr->drum.vel[l][i]) rect(r, px + i * pw, py + l * lh + 2, pw - 3, lh - 4, scale(base, 35 + 65 * tr->drum.vel[l][i] / 127));
            if (i == head) frame(r, px + i * pw - 1, py, pw - 1, ph, 1, WHITE);
            if (u->page == P_STEP && i == u->step) frame(r, px + i * pw - 2, py - 1, pw + 1, ph + 2, 2, YELLOW);
        }
        return;
    }
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

/* The 8-letter name editor (slot and lane names), at the right of a list page. */
static void draw_name_editor(SDL_Renderer *r, const Ui *u, int y) {
    text(r, 392, y, 2, YELLOW, "RENAME");
    for (int i = 0; i < SEQ_SLOT_NAME; i++) {
        int x = 392 + i * 28;
        char ch[2] = {u->name_buf[i], 0};
        rect(r, x, y + 24, 24, 34, i == u->name_pos ? (Col){40, 44, 64} : (Col){24, 26, 36});
        if (i == u->name_pos) frame(r, x, y + 24, 24, 34, 2, YELLOW);
        text(r, x + 5, y + 30, 2, i == u->name_pos ? YELLOW : WHITE, ch[0] == ' ' ? "_" : ch);
    }
    text(r, 392, y + 70, 1, DIM, "UP/DOWN LETTER  LEFT/RIGHT MOVE");
    text(r, 392, y + 82, 1, DIM, "X SPACE  A KEEP  B CANCEL");
}

/* Text wrapped at w characters, at most lines lines; returns the next y. */
static int wrapped(SDL_Renderer *r, int x, int y, int w, int lines, Col c, const char *s) {
    char buf[64];
    for (int i = 0; i < lines && *s; i++, y += 12) {
        int n = (int)strlen(s);
        if (n > w) {   /* break at the last space that fits */
            n = w;
            for (int k = w; k > w / 2; k--) if (s[k] == ' ') { n = k; break; }
        }
        snprintf(buf, sizeof buf, "%.*s", n, s);
        text(r, x, y, 1, c, buf);
        s += n;
        while (*s == ' ') s++;
    }
    return y;
}

/* Right-hand side of the MIDI page: the connection, what is on USB, and what
 * that most likely means. */
static void draw_midi_side(SDL_Renderer *r, const Ui *u, const char *desc) {
    char buf[96];
    const Midi *m = u->p->midi;
    int waiting = !strncmp(desc, "WAITING", 7), off = !m || !m->choose;
    text(r, 392, 62, 2, YELLOW, "MIDI OUTPUT");
    int y = wrapped(r, 392, 86, 41, 2, off ? DIM : waiting ? YELLOW : (Col){90, 220, 120}, desc);
    snprintf(buf, sizeof buf, "CHOSEN: %s", off ? "NONE" : m->want[0] ? m->want : "AUTO (FIRST FOUND)");
    y = wrapped(r, 392, y + 2, 41, 2, DIM, buf);
    if (u->row[P_MIDI] > 0) {
        snprintf(buf, sizeof buf, "THIS ROW: %s", u->ports[u->row[P_MIDI] - 1]);
        y = wrapped(r, 392, y, 41, 2, WHITE, buf);
    }

    int devs = 0, midi = 0, bound = 0;
    y += 8;
    text(r, 392, y, 1, WHITE, "USB DEVICES SEEN:");
    y += 14;
    for (int i = 0; i < u->nusb && y < 330; i++) {
        const UsbDev *d = &u->usb[i];
        if (!d->hub) { devs++; midi += d->midi; bound += d->midi && d->driver[0]; }
        snprintf(buf, sizeof buf, "%s %.30s", d->id, d->name);
        text(r, 392, y, 1, d->midi ? WHITE : DIM, buf);
        if (d->hub) snprintf(buf, sizeof buf, "  HUB");
        else if (d->midi) snprintf(buf, sizeof buf, "  USB MIDI, DRIVER %s", d->driver[0] ? d->driver : "NONE");
        else snprintf(buf, sizeof buf, "  NOT A USB MIDI DEVICE%s", d->audio ? " (AUDIO ONLY)" : "");
        text(r, 392, y + 11, 1, d->midi && d->driver[0] ? (Col){90, 220, 120} : d->hub ? DIM : YELLOW, buf);
        y += 26;
    }
    if (!u->nusb) { text(r, 392, y, 1, YELLOW, "NONE"); y += 14; }

    const char *hint =
        off ? "SEQ4H WAS STARTED WITH --MIDI NONE." :
        !devs ? "NOTHING DETECTED ON USB. CHECK THE OTG ADAPTER, TRY THE CABLE IN A POWERED HUB, AND REPLUG." :
        !midi ? "A DEVICE IS SEEN BUT IT IS NOT USB MIDI CLASS. THIS CABLE NEEDS ITS OWN DRIVER: USE ANOTHER." :
        !bound ? "USB MIDI SEEN BUT NO DRIVER TOOK IT. QUIT AND START SEQ4H AGAIN WITH IT PLUGGED IN." :
        !u->nports ? "USB MIDI AND DRIVER OK BUT NO PORT YET. WAIT A SECOND OR REPLUG." :
        waiting ? "PICK A PORT ON THE LEFT AND PRESS A." :
        "OK. NO SOUND? SWAP THE DIN PLUGS, USE A TYPE A TRS ADAPTER, CHECK THE MIDI IN CHANNELS.";
    wrapped(r, 392, y + 8, 41, 5, WHITE, hint);
    text(r, 20, H - 34, 1, DIM, "UP/DOWN PORT  A USE THIS PORT (KEPT FOR NEXT START)  * = IN USE");
    text(r, 20, H - 20, 1, DIM, "B BACK TO MENU  M CLOSE MENU  START PLAY/STOP");
}

/* Right-hand side of the DRUM MAP page. */
static void draw_map_side(SDL_Renderer *r, const Seq *s, const Ui *u) {
    char buf[64], nn[8];
    int at = audition_track(s, u), ch = s->track[at].channel + 1, k = u->row[P_DRUMMAP];
    text(r, 392, 62, 2, YELLOW, "DRUM MAP");
    /* where the notes go: Felucca's MIDI IN CH1-4 plays channel n on FM1 track n */
    snprintf(buf, sizeof buf, "PLAYS ON T%d, MIDI CHANNEL %d", at + 1, ch);
    text(r, 392, 86, 1, WHITE, buf);
    if (!is_drums(s, at)) text(r, 392, 98, 1, YELLOW, "NO DRUM TRACK: SET TRACK > TYPE");
    else if (ch <= 4) {
        snprintf(buf, sizeof buf, "FM1 TRACK %d MUST USE THE DRUM ENGINE", ch);
        text(r, 392, 98, 1, YELLOW, buf);
    } else text(r, 392, 98, 1, YELLOW, "FM1 MIDI IN CH1-4 HEARS CH 1-4 ONLY");
    int y = 120;
    if (u->naming) draw_name_editor(r, u, y);
    else {
        if (k < SEQ_LANES) {
            int st;
            const char *snd = seq_felucca_sound(s->lane_note[k], &st);
            seq_note_name(s->lane_note[k], nn);
            snprintf(buf, sizeof buf, "%s: NOTE %d (%s)", s->lane_name[k], s->lane_note[k], nn);
            text(r, 392, y, 1, WHITE, buf);
            if (st) snprintf(buf, sizeof buf, "FELUCCA PLAYS %s, %d SEMITONES %s", snd, abs(st), st < 0 ? "DOWN" : "UP");
            else snprintf(buf, sizeof buf, "FELUCCA PLAYS %s", snd);
            text(r, 392, y + 12, 1, WHITE, buf);
        } else {
            text(r, 392, y, 1, WHITE, "FELUCCA: THE NOTES FELUCCA'S 8");
            text(r, 392, y + 12, 1, WHITE, "LANES ARE BUILT ON. GM: CRASH 49");
        }
        text(r, 392, y + 32, 1, DIM, "LEFT/RIGHT  NOTE BY 1 (PLAYS IT)");
        text(r, 392, y + 44, 1, DIM, "HOLD Y +    NEXT FELUCCA SOUND");
        text(r, 392, y + 56, 1, DIM, " LEFT/RIGHT (KICK RIM SNARE CLAP");
        text(r, 392, y + 68, 1, DIM, "            HAT TOM CYMBAL BELL..)");
        text(r, 392, y + 80, 1, DIM, "A           PLAY IT AGAIN");
        text(r, 392, y + 92, 1, DIM, "X           RENAME THE LANE");
        text(r, 392, y + 112, 1, DIM, "ONE MAP FOR ALL DRUM TRACKS,");
        text(r, 392, y + 124, 1, DIM, "SAVED WITH THE PATTERNS.");
    }
    text(r, 20, H - 34, 1, DIM, "UP/DOWN LANE  LEFT/RIGHT NOTE  HOLD Y+LEFT/RIGHT NEXT SOUND  A PLAY  X RENAME");
    text(r, 20, H - 20, 1, DIM, "L2/R2 TRACK  B BACK TO MENU  M CLOSE MENU  START PLAY/STOP");
}

/* Right-hand side of the SLOTS page: the selected slot's pattern, then the name
 * editor or the copy / clear panel when open, and the button help. */
static void draw_slots_side(SDL_Renderer *r, const Seq *s, const Ui *u) {
    const SeqTrack *tr = &s->track[u->track];
    int k = u->row[P_SLOTS];
    const SeqSlot *sl = &tr->slot[k];
    Col base = TRACK_COL[u->track];
    char buf[64], nm[SEQ_SLOT_NAME + 1];
    seq_slot_name(s, u->track, k, nm);
    snprintf(buf, sizeof buf, "T%d SLOTS", u->track + 1);
    text(r, 392, 62, 2, base, buf);
    int px = 392, py = 100, len = sl->used ? sl->len : 16, pw = len > 16 ? 240 / len : 15, ph = 90;
    snprintf(buf, sizeof buf, "%d %s: %s", k + 1, nm, sl->used ? "STORED PATTERN" : "EMPTY");
    text(r, px, py - 14, 1, DIM, buf);
    rect(r, px - 4, py - 2, 16 * 15 + 6, ph + 4, (Col){24, 26, 36});
    if (sl->used) {
        int lo = 127, hi = 0;
        for (int i = 0; i < len; i++) if (sl->step[i].on) { if (sl->step[i].note < lo) lo = sl->step[i].note; if (sl->step[i].note > hi) hi = sl->step[i].note; }
        for (int i = 0; i < len; i++) {
            const SeqStep *st = &sl->step[i];
            if (!st->on) continue;
            int yy = py + ph - 8 - (st->note - lo) * (ph - 8) / (hi > lo ? hi - lo : 1);
            rect(r, px + i * pw, yy, pw - 3, 8, scale(base, 35 + 65 * st->vel / 127));
        }
    }
    int y = 204;
    if (tr->chain_len > 1) {
        snprintf(buf, sizeof buf, "CHAIN PLAYS SLOTS 1-%d", tr->chain_len);
        text(r, 392, y, 1, DIM, buf);
    }
    y += 18;
    if (u->naming) {
        draw_name_editor(r, u, y);
    } else if (u->slot_more) {
        static const char *L[MORE_ROWS] = {"COPY TO", "CLEAR SLOT", "BACK"};
        for (int i = 0; i < MORE_ROWS; i++) {
            int yy = y + i * 24, sel = i == u->more_row;
            if (sel) rect(r, 388, yy - 3, 244, 20, (Col){40, 44, 64});
            text(r, 394, yy, 2, sel ? YELLOW : WHITE, L[i]);
            if (i == MORE_COPY) {
                char to[SEQ_SLOT_NAME + 1];
                seq_slot_name(s, u->track, u->copy_to, to);
                snprintf(buf, sizeof buf, "SLOT %d", u->copy_to + 1);
                text(r, 494, yy, 2, sel ? YELLOW : WHITE, buf);
                if (tr->slot[u->copy_to].name[0]) text(r, 578, yy + 4, 1, sel ? YELLOW : WHITE, to);
            }
        }
        text(r, 392, y + 80, 1, DIM, "LEFT/RIGHT PICK THE SLOT TO COPY TO");
        text(r, 392, y + 92, 1, DIM, "A DO IT (A AGAIN TO CONFIRM)  B CLOSE");
    } else {
        text(r, 392, y, 1, DIM, "A     LOAD INTO THE TRACK (X ON");
        text(r, 392, y + 12, 1, DIM, "      GENERATE UNDOES IT)");
        text(r, 392, y + 26, 1, DIM, "X     SAVE THE TRACK HERE");
        text(r, 392, y + 40, 1, DIM, "Y     RENAME");
        text(r, 392, y + 54, 1, DIM, "SELECT COPY OR CLEAR");
        text(r, 392, y + 76, 1, DIM, "< = SLOT LAST LOADED OR SAVED");
    }
    text(r, 20, H - 34, 1, DIM, "UP/DOWN SLOT  A LOAD  X SAVE HERE  Y RENAME  SELECT COPY/CLEAR");
    text(r, 20, H - 20, 1, DIM, "L2/R2 TRACK  B BACK TO MENU  M CLOSE MENU  START PLAY/STOP");
}

static void draw_list(SDL_Renderer *r, const Seq *sp, const Ui *u, const char *midi_desc) {
    const Seq s = *sp;
    char label[24], val[24];
    int n = page_rows(u, u->page), pitch = n > 12 ? 22 : 30, vis = (H - 40 - 62) / pitch;
    int first = n <= vis ? 0 : clampi(u->row[u->page] - vis / 2, 0, n - vis);   /* scroll long lists */
    if (first > 0) text(r, 360, 52, 1, DIM, "^");
    if (first + vis < n) text(r, 360, 62 + vis * pitch - 8, 1, DIM, "v");
    for (int row = first; row < n && row < first + vis; row++) {
        int y = 62 + (row - first) * pitch, head = 1;
        label[0] = val[0] = 0;
        if (u->page == P_STEP) step_row_text(&s, u, row, label, val);
        else if (u->page == P_MENU) menu_row_text(&s, u, row, label, val);
        else if (u->page == P_SLOTS) { slots_row_text(&s, u, row, label, val); head = s.track[u->track].slot[row].used; }
        else if (u->page == P_TRACK) { track_row_text(&s, u, row, label, val, midi_desc); head = row == 0 || row == TRACK_BPM_ROW; }
        else if (u->page == P_DRUMMAP) map_row_text(&s, u, row, label, val);
        else if (u->page == P_MIDI) { midi_row_text(u, row, midi_desc, label, val); head = row == 0; }
        else if (u->page == P_ALLSTEPS) all_row_text(&s, u, row, label, val);
        else { gen_row_text(&s, u, row, label, val); head = gen_row_is_head(row); }
        int sel = row == u->row[u->page];
        if (sel) rect(r, 14, y - 3, 352, 20, (Col){40, 44, 64});
        text(r, 22, y, 2, sel ? YELLOW : head ? WHITE : DIM, label);
        text(r, 215, y, 2, sel ? YELLOW : WHITE, val);
    }
    if (u->page == P_SLOTS) { draw_slots_side(r, &s, u); return; }
    if (u->page == P_DRUMMAP) { draw_map_side(r, &s, u); return; }
    if (u->page == P_MIDI) { draw_midi_side(r, u, midi_desc); return; }
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
    } else if (u->page == P_ALLSTEPS) {
        char t[48];
        snprintf(t, sizeof t, "ALL STEPS OF T%d%s", u->track + 1, s.track[u->track].locked ? " (TRACK LOCKED)" : "");
        text(r, 392, 240, 1, YELLOW, t);
        text(r, 392, 256, 1, DIM, "LEFT/RIGHT MOVES EVERY STEP'S VALUE");
        text(r, 392, 268, 1, DIM, "UP OR DOWN TOGETHER (HOLD Y = BIG).");
        text(r, 392, 280, 1, DIM, "A GIVES EVERY STEP THE AVERAGE.");
        text(r, 392, 292, 1, DIM, is_drums(&s, u->track) ? "DRUMS: EVERY HIT OF EVERY LANE." : "THE RANGE IS OVER THE NOTES THAT PLAY.");
        text(r, 392, 316, 1, DIM, "STEP LOCKS: A LOCKS EVERY STEP, OR");
        text(r, 392, 328, 1, DIM, "CLEARS THEM IF ANY IS LOCKED.");
        text(r, 392, 340, 1, DIM, "LOCKED STEPS CHANGE HERE TOO.");
        text(r, 392, 364, 1, YELLOW, "B OR SELECT: BACK TO THE TRACK BOX");
    } else if (u->page == P_STEP && is_drums(&s, u->track)) {
        text(r, 392, 240, 1, DIM, "A DRUM STEP: ONE ROW PER LANE.");
        text(r, 392, 252, 1, DIM, "LEFT/RIGHT CHANGES THE HIT'S");
        text(r, 392, 264, 1, DIM, "VELOCITY; DOWN PAST 1 REMOVES IT.");
        text(r, 392, 276, 1, DIM, "A PUTS A HIT ON OR OFF.");
        text(r, 392, 300, 1, DIM, "CHANCE AND LANE LOCKS ARE IN THE");
        text(r, 392, 312, 1, DIM, "DRUM GRID (SELECT ON THE GRID).");
        text(r, 392, 336, 1, YELLOW, "HOLD X + LEFT/RIGHT, OR THE STICK:");
        text(r, 392, 348, 1, YELLOW, "PREVIOUS / NEXT STEP, SAME ROW.");
    } else if (u->page == P_STEP) {
        text(r, 392, 240, 1, DIM, "NOTE MOVES BY SCALE DEGREE;");
        text(r, 392, 252, 1, DIM, "HOLD Y TO MOVE BY OCTAVE.");
        text(r, 392, 276, 1, DIM, "LOCKED STEPS ARE NEVER CHANGED");
        text(r, 392, 288, 1, DIM, "BY ROLL OR EVOLVE.");
        text(r, 392, 312, 1, DIM, "OFFSET PLAYS THE NOTE A LITTLE");
        text(r, 392, 324, 1, DIM, "EARLY OR LATE (UP TO 46% OF A STEP).");
        text(r, 392, 336, 1, DIM, "A ROLL PUTS NOTES BACK ON THE GRID.");
        text(r, 392, 360, 1, YELLOW, "HOLD X + LEFT/RIGHT, OR THE STICK:");
        text(r, 392, 372, 1, YELLOW, "PREVIOUS / NEXT STEP, SAME ROW.");
    }
    if (u->page == P_MENU) {
        text(r, 20, H - 34, 1, DIM, "UP/DOWN ROW  LEFT/RIGHT CHANGE  A SELECT");
        text(r, 20, H - 20, 1, DIM, "M OR B CLOSE THE MENU  START PLAY/STOP");
        return;
    }
    text(r, 20, H - 34, 1, DIM, u->page == P_GEN ? "UP/DOWN ROW  LEFT/RIGHT CHANGE (HOLD Y = BIG)  A ROLL OR DO ROW  X UNDO"
                              : u->page == P_STEP ? "UP/DOWN ROW  LEFT/RIGHT CHANGE (Y = BIG)  HOLD X+LEFT/RIGHT OR STICK: OTHER STEP"
                              : u->page == P_ALLSTEPS ? "UP/DOWN ROW  LEFT/RIGHT CHANGE ALL (HOLD Y = BIG)  A ALL TO THE AVERAGE"
                              : "UP/DOWN ROW  LEFT/RIGHT CHANGE (HOLD Y = BIG STEP)");
    text(r, 20, H - 20, 1, DIM, "L1/R1 PAGE  L2/R2 TRACK  M MENU  B BACK TO GRID  START PLAY/STOP");
}

static void draw_roll(SDL_Renderer *r, const Seq *s, const Ui *u) {
    const SeqTrack *tr = &s->track[u->track];
    Col base = TRACK_COL[u->track];
    char buf[96], nn[8];
    int view = (u->step / GRID_COLS) * GRID_COLS;
    snprintf(buf, sizeof buf, "ROLL %d-%d", view + 1, view + GRID_COLS);
    text(r, 560, 46, 1, YELLOW, buf);
    int cur = gen_snap_note(s, u->roll_note), top = gen_snap_note(s, u->roll_top);
    int dtop = gen_note_degree(s, top), xend = X0 + GRID_COLS * CELL;
    /* rows: the scale's notes, the root's rows lighter */
    for (int k = 0; k < ROLL_ROWS; k++) {
        int note = gen_degree_note(s, dtop - k), y = ROLL_Y0 + k * ROLL_RH;
        if (dtop - k < 0) break;
        int root = ((note - s->root) % 12 + 12) % 12 == 0;
        seq_note_name(note, nn);
        text(r, 10, y + 8, 1, note == cur ? YELLOW : root ? WHITE : DIM, nn);
        for (int i = view; i < view + GRID_COLS; i++) {
            int x = X0 + (i - view) * CELL;
            if (i >= tr->len) { rect(r, x, y, CELL - 1, ROLL_RH - 1, (Col){20, 21, 28}); continue; }
            Col bg = (i / 4) % 2 ? (Col){34, 36, 48} : (Col){28, 30, 40};
            if (root) bg = (Col){(uint8_t)(bg.r + 14), (uint8_t)(bg.g + 14), (uint8_t)(bg.b + 18)};
            rect(r, x, y, CELL - 1, ROLL_RH - 1, bg);
        }
    }
    /* notes: a bar as long as the note sounds (gate + hold, cut by the next
     * note), shifted by its offset; brightness = velocity */
    for (int i = 0; i < tr->len; i++) {
        const SeqStep *st = &tr->step[i];
        if (!st->on) continue;
        int L = st->hold * 100 + st->gate, next = tr->len;
        for (int j = i + 1; j < tr->len; j++) if (tr->step[j].on) { next = j; break; }
        if (L > (next - i) * 100) L = (next - i) * 100;
        int x0 = X0 + (i - view) * CELL + st->nudge * CELL / SEQ_TICKS_PER_STEP, x1 = x0 + CELL * L / 100 - 2;
        int cx0 = x0 < X0 ? X0 : x0, cx1 = x1 > xend ? xend : x1;
        if (cx1 <= cx0) continue;
        uint8_t cn[SEQ_CHORD_MAX];
        int nc = tr->chord ? gen_chord_notes(s, st->note, tr->chord, cn) : 0;
        for (int c = 0; c < nc; c++) {   /* chord notes, faint (set by CHORD, not edited here) */
            int row = dtop - gen_note_degree(s, cn[c]);
            if (row >= 0 && row < ROLL_ROWS) rect(r, cx0, ROLL_Y0 + row * ROLL_RH + 8, cx1 - cx0, ROLL_RH - 17, scale(base, 30));
        }
        int row = dtop - gen_note_degree(s, st->note);
        if (row < 0 || row >= ROLL_ROWS) continue;
        int y = ROLL_Y0 + row * ROLL_RH;
        rect(r, cx0, y + 3, cx1 - cx0, ROLL_RH - 7, scale(base, 35 + 65 * st->vel / 127));
        if (!seq_in_scale(s, st->note)) frame(r, cx0, y + 3, cx1 - cx0, ROLL_RH - 7, 1, WHITE);   /* not in the scale */
        if (st->prob < 100) rect(r, cx0, y + 3, (cx1 - cx0) * st->prob / 100, 2, WHITE);
        if (st->lock && x0 >= X0) rect(r, x0 + 2, y + 5, 5, 5, YELLOW);
    }
    int head = seq_playhead(s, u->track);
    if (head >= view && head < view + GRID_COLS) {
        int x = X0 + (head - view) * CELL;
        frame(r, x - 1, ROLL_Y0 - 1, CELL + 1, ROLL_ROWS * ROLL_RH + 1, 1, WHITE);
    }
    int crow = dtop - gen_note_degree(s, cur);
    if (crow >= 0 && crow < ROLL_ROWS)
        frame(r, X0 + (u->step - view) * CELL - 2, ROLL_Y0 + crow * ROLL_RH - 2, CELL + 3, ROLL_RH + 3, 2, YELLOW);

    const SeqStep *st = &tr->step[u->step];
    int y = ROLL_Y0 + ROLL_ROWS * ROLL_RH + 8;
    if (st->on) {
        int L = st->hold * 100 + st->gate;
        char ob[16] = "";
        if (st->nudge) snprintf(ob, sizeof ob, "  %s %d%%", st->nudge < 0 ? "EARLY" : "LATE", (abs(st->nudge) * 100 + 12) / SEQ_TICKS_PER_STEP);
        seq_note_name(st->note, nn);
        snprintf(buf, sizeof buf, "STEP %02d %s  VEL %d  LEN %d.%02d%s", u->step + 1, nn, st->vel, L / 100, L % 100, ob);
    } else snprintf(buf, sizeof buf, "STEP %02d  NO NOTE", u->step + 1);
    text(r, 20, y, 2, WHITE, buf);
    seq_note_name(cur, nn);
    snprintf(buf, sizeof buf, "NEW NOTES: LEN %d.%02d", u->roll_len / 100, u->roll_len % 100);
    text(r, 400, y + 20, 1, YELLOW, buf);
    snprintf(buf, sizeof buf, "T%d %s  CURSOR %s  PROB %d%%%s  %d STEPS", u->track + 1, gen_name(tr->gen.type), nn, st->prob,
             st->lock ? "  LOCKED" : "", tr->len);
    text(r, 20, y + 20, 1, DIM, buf);
    text(r, 20, H - 34, 1, DIM, "D-PAD MOVE  A NOTE ON/OFF  HOLD Y: LEFT/RIGHT LENGTH (NEW NOTES TOO), UP/DOWN MOVE THE NOTE");
    text(r, 20, H - 20, 1, DIM, "HOLD X: LEFT/RIGHT OFFSET, UP/DOWN VELOCITY  L2/R2 TRACK  B OR SELECT BACK TO GRID");
}

static void draw_drums(SDL_Renderer *r, const Seq *s, const Ui *u) {
    const SeqTrack *tr = &s->track[u->track];
    Col base = seq_track_audible(s, u->track) ? TRACK_COL[u->track] : scale(TRACK_COL[u->track], 40);
    char buf[96], nn[8];
    int view = (u->step / GRID_COLS) * GRID_COLS, head = seq_playhead(s, u->track);
    snprintf(buf, sizeof buf, "DRUMS %d-%d", view + 1, view + GRID_COLS);
    text(r, 560, 46, 1, YELLOW, buf);
    for (int l = 0; l < SEQ_LANES; l++) {
        int y = DRUM_Y0 + l * DRUM_RH, locked = tr->drum_lock & (1 << l), cur = l == u->lane;
        if (u->drum_label && cur) { rect(r, 2, y - 2, 52, DRUM_RH - 2, (Col){40, 44, 64}); frame(r, 2, y - 2, 52, DRUM_RH - 2, 2, YELLOW); }
        int lmute = tr->drum_mute & (1 << l), lsolo = tr->drum_solo & (1 << l), heard = seq_lane_audible(tr, l);
        Col lane_col = heard ? base : scale(base, 30);   /* muted, or silent because another lane is soloed */
        text(r, 5, y + 6, 1, cur ? YELLOW : heard ? WHITE : DIM, s->lane_name[l]);
        if (lmute) { rect(r, 30, y + 26, 7, 9, (Col){200, 200, 205}); text(r, 31, y + 27, 1, (Col){16, 17, 24}, "M"); }
        if (lsolo) { rect(r, 38, y + 26, 7, 9, YELLOW); text(r, 39, y + 27, 1, (Col){16, 17, 24}, "S"); }
        seq_note_name(s->lane_note[l], nn);
        snprintf(buf, sizeof buf, "%d %s", s->lane_note[l], nn);
        text(r, 5, y + 18, 1, DIM, buf);
        if (locked) rect(r, 46, y + 26, 6, 6, YELLOW);
        for (int i = view; i < view + GRID_COLS; i++) {
            int x = X0 + (i - view) * CELL, w = CELL - 4, h = DRUM_RH - 6;
            if (i >= tr->len) { frame(r, x, y, w, h, 1, (Col){40, 42, 52}); continue; }
            rect(r, x, y, w, h, (i / 4) % 2 ? (Col){38, 40, 52} : (Col){30, 32, 42});
            int v = tr->drum.vel[l][i];
            if (v) {
                rect(r, x + 3, y + 3, w - 6, h - 6, scale(lane_col, 35 + 65 * v / 127));
                if (tr->drum.prob[l][i] < 100) rect(r, x + 3, y + 3, (w - 6) * tr->drum.prob[l][i] / 100, 3, WHITE);
            }
            if (locked) rect(r, x + w - 6, y + 2, 4, 4, YELLOW);
            if (i == head) frame(r, x - 1, y - 1, w + 2, h + 2, 1, WHITE);
            if (cur && i == u->step && !u->drum_label) frame(r, x - 2, y - 2, w + 4, h + 4, 2, YELLOW);
        }
    }
    for (int i = view; i < view + GRID_COLS && i < tr->len; i++)   /* step locks (Y on the GRID): a bar above the column */
        if (tr->drum_step_lock & (1u << i)) rect(r, X0 + (i - view) * CELL, DRUM_Y0 - 5, CELL - 4, 3, YELLOW);
    int y = DRUM_Y0 + SEQ_LANES * DRUM_RH + 6, l = u->lane, v = tr->drum.vel[l][u->step];
    seq_note_name(s->lane_note[l], nn);
    if (u->drum_label) snprintf(buf, sizeof buf, "T%d %s  NOTE %d %s%s%s%s", u->track + 1, s->lane_name[l], s->lane_note[l], nn,
                                tr->drum_mute & (1 << l) ? "  MUTED" : "", tr->drum_solo & (1 << l) ? "  SOLO" : "",
                                tr->drum_lock & (1 << l) ? "  LOCKED" : "");
    else if (v) snprintf(buf, sizeof buf, "STEP %02d %s  VEL %d  CHANCE %d%%", u->step + 1, s->lane_name[l], v, tr->drum.prob[l][u->step]);
    else snprintf(buf, sizeof buf, "STEP %02d %s  NO HIT", u->step + 1, s->lane_name[l]);
    text(r, 20, y, 2, WHITE, buf);
    snprintf(buf, sizeof buf, "T%d DRUMS  CH %d  LEN %d  %d HITS  BUSY %d%%", u->track + 1, tr->channel + 1, tr->len, seq_drum_hits(tr), tr->drum_busy);
    text(r, 20, y + 20, 1, DIM, buf);
    if (u->drum_label) {
        text(r, 20, H - 34, 1, DIM, "UP/DOWN LANE  A MUTE  B SOLO  X HEAR THE NOTE  Y LOCK  RIGHT BACK TO STEP 1");
        text(r, 20, H - 20, 1, DIM, "NOTES ARE SET ON M > DRUM MAP  L2/R2 TRACK  SELECT BACK TO GRID");
        return;
    }
    text(r, 20, H - 34, 1, DIM, "D-PAD MOVE  A HIT ON/OFF  HOLD X: UP/DOWN VELOCITY, LEFT/RIGHT CHANCE  Y LOCK LANE");
    text(r, 20, H - 20, 1, DIM, "LEFT OF STEP 1: LANE NAME (MUTE/SOLO)  L2/R2 TRACK  M MENU  B OR SELECT BACK TO GRID");
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
    else if (u->page == P_ROLL) draw_roll(r, &s, u);
    else if (u->page == P_DRUMS) draw_drums(r, &s, u);
    else draw_list(r, &s, u, midi_desc);
    if (u->page == P_GRID || u->page == P_DRUMS)   /* stick and Select shortcuts (the other pages have no room; README lists them) */
        text(r, 20, u->page == P_GRID && u->on_label ? H - 62 : H - 48, 1, (Col){150, 155, 175},
             "STICK UP/DOWN VELOCITY (LISTS: VALUE)  STICK PRESS ROLL  SELECT+ A ROLL X UNDO Y LOCK STICK TEMPO");
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

/* Keep the pattern file current while running, so stored slots, chains and
 * everything else survive however seq4h ends: Select+Start, the M menu, the
 * system's own exit hotkey killing the process, or a flat battery. Runs two
 * seconds after a button press and once a minute; the file is only written
 * when its contents changed. The player lock is held just for a copy of the
 * state, never while writing to the SD card, so MIDI timing is not held up. */
static void autosave(Ui *u, const char *path) {
    static Seq snap;
    player_lock(u->p);
    snap = u->p->seq;
    player_unlock(u->p);
    char *buf = NULL;
    size_t len = 0;
    FILE *m = open_memstream(&buf, &len);
    if (!m) return;
    seq_save(&snap, m);
    fclose(m);
    if (u->saved && len == u->saved_len && memcmp(buf, u->saved, len) == 0) { free(buf); return; }
    if (seq_save_file(&snap, path) != 0) {
        if (!u->save_failed) fprintf(stderr, "seq4h: autosave to %s failed\n", path);
        u->save_failed = 1;
        free(buf);
        return;
    }
    if (!u->saved || u->save_failed) fprintf(stderr, "seq4h: autosaved %s (%d stored slots)\n", path, seq_slots_used(&snap));
    u->save_failed = 0;
    free(u->saved);
    u->saved = buf;
    u->saved_len = len;
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
    Ui u = {.p = p, .roll_len = 100, .undo_track = -1, .xf_amt = 1, .snap = 1, .map = opt->input ? opt->input : &defaults, .show_input = opt->show_input,
           .midi_up = -1, .midi_file = opt->midi_file};
    if (opt->keys) {
        static char tmp[4096];
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
    Uint32 next_poll = 0, next_bat = 0, next_save = SDL_GetTicks() + 60000;
    while (!u.quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) u.quit = 1;
            else handle_input(&u, devs, &e, opt->verbose);
        }
        Uint32 now = SDL_GetTicks();
        label_x_hold(&u, now);
        for (int b = 0; b < B_COUNT; b++)
            if (u.held[b] && repeats((Btn)b) && now >= u.next_rep[b]) {
                u.next_rep[b] = now + 80;
                press(&u, (Btn)b);
            }

        if (now >= next_poll) {   /* MIDI hot-plug: reconnect if unplugged or plugged in late */
            next_poll = now + 1000;
            scan_midi(&u);
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
        if (opt->autosave && ((u.save_due && (Sint32)(now - u.save_due) >= 0) || (Sint32)(now - next_save) >= 0)) {
            u.save_due = 0;
            next_save = now + 60000;
            autosave(&u, opt->autosave);
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
    free(u.saved);
    for (int i = 0; i < MAX_DEVS; i++) close_dev(&devs[i]);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
