/* Pattern generators, scales and evolution. Generators write into a track's
 * step grid (leaving locked steps alone), so results stay editable. */
#ifndef SEQ4H_GEN_H
#define SEQ4H_GEN_H

#include "engine.h"

typedef enum {
    GEN_NONE,       /* manual: nothing is generated */
    GEN_EUCLID,     /* evenly spaced hits */
    GEN_WALK,       /* random walk through the scale */
    GEN_MARKOV,     /* learns note transitions from the track's own phrase */
    GEN_CHAOS,      /* logistic map */
    GEN_CELLULAR,   /* 1-D cellular automaton; advances a generation per roll */
    GEN_WEIGHTED,   /* chance of each scale note, like SEQ-4's stochastic mode */
    GEN_FOLLOW,     /* takes another track's rhythm (and optionally its pitch plus an interval) */
    GEN_ARP,        /* arpeggiates a chord built from the scale */
    GEN_TURING,     /* looping shift register; CHANGE sets how often bits flip per roll */
    GEN_MOTIF,      /* invents a short phrase and develops it (repeat, transpose, invert, reverse) */
    GEN_DRONE,      /* one to four long held notes on a steady tone that moves rarely */
    GEN_DRIFT,      /* a few long notes, each a small step from the last, carried on across rolls */
    GEN_BELL,       /* sparse, soft, chance-played notes on the scale's most stable tones */
    GEN_COUNT
} GenType;

enum { EVO_PITCH = 1, EVO_RHYTHM = 2, EVO_VEL = 4, EVO_ROTATE = 8, EVO_REGEN = 16, EVO_PROB = 32,
       EVO_ALL = 63 };

/* Hand-editing transforms; all leave locked steps where they are. */
typedef enum {
    XF_REVERSE,     /* play the unlocked steps backwards */
    XF_INVERT,      /* mirror pitches around the first sounding note, in scale degrees */
    XF_TRANSPOSE,   /* move every note by `amount` scale degrees */
    XF_SHIFT,       /* rotate the unlocked steps by `amount` (positive = later) */
    XF_THIN,        /* switch off `amount` sounding steps, quietest and offbeat first */
    XF_FILL,        /* switch on `amount` silent steps with notes borrowed from neighbours */
    XF_HUMANIZE,    /* nudge velocities by up to +/- `amount` */
    XF_CLEAR,       /* switch every unlocked step off: a blank track to write on */
    XF_COUNT
} XformType;

enum { SCALE_CHROMATIC = 0, SCALE_MIN_PENT = 9 };

int         scale_count(void);
const char *scale_name(int scale);
int         seq_in_scale(const Seq *s, int note);

/* Notes in the track's scale from gen.base up gen.span octaves. Returns the
 * count (always >= 1). */
int gen_pool(const Seq *s, int t, uint8_t pool[64]);
/* Move `note` by `steps` scale degrees (negative = down), clamped to 0..127. */
int gen_step_note(const Seq *s, int note, int steps);
/* Nearest in-scale note (ties go down). */
int gen_snap_note(const Seq *s, int note);
/* Scale degree of a note counted from MIDI 0, and back. Notes outside the
 * scale take the degree of the in-scale note below them. */
int gen_note_degree(const Seq *s, int note);
int gen_degree_note(const Seq *s, int degree);
/* Number of steps switched on within the track's length. */
int gen_hits(const Seq *s, int t);

const char *gen_name(int type);
int         gen_param_count(int type);
const char *gen_param_name(int type, int k);
int         gen_param_min(int type, int k);
int         gen_param_max(int type, int k);
void        gen_param_text(int type, int k, int value, char out[16]);
void        gen_set_type(Seq *s, int t, int type);          /* resets params to defaults */
void        gen_load_settings(GenSettings *g, int type, int p0, int p1, int p2, int p3, int base, int span);

/* Write a new pattern into track t (unlocked steps only). */
void gen_apply(Seq *s, int t);
/* Step the generator's own state forward (cellular: next generation), then apply. */
void gen_roll(Seq *s, int t);
/* One evolution tick, called at bar boundaries. */
void gen_evolve_bar(Seq *s, int t);

/* Remember the track's current pattern as the anchor evolution returns to. */
void gen_set_anchor(Seq *s, int t);
void gen_clear_anchor(Seq *s, int t);

/* Each unlocked step that differs from target[] becomes target[i] with chance
 * pct (its lock flag is kept). Used by the anchor pull and by chain morphing. */
void gen_morph(Seq *s, int t, const SeqStep *target, int pct);

/* Styles set generator defaults and can lay out a whole starting project. */
/* STYLE_BLANK is "no style": table generator defaults, and NEW FROM STYLE
 * gives four empty MANUAL tracks to build from scratch. */
typedef enum { STYLE_AMBIENT, STYLE_TECHNO, STYLE_MELODIC, STYLE_BLANK, STYLE_COUNT } Style;
int         style_count(void);
const char *style_name(int style);
/* Set up the style's starting set: tempo, scale, and for each track its
 * generator, length, gates, holds, evolution and anchor. With keep_manual,
 * a MANUAL track that has notes is left exactly as it is (it's your own
 * pattern), and if any track is kept the scale and root stay too, so the new
 * tracks fit it. Returns how many tracks were kept.
 * STYLE_BLANK clears all four tracks (keep_manual is ignored): MANUAL, 16
 * steps, every step off, evolution off, no anchor, chaining off, unmuted.
 * Tempo, swing, scale, root, MIDI channels and stored slots stay as they are. */
int         style_apply(Seq *s, int style, int keep_manual);
/* Short generator name for the grid ("MANUAL", "EUCLID", "CA", ...), <= 6 chars. */
const char *gen_short_name(int type);

/* Chords stacked on each note a track plays (SeqTrack.chord), so a drone
 * track can hold a pad. 0 = single notes. */
enum { CHORD_OFF, CHORD_FIFTH, CHORD_TRIAD, CHORD_SUS2, CHORD_SUS4, CHORD_SEVENTH, CHORD_OPEN, CHORD_COUNT };
const char *gen_chord_name(int chord);
/* The extra notes the chord adds above `note` (in the scale, ascending, never
 * `note` itself, all <= 127). Returns how many (0..SEQ_CHORD_MAX). */
int gen_chord_notes(const Seq *s, int note, int chord, uint8_t out[SEQ_CHORD_MAX]);

const char *gen_xform_name(int op);
void gen_transform(Seq *s, int t, int op, int amount);

#endif
