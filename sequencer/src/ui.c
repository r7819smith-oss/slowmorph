#include "ui.h"

#include "gen.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define W 640
#define H 480
#define X0 56
#define CELL 35
#define ROW_Y0 70
#define ROW_H 64

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

typedef enum { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_X, B_Y, B_L, B_R, B_SEL, B_START,
               B_PAGE, B_ROLL, B_UNDO, B_LOCK,   /* virtual: Select+X/B/Y/A, keys E/R/U/K, L2/R2 */
               B_COUNT } Btn;

typedef struct {
    Player *p;
    int track, step;
    int held[B_COUNT];
    Uint32 next_rep[B_COUNT];
    int quit;
    int midi_up;              /* last MIDI poll result; -1 = not polled yet */
    int page;                 /* 0 = grid, 1 = generate/evolve */
    int row;                  /* selected row on the generate page */
    SeqStep undo[SEQ_STEPS];  /* the track as it was before the last roll */
    int undo_track;           /* -1 = nothing to undo */
    const char *msg;
    Uint32 msg_until;
} Ui;

#define GEN_ROWS 17
static const int EVERY[5] = {0, 1, 2, 4, 8};

static int wrap(int v, int n) { return (v % n + n) % n; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }


static void gen_row_text(const Seq *s, int t, int row, char *label, char *val) {
    const SeqTrack *tr = &s->track[t];
    const GenSettings *g = &tr->gen;
    static const char *EVO_NAMES[5] = {"EVO PITCH", "EVO RHYTHM", "EVO VELOCITY", "EVO ROTATE", "EVO REGEN"};
    label[0] = val[0] = 0;
    if (row == 0) { snprintf(label, 24, "TRACK"); snprintf(val, 24, "%d", t + 1); }
    else if (row == 1) { snprintf(label, 24, "GENERATOR"); snprintf(val, 24, "%s", gen_name(g->type)); }
    else if (row <= 5) {
        int k = row - 2;
        if (k < gen_param_count(g->type)) {
            snprintf(label, 24, "  %s", gen_param_name(g->type, k));
            char tmp[16];
            gen_param_text(g->type, k, g->p[k], tmp);
            snprintf(val, 24, "%s", tmp);
        } else {
            snprintf(label, 24, "  -");
        }
    }
    else if (row == 6) { snprintf(label, 24, "LOWEST NOTE"); char nn[8]; seq_note_name(g->base, nn); snprintf(val, 24, "%s", nn); }
    else if (row == 7) { snprintf(label, 24, "RANGE"); snprintf(val, 24, "%d OCT", g->span); }
    else if (row == 8) {
        snprintf(label, 24, "EVOLVE");
        if (tr->evo.every) snprintf(val, 24, "EVERY %d BAR%s", tr->evo.every, tr->evo.every > 1 ? "S" : "");
        else snprintf(val, 24, "OFF");
    }
    else if (row == 9) { snprintf(label, 24, "  AMOUNT"); snprintf(val, 24, "%d%%", tr->evo.amount); }
    else if (row <= 14) { snprintf(label, 24, "  %s", EVO_NAMES[row - 10]); snprintf(val, 24, "%s", (tr->evo.kinds >> (row - 10)) & 1 ? "ON" : "OFF"); }
    else if (row == 15) { snprintf(label, 24, "SCALE"); snprintf(val, 24, "%s", scale_name(s->scale)); }
    else { static const char *R[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
           snprintf(label, 24, "ROOT"); snprintf(val, 24, "%s", R[s->root % 12]); }
}

static void gen_row_adjust(Seq *s, Ui *u, int row, int d, int big) {
    SeqTrack *tr = &s->track[u->track];
    GenSettings *g = &tr->gen;
    if (row == 0) u->track = wrap(u->track + d, SEQ_TRACKS);
    else if (row == 1) gen_set_type(s, u->track, wrap(g->type + d, GEN_COUNT));
    else if (row <= 5) {
        int k = row - 2;
        if (k < gen_param_count(g->type))
            g->p[k] = (uint8_t)clampi(g->p[k] + d * (big ? 5 : 1), gen_param_min(g->type, k), gen_param_max(g->type, k));
    }
    else if (row == 6) g->base = (uint8_t)clampi(g->base + d * (big ? 12 : 1), 0, 115);
    else if (row == 7) g->span = (uint8_t)clampi(g->span + d, 1, 4);
    else if (row == 8) {
        int i = 0;
        while (i < 4 && EVERY[i] != tr->evo.every) i++;
        tr->evo.every = (uint8_t)EVERY[clampi(i + d, 0, 4)];
    }
    else if (row == 9) tr->evo.amount = (uint8_t)clampi(tr->evo.amount + 5 * d * (big ? 2 : 1), 0, 100);
    else if (row <= 14) tr->evo.kinds ^= (uint8_t)(1 << (row - 10));
    else if (row == 15) s->scale = wrap(s->scale + d, scale_count());
    else s->root = wrap(s->root + d, 12);
}

static void say(Ui *u, const char *m) { u->msg = m; u->msg_until = SDL_GetTicks() + 2500; }

static void press(Ui *u, Btn b) {
    Player *p = u->p;
    if (u->held[B_SEL]) {                       /* Select + face button = special actions */
        if (b == B_X) b = B_PAGE;
        else if (b == B_B) b = B_ROLL;
        else if (b == B_Y) b = B_UNDO;
        else if (b == B_A) b = B_LOCK;
    }
    if (u->page == 1 && b == B_A) b = B_ROLL;
    int dx = (b == B_RIGHT) - (b == B_LEFT), dy = (b == B_DOWN) - (b == B_UP);
    player_lock(p);
    Seq *s = &p->seq;
    if (b == B_START && u->held[B_SEL]) { u->quit = 1; }
    else if (b == B_START) { s->playing ? seq_stop(s) : seq_start(s); }
    else if (b == B_PAGE) { u->page ^= 1; }
    else if (b == B_ROLL) {
        if (s->track[u->track].gen.type == GEN_NONE) {
            say(u, "NO GENERATOR: SELECT+X, PICK ONE");
        } else {
            memcpy(u->undo, s->track[u->track].step, sizeof u->undo);
            u->undo_track = u->track;
            gen_roll(s, u->track);
            say(u, "ROLLED");
        }
    }
    else if (b == B_UNDO) {
        if (u->undo_track >= 0) {
            memcpy(s->track[u->undo_track].step, u->undo, sizeof u->undo);
            u->undo_track = -1;
            say(u, "UNDONE");
        } else {
            say(u, "NOTHING TO UNDO");
        }
    }
    else if (b == B_LOCK) { s->track[u->track].step[u->step].lock ^= 1; }
    else if (b == B_A) { seq_step_toggle(s, u->track, u->step); }
    else if (b == B_X) { seq_track_mute_toggle(s, u->track); }
    else if (b == B_L || b == B_R) {
        int d = b == B_R ? 1 : -1;
        if (u->held[B_Y]) seq_adjust_swing(s, d);
        else seq_adjust_bpm(s, u->held[B_B] ? 10 * d : d);
    } else if (u->page == 1) {
        if (dy) u->row = wrap(u->row + dy, GEN_ROWS);
        else if (dx) gen_row_adjust(s, u, u->row, dx, u->held[B_B]);
    } else if (dx || dy) {
        if (u->held[B_B]) seq_step_adjust(s, u->track, u->step, -dy, 8 * dx, 0, 0);
        else if (u->held[B_Y]) seq_step_adjust(s, u->track, u->step, 0, 0, -10 * dy, 10 * dx);
        else if (u->held[B_SEL]) { if (dx) seq_track_len(s, u->track, dx); else seq_track_channel(s, u->track, -dy); }
        else { u->step = wrap(u->step + dx, SEQ_STEPS); u->track = wrap(u->track + dy, SEQ_TRACKS); }
    }
    player_unlock(p);
}

static int repeats(Btn b) { return b <= B_RIGHT || b == B_L || b == B_R; }  /* d-pad and shoulders auto-repeat */

static void set_button(Ui *u, Btn b, int down) {
    if (down && !u->held[b]) {
        u->held[b] = 1;
        u->next_rep[b] = SDL_GetTicks() + 350;
        press(u, b);
    } else if (!down) {
        u->held[b] = 0;
    }
}

static int map_key(SDL_Keycode k, Btn *b) {
    switch (k) {
    case SDLK_UP: *b = B_UP; return 1;       case SDLK_DOWN: *b = B_DOWN; return 1;
    case SDLK_LEFT: *b = B_LEFT; return 1;   case SDLK_RIGHT: *b = B_RIGHT; return 1;
    case SDLK_z: *b = B_A; return 1;         case SDLK_x: *b = B_B; return 1;
    case SDLK_c: *b = B_X; return 1;         case SDLK_v: *b = B_Y; return 1;
    case SDLK_q: *b = B_L; return 1;         case SDLK_w: *b = B_R; return 1;
    case SDLK_TAB: *b = B_SEL; return 1;     case SDLK_SPACE: *b = B_START; return 1;
    case SDLK_e: *b = B_PAGE; return 1;      case SDLK_r: *b = B_ROLL; return 1;
    case SDLK_u: *b = B_UNDO; return 1;      case SDLK_k: *b = B_LOCK; return 1;
    default: return 0;
    }
}

static int map_pad(Uint8 k, Btn *b) {
    switch (k) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: *b = B_UP; return 1;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: *b = B_DOWN; return 1;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: *b = B_LEFT; return 1;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: *b = B_RIGHT; return 1;
    case SDL_CONTROLLER_BUTTON_A: *b = B_A; return 1;
    case SDL_CONTROLLER_BUTTON_B: *b = B_B; return 1;
    case SDL_CONTROLLER_BUTTON_X: *b = B_X; return 1;
    case SDL_CONTROLLER_BUTTON_Y: *b = B_Y; return 1;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: *b = B_L; return 1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: *b = B_R; return 1;
    case SDL_CONTROLLER_BUTTON_BACK: *b = B_SEL; return 1;
    case SDL_CONTROLLER_BUTTON_START: *b = B_START; return 1;
    default: return 0;
    }
}

