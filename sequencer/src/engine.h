/* seq4h engine: four step-sequencer tracks, driven by a tick counter.
 * Pure C, no I/O except through the send callback, so it can be unit-tested. */
#ifndef SEQ4H_ENGINE_H
#define SEQ4H_ENGINE_H

#include <stdint.h>
#include <stdio.h>

#define SEQ_TRACKS 4
#define SEQ_STEPS 32               /* longest pattern */
#define SEQ_DEFAULT_LEN 16         /* new tracks start one bar long */
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
    uint8_t hold;    /* 0..SEQ_MAX_HOLD extra whole steps the note sustains for (ties), unless
                        the track's next note cuts it off */
} SeqStep;
#define SEQ_MAX_HOLD 31

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
    uint8_t min_hits;/* evolution keeps at least this many steps on (0..32) */
    uint8_t max_hits;/* ...and at most this many; 0 = no maximum */
} EvolveSettings;

#define SEQ_SLOTS 8   /* stored patterns per track */
#define SEQ_SLOT_NAME 8   /* characters in a slot name */

typedef struct {
    SeqStep step[SEQ_STEPS];
    uint8_t len;
    uint8_t used;       /* 0 = empty slot */
    char name[SEQ_SLOT_NAME + 1];   /* "" = default ("SLOT 3"); A-Z, 0-9, space, '-' */
} SeqSlot;

typedef struct {
    SeqStep step[SEQ_STEPS];
    uint8_t len;        /* 1..SEQ_STEPS */
    uint8_t channel;    /* 0..15 (shown as 1..16) */
    uint8_t mute;
    uint8_t solo;       /* while any track is soloed, only soloed tracks play (not saved) */
    GenSettings gen;
    EvolveSettings evo;
    SeqStep anchor[SEQ_STEPS];  /* pattern evolution is pulled back towards */
    uint8_t has_anchor;
    SeqSlot slot[SEQ_SLOTS];
    uint8_t cur_slot;           /* slot last stored or recalled */
    uint8_t chain[SEQ_SLOTS];   /* slot numbers to play in order */
    uint8_t chain_len;          /* 0 = chaining off */
    uint8_t chain_pos;          /* index into chain[] */
    uint8_t chain_bars;         /* bars per chain link: 1, 2, 4, 8 or 16 */
    uint8_t chain_morph;        /* 0 = jump to the next slot; else % chance per bar each step moves towards it */
    int16_t sounding;   /* note currently held, or -1 */
    int16_t last_note;  /* last note sent on this track, or -1 (seq_panic re-sends its note-off) */
    uint32_t off_tick;
} SeqTrack;

typedef void (*SeqSend)(void *ctx, const uint8_t *msg, int len);

typedef struct {
    SeqTrack track[SEQ_TRACKS];
    int bpm;            /* 30..300 */
    int swing;          /* 0..SEQ_MAX_SWING ticks */
    int scale;          /* index into the scale list (see gen.h) */
    int root;           /* 0..11, C = 0 */
    int clock_out;      /* send MIDI clock/start/stop; as clock master, clock also runs while stopped */
    int style;          /* Style (gen.h): sets generator defaults; STYLE_AMBIENT by default */
    int playing;
    uint32_t tick;      /* next tick to process */
    uint32_t idle_tick; /* tick counter while stopped, for the free-running clock */
    int resync;         /* send Stop+Start at the next bar boundary (after a MIDI reconnect) */
    uint32_t rng;
    SeqSend send;
    void *ctx;
} Seq;

void seq_init(Seq *s, SeqSend send, void *ctx);
uint32_t seq_rand(Seq *s);              /* deterministic xorshift */
void seq_load_demo(Seq *s);

void seq_start(Seq *s);
void seq_stop(Seq *s);
/* Silence the receiver: note-off for every held and last-played note, then
 * All Sound Off (CC120) and All Notes Off (CC123) on each track's channel. */
void seq_panic(Seq *s);
/* After the MIDI device comes back: panic now, keep playing in place, and
 * re-send Start at the next bar line so the receiver's clock is back in phase
 * without the pattern jumping back to step 1. */
void seq_reconnected(Seq *s);
void seq_tick(Seq *s);                 /* advance one tick (1/96 beat) */
int  seq_playhead(const Seq *s, int t);/* step being played on track t, or -1 */

void seq_step_toggle(Seq *s, int t, int i);
/* Adjust step parameters; a note change also switches the step on. */
void seq_step_adjust(Seq *s, int t, int i, int dnote, int dvel, int dgate, int dprob);
void seq_step_hold(Seq *s, int t, int i, int delta);   /* change a step's hold, 0..SEQ_MAX_HOLD */
void seq_track_mute_toggle(Seq *s, int t);
/* Solo: while any track is soloed, only soloed, unmuted tracks play. Several
 * tracks can be soloed at once. Notes held by tracks that go silent are
 * released straight away. */
void seq_track_solo_toggle(Seq *s, int t);
int  seq_any_solo(const Seq *s);
int  seq_track_audible(const Seq *s, int t);   /* not muted, and soloed if any track is */
void seq_track_len(Seq *s, int t, int delta);
void seq_track_channel(Seq *s, int t, int delta);
void seq_adjust_bpm(Seq *s, int delta);
void seq_adjust_swing(Seq *s, int delta);
/* Change scale and root. With snap, every step's note moves to the nearest
 * note of the new scale (locked steps included, since they would sound wrong). */
void seq_set_scale(Seq *s, int scale, int root, int snap);

/* Pattern slots. Store copies the track's live pattern into slot k; recall
 * copies it back (returns -1 for an empty slot). */
void seq_slot_store(Seq *s, int t, int k);
int  seq_slot_recall(Seq *s, int t, int k);
/* Empty slot k and reset its name. */
void seq_slot_clear(Seq *s, int t, int k);
/* Copy slot `from` (pattern and name) over slot `to`; -1 if `from` is empty. */
int  seq_slot_copy(Seq *s, int t, int from, int to);
/* Name a slot: lower case becomes upper case, other characters become spaces,
 * trailing spaces go, and the default name ("SLOT 3") is stored as "". */
void seq_slot_set_name(Seq *s, int t, int k, const char *name);
/* The name to show: the slot's own name, or "SLOT 3". out holds SEQ_SLOT_NAME + 1. */
void seq_slot_name(const Seq *s, int t, int k, char *out);
/* Chain: play slots seq[0..n-1] in turn, `bars` bars each. morph 0 jumps,
 * otherwise steps drift towards the next slot (morph = % chance per bar). */
void seq_chain_set(Seq *s, int t, const uint8_t *seq, int n, int bars, int morph);
/* Called at every bar boundary by seq_tick (bar counts from 1). */
void seq_chain_bar(Seq *s, int t, uint32_t bar);

void seq_note_name(int note, char out[8]);   /* 60 -> "C4" */

void seq_save(const Seq *s, FILE *f);
int  seq_load(Seq *s, FILE *f);              /* 0 on success */
/* Save to `path` safely: write path.tmp, sync it to disk, then rename it over
 * path. Returns 0 on success. */
int  seq_save_file(const Seq *s, const char *path);
int  seq_slots_used(const Seq *s);           /* stored slots across all tracks */

#endif
