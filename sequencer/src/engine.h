/* seq4h engine: four step-sequencer tracks, driven by a tick counter.
 * Pure C, no I/O except through the send callback, so it can be unit-tested. */
#ifndef SEQ4H_ENGINE_H
#define SEQ4H_ENGINE_H

#include <stdint.h>
#include <stdio.h>

#define SEQ_TRACKS 4
#define SEQ_STEPS 16
#define SEQ_PPQN 96                 /* engine ticks per quarter note */
#define SEQ_TICKS_PER_STEP 24       /* one step = a 1/16 note */
#define SEQ_MAX_SWING 12            /* ticks the odd steps are delayed by */

typedef struct {
    uint8_t on;      /* 0 or 1 */
    uint8_t note;    /* 0..127 */
    uint8_t vel;     /* 1..127 */
    uint8_t gate;    /* 5..100, percent of a step */
    uint8_t prob;    /* 0..100, percent chance of playing */
    uint8_t lock;    /* 1 = generators and evolution leave this step alone */
} SeqStep;

/* Pattern generator for a track. Generators write into the step grid, so the
 * result can be edited by hand afterwards. See gen.h. */
typedef struct {
    uint8_t type;    /* GenType; 0 = manual */
    uint8_t p[4];    /* generator-specific parameters */
    uint8_t base;    /* lowest MIDI note generators use */
    uint8_t span;    /* octaves above base, 1..4 */
    uint32_t state;  /* generator memory kept between rolls (TURING register) */
} GenSettings;

/* Evolution: every `every` bars, change the pattern a little. */
typedef struct {
    uint8_t every;   /* 0 = off, else 1, 2, 4 or 8 bars */
    uint8_t amount;  /* 0..100, percent chance each step changes */
    uint8_t kinds;   /* EVO_* bit flags */
    uint8_t pull;    /* 0..100, chance per bar that a changed step returns to the anchor */
    uint8_t min_hits;/* evolution keeps at least this many steps on (0..16) */
    uint8_t max_hits;/* ...and at most this many; 0 = no maximum */
} EvolveSettings;

typedef struct {
    SeqStep step[SEQ_STEPS];
    uint8_t len;        /* 1..16 */
    uint8_t channel;    /* 0..15 (shown as 1..16) */
    uint8_t mute;
    GenSettings gen;
    EvolveSettings evo;
    SeqStep anchor[SEQ_STEPS];  /* pattern evolution is pulled back towards */
    uint8_t has_anchor;
    int16_t sounding;   /* note currently held, or -1 */
    uint32_t off_tick;
} SeqTrack;

typedef void (*SeqSend)(void *ctx, const uint8_t *msg, int len);

typedef struct {
    SeqTrack track[SEQ_TRACKS];
    int bpm;            /* 30..300 */
    int swing;          /* 0..SEQ_MAX_SWING ticks */
    int scale;          /* index into the scale list (see gen.h) */
    int root;           /* 0..11, C = 0 */
    int clock_out;      /* send MIDI clock/start/stop */
    int playing;
    uint32_t tick;      /* next tick to process */
    uint32_t rng;
    SeqSend send;
    void *ctx;
} Seq;

void seq_init(Seq *s, SeqSend send, void *ctx);
uint32_t seq_rand(Seq *s);              /* deterministic xorshift */
void seq_load_demo(Seq *s);

void seq_start(Seq *s);
void seq_stop(Seq *s);
void seq_tick(Seq *s);                 /* advance one tick (1/96 beat) */
int  seq_playhead(const Seq *s, int t);/* step being played on track t, or -1 */

void seq_step_toggle(Seq *s, int t, int i);
/* Adjust step parameters; a note change also switches the step on. */
void seq_step_adjust(Seq *s, int t, int i, int dnote, int dvel, int dgate, int dprob);
void seq_track_mute_toggle(Seq *s, int t);
void seq_track_len(Seq *s, int t, int delta);
void seq_track_channel(Seq *s, int t, int delta);
void seq_adjust_bpm(Seq *s, int delta);
void seq_adjust_swing(Seq *s, int delta);
/* Change scale and root. With snap, every step's note moves to the nearest
 * note of the new scale (locked steps included, since they would sound wrong). */
void seq_set_scale(Seq *s, int scale, int root, int snap);

void seq_note_name(int note, char out[8]);   /* 60 -> "C4" */

void seq_save(const Seq *s, FILE *f);
int  seq_load(Seq *s, FILE *f);              /* 0 on success */

#endif