static void draw_header(SDL_Renderer *r, const Seq *s, const Ui *u, const char *midi_desc) {
    char buf[160];
    text(r, 20, 14, 3, WHITE, "SEQ4H");
    snprintf(buf, sizeof buf, "BPM %d", s->bpm);
    text(r, 150, 18, 2, WHITE, buf);
    snprintf(buf, sizeof buf, "SWING %d", s->swing);
    text(r, 270, 18, 2, DIM, buf);
    text(r, 440, 18, 2, s->playing ? (Col){90, 220, 120} : DIM, s->playing ? "PLAYING" : "STOPPED");
    snprintf(buf, sizeof buf, "MIDI: %.30s", midi_desc ? midi_desc : "NONE");
    text(r, 20, 46, 1, DIM, buf);
    static const char *R[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    snprintf(buf, sizeof buf, "%s %s", R[s->root % 12], scale_name(s->scale));
    text(r, 260, 46, 1, DIM, buf);
    if (u->msg && SDL_GetTicks() < u->msg_until) text(r, 400, 46, 1, YELLOW, u->msg);
}

static void draw_grid(SDL_Renderer *r, const Seq *sp, const Ui *u) {
    const Seq s = *sp;
    char buf[160];
    for (int t = 0; t < SEQ_TRACKS; t++) {
        const SeqTrack *tr = &s.track[t];
        int y = ROW_Y0 + t * ROW_H;
        Col base = tr->mute ? (Col){90, 90, 95} : TRACK_COL[t];
        snprintf(buf, sizeof buf, "T%d", t + 1);
        text(r, 12, y + 4, 2, t == u->track ? YELLOW : base, buf);
        snprintf(buf, sizeof buf, "C%d", tr->channel + 1);
        text(r, 12, y + 24, 1, DIM, buf);
        if (tr->mute) text(r, 12, y + 38, 1, DIM, "MUTE");
        else if (tr->gen.type != GEN_NONE) text(r, 12, y + 38, 1, scale(base, 70), tr->evo.every ? "EVOLVE" : "GEN");
        int head = seq_playhead(&s, t);
        for (int i = 0; i < SEQ_STEPS; i++) {
            int x = X0 + i * CELL, w = CELL - 4, h = ROW_H - 8;
            if (i >= tr->len) { frame(r, x, y, w, h, 1, (Col){40, 42, 52}); continue; }
            rect(r, x, y, w, h, (i / 4) % 2 ? (Col){38, 40, 52} : (Col){30, 32, 42});
            const SeqStep *st = &tr->step[i];
            if (st->on) {
                rect(r, x + 3, y + 3, w - 6, h - 6, scale(base, 35 + 65 * st->vel / 127));
                if (st->prob < 100) rect(r, x + 3, y + 3, (w - 6) * st->prob / 100, 3, WHITE);
                rect(r, x + 3, y + h - 6, (w - 6) * st->gate / 100, 3, scale(WHITE, 70));
            }
            if (st->lock) rect(r, x + w - 9, y + 2, 6, 6, YELLOW);
            if (i == head) frame(r, x - 1, y - 1, w + 2, h + 2, 2, WHITE);
            if (t == u->track && i == u->step) frame(r, x - 2, y - 2, w + 4, h + 4, 2, YELLOW);
        }
    }

    const SeqTrack *tr = &s.track[u->track];
    const SeqStep *st = &tr->step[u->step];
    char nn[8];
    seq_note_name(st->note, nn);
    int y = ROW_Y0 + SEQ_TRACKS * ROW_H + 10;
    snprintf(buf, sizeof buf, "T%d STEP %02d %s%s  LEN %d  CH %d", u->track + 1, u->step + 1, st->on ? "ON" : "OFF", st->lock ? " LOCKED" : "", tr->len, tr->channel + 1);
    text(r, 20, y, 2, WHITE, buf);
    snprintf(buf, sizeof buf, "NOTE %s  VEL %d  GATE %d  PROB %d", nn, st->vel, st->gate, st->prob);
    text(r, 20, y + 22, 2, WHITE, buf);
    if (tr->evo.every) snprintf(buf, sizeof buf, "GEN %s  EVOLVE EVERY %d BAR%s %d%%", gen_name(tr->gen.type), tr->evo.every, tr->evo.every > 1 ? "S" : "", tr->evo.amount);
    else snprintf(buf, sizeof buf, "GEN %s  EVOLVE OFF", gen_name(tr->gen.type));
    text(r, 20, y + 44, 2, DIM, buf);

    text(r, 20, H - 62, 1, DIM, "A STEP  X MUTE  START PLAY/STOP  SELECT+START QUIT");
    text(r, 20, H - 48, 1, DIM, "B+UP/DOWN NOTE  B+LEFT/RIGHT VEL  Y+UP/DOWN GATE  Y+LEFT/RIGHT PROB");
    text(r, 20, H - 34, 1, DIM, "SELECT+LEFT/RIGHT LENGTH  SELECT+UP/DOWN CHANNEL  L/R BPM  Y+L/R SWING");
    text(r, 20, H - 20, 1, DIM, "SELECT+A LOCK STEP  SELECT+B ROLL  SELECT+Y UNDO  SELECT+X GENERATE PAGE");
}

static void draw_gen(SDL_Renderer *r, const Seq *sp, const Ui *u) {
    const Seq s = *sp;
    char label[24], val[24];
    for (int row = 0; row < GEN_ROWS; row++) {
        int y = 62 + row * 22;
        gen_row_text(&s, u->track, row, label, val);
        int sel = row == u->row;
        if (sel) rect(r, 14, y - 3, 352, 20, (Col){40, 44, 64});
        text(r, 22, y, 2, sel ? YELLOW : (row == 0 || row == 1 || row == 8 || row == 15 ? WHITE : DIM), label);
        text(r, 215, y, 2, sel ? YELLOW : WHITE, val);
    }
    /* live preview of the selected track: height = pitch, brightness = velocity */
    const SeqTrack *tr = &s.track[u->track];
    Col base = TRACK_COL[u->track];
    int px = 392, py = 70, pw = 15, ph = 150;
    text(r, px, py - 14, 1, DIM, "PATTERN");
    rect(r, px - 4, py - 2, 16 * pw + 6, ph + 4, (Col){24, 26, 36});
    int lo = tr->gen.base, hi = tr->gen.base + 12 * tr->gen.span;
    int head = seq_playhead(&s, u->track);
    for (int i = 0; i < tr->len; i++) {
        const SeqStep *st = &tr->step[i];
        if (st->on) {
            int n = clampi(st->note, lo, hi);
            int yy = py + ph - 8 - (n - lo) * (ph - 8) / (hi > lo ? hi - lo : 1);
            rect(r, px + i * pw, yy, pw - 3, 8, scale(base, 35 + 65 * st->vel / 127));
        }
        if (st->lock) rect(r, px + i * pw, py + ph - 2, pw - 3, 2, YELLOW);
        if (i == head) frame(r, px + i * pw - 1, py, pw - 1, ph, 1, WHITE);
    }
    text(r, 392, 240, 1, DIM, "GENERATORS WRITE INTO THE GRID, SO");
    text(r, 392, 252, 1, DIM, "YOU CAN EDIT THE RESULT. LOCKED STEPS");
    text(r, 392, 264, 1, DIM, "(SELECT+A ON THE GRID) ARE NEVER");
    text(r, 392, 276, 1, DIM, "CHANGED BY ROLL OR EVOLVE.");
    text(r, 392, 300, 1, DIM, "EVOLVE CHANGES THE PATTERN EVERY N");
    text(r, 392, 312, 1, DIM, "BARS: AMOUNT = CHANCE PER STEP.");
    text(r, 392, 324, 1, DIM, "REGEN RE-RUNS THE GENERATOR.");
    text(r, 20, H - 34, 1, DIM, "UP/DOWN ROW  LEFT/RIGHT VALUE (HOLD B = BIG STEP)  A ROLL NEW PATTERN");
    text(r, 20, H - 20, 1, DIM, "SELECT+Y UNDO ROLL  SELECT+X BACK TO GRID  START PLAY/STOP");
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
    if (u->page == 1) draw_gen(r, &s, u);
    else draw_grid(r, &s, u);
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

    SDL_GameController *pads[8] = {0};
    for (int i = 0; i < SDL_NumJoysticks() && i < 8; i++)
        if (SDL_IsGameController(i)) pads[i] = SDL_GameControllerOpen(i);

    Ui u = {.p = p, .undo_track = -1, .midi_up = -1};
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
    Uint32 next_poll = 0;
    while (!u.quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            Btn b;
            if (e.type == SDL_QUIT) u.quit = 1;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) u.quit = 1;
            else if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat && map_key(e.key.keysym.sym, &b))
                set_button(&u, b, e.type == SDL_KEYDOWN);
            else if ((e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) && map_pad(e.cbutton.button, &b))
                set_button(&u, b, e.type == SDL_CONTROLLERBUTTONDOWN);
            else if (e.type == SDL_CONTROLLERAXISMOTION &&
                     (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
                set_button(&u, e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? B_PAGE : B_ROLL, e.caxis.value > 16000);
            else if (e.type == SDL_CONTROLLERDEVICEADDED && e.cdevice.which < 8 && !pads[e.cdevice.which])
                pads[e.cdevice.which] = SDL_GameControllerOpen(e.cdevice.which);
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
                /* Reconnected while playing: the FM1 missed Start, so restart from
                 * step 1 with a fresh Start to bring its clock back in phase. */
                if (up && u.midi_up == 0 && p->seq.playing) { seq_stop(&p->seq); seq_start(&p->seq); }
                u.midi_up = up;
            }
            player_unlock(p);
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

    for (int i = 0; i < 8; i++) if (pads[i]) SDL_GameControllerClose(pads[i]);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
