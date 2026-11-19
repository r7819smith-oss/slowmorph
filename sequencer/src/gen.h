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
    GEN_COUNT
} GenType;

enum { EVO_PITCH = 1, EVO_RHYTHM = 2, EVO_VEL = 4, EVO_ROTATE = 8, EVO_REGEN = 16 };

enum { SCALE_CHROMATIC = 0, SCALE_MIN_PENT = 9 };

int         scale_count(void);
const char *scale_name(int scale);
int         seq_in_scale(const Seq *s, int note);

/* Notes in the track's scale from gen.base up gen.span octaves. Returns the
 * count (always >= 1). */
int gen_pool(const Seq *s, int t, uint8_t pool[64]);
/* Move `note` by `steps` scale degrees (negative = down), clamped to 0..127. */
int gen_step_note(const Seq *s, int note, int steps);

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

#endif
