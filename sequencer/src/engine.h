/* seq4h engine: eight step-sequencer tracks, driven by a tick counter.
 * Pure C, no I/O except through the send callback, so it can be unit-tested. */
#ifndef SEQ4H_ENGINE_H
#define SEQ4H_ENGINE_H

#include <stdint.h>
#include <stdio.h>

#include "mod.h"

#define SEQ_TRACKS 8
#define SEQ_BANK 4                 /* tracks on one GRID page: T1-T4, then T5-T8. Felucca plays
                                      T1-T4 (channels 1-4); T5-T8 start on channels 5-8 for a
                                      second instrument (Blackbox 2, Digitone ...) */
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
    int8_t nudge;    /* -SEQ_MAX_NUDGE..SEQ_MAX_NUDGE ticks: plays this much early (-) or late (+) */
    int8_t more[3];  /* the step's other notes (a chord), in scale degrees above (+) or below (-) `note`;
                        0 = unused, the used ones come first. They follow `note` when it moves (see seq_step_notes).
                        Only a POLY track plays them. */
    uint8_t ratchet; /* 0 or 1 = the note plays once; 2..SEQ_MAX_RATCHET = it repeats that many times inside its step */
    uint8_t cond;    /* trig condition (COND_*): which passes of the pattern the step plays on; 0 = every pass */
} SeqStep;
#define SEQ_MAX_RATCHET 8
/* Trig conditions. A:B plays on pass A of every B passes of the track's
 * pattern (1:2 = passes 1, 3, 5 ...; 2:4 = passes 2, 6, 10 ...). FIRST plays
 * only on the first pass after Start (or a REALIGN that moved the track);
 * PRE plays when the track's last A:B / FIRST step did, NOT PRE when it
 * didn't, so two steps can take turns. Probability still applies on top. */
enum { COND_ALWAYS = 0, COND_AB_FIRST = 1, COND_AB_LAST = 35,   /* 1:2 2:2 1:3 2:3 3:3 1:4 ... 8:8 */
       COND_FIRST, COND_NOT_FIRST, COND_PRE, COND_NOT_PRE, COND_COUNT };
void seq_cond_name(int cond, char out[12]);   /* "ALWAYS", "1:2", "NOT FIRST" ... */
/* A:B of an A:B condition (returns 1), or 0 for the others. */
int  seq_cond_ab(int cond, int *a, int *b);
int  seq_cond_make(int a, int b);             /* the A:B condition, COND_ALWAYS if out of range */
#define SEQ_POLY 4         /* notes one step can hold: its own note and up to 3 more */
#define SEQ_POLY_VOICES 8  /* notes a POLY track keeps sounding at once (Felucca has 8 voices in all) */
#define SEQ_MORE_MAX 35    /* furthest a chord note sits from the step's note, in scale degrees (5 octaves) */
#define SEQ_MAX_HOLD 31
#define SEQ_MAX_NUDGE 11   /* just under half a step either way */
#define SEQ_OFFGRID_MIN 2  /* NEVER ON GRID keeps a note at least this many ticks away from the grid */

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
    uint8_t every;   /* 0 = off, else 1, 2, 4, 8, 16, 32 or 64 bars */
    uint8_t amount;  /* 0..100, percent chance each step changes */
    uint8_t kinds;   /* EVO_* bit flags */
    uint8_t pull;    /* 0..100, chance per bar that a changed step returns to the anchor */
    uint8_t min_hits;/* evolution keeps at least this many steps on (0..32) */
    uint8_t max_hits;/* ...and at most this many; 0 = no maximum */
} EvolveSettings;

#define SEQ_SLOTS 8   /* stored patterns per track */
#define SEQ_CHORD_MAX 3   /* notes a chord adds on top of the step's own note */
#define SEQ_MAX_EVERY 64  /* longest evolution interval, in bars */
#define SEQ_SLOT_NAME 8   /* characters in a slot name */

/* Drum tracks (SeqTrack.type = TRACK_DRUMS): instead of one note per step,
 * 8 lanes, each with its own hits; every lane plays its own MIDI note (the
 * DRUM MAP, Seq.lane_note), so a kick and a hat can sound on the same step.
 * See drum.h. */
#define SEQ_LANES 8
#define SEQ_LANE_NAME 8   /* characters in a lane name */
enum { TRACK_NOTES, TRACK_DRUMS };
#define SEQ_DRUM_GATE 12  /* ticks a drum hit's note is held (half a step) */
typedef struct {
    uint8_t vel[SEQ_LANES][SEQ_STEPS];    /* 0 = no hit, else the hit's velocity 1..127 */
    uint8_t prob[SEQ_LANES][SEQ_STEPS];   /* chance a hit plays, 0..100 */
    uint8_t rat[SEQ_LANES][SEQ_STEPS];    /* the hit's ratchet, as SeqStep.ratchet */
    uint8_t cond[SEQ_LANES][SEQ_STEPS];   /* the hit's trig condition, as SeqStep.cond */
} SeqDrums;

typedef struct {
    SeqStep step[SEQ_STEPS];
    uint8_t len;
    uint8_t used;       /* 0 = empty slot */
    char name[SEQ_SLOT_NAME + 1];   /* "" = default ("SLOT 3"); A-Z, 0-9, space, '-' */
    SeqDrums drum;      /* a drum track's lanes (slots keep both kinds of pattern) */
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
    /* Playback colour, applied as notes are sent; the step grid is unchanged. */
    uint8_t chord;              /* CHORD_* (gen.h): notes stacked on each step's note; 0 = single notes */
    uint8_t swell;              /* 0 = off, else a slow velocity swell lasting this many bars (2..32) */
    uint8_t swell_depth;        /* 0..100: how far the swell dips below the step's velocity */
    uint8_t rest;               /* 0..100: chance each bar that the track rests for 1-4 bars */
    uint8_t rest_left;          /* bars of the current rest still to go (not saved) */
    /* Live shuffle: each time a note plays, the next time round it gets a
     * fresh random offset of up to `shuffle` ticks either side of its own
     * OFFSET. `offgrid` keeps every note at least SEQ_OFFGRID_MIN ticks off
     * the grid. The pattern itself is unchanged. */
    uint8_t shuffle;            /* 0..SEQ_MAX_NUDGE ticks */
    uint8_t offgrid;            /* 0 or 1 */
    int8_t jit[SEQ_STEPS];      /* offset each step will use next time it plays (not saved) */
    uint32_t played[SEQ_STEPS]; /* 1 + the 1/16 number each step last played on, so it plays once per pass (not saved) */
    uint8_t nvoice;             /* chord notes sounding with `sounding` */
    uint8_t voice[SEQ_CHORD_MAX];
    uint8_t last_nvoice;        /* chord notes of the last note sent (for seq_panic) */
    uint8_t last_voice[SEQ_CHORD_MAX];
    /* VOICES: MONO (0) plays one note at a time, the next note cutting the
     * last, as before. POLY (1) plays each step's chord (its `more` notes)
     * and lets every note ring for its own length, over the next steps;
     * the oldest goes first if SEQ_POLY_VOICES are sounding. */
    uint8_t poly;
    uint8_t npv;                          /* POLY notes sounding (not saved) */
    uint8_t pv_note[SEQ_POLY_VOICES];
    uint32_t pv_off[SEQ_POLY_VOICES];     /* tick each one's note-off is due */
    int16_t sounding;   /* note currently held (MONO), or -1 */
    int16_t last_note;  /* last note sent on this track, or -1 (seq_panic re-sends its note-off) */
    uint32_t off_tick;
    /* Drum track. The note steps above are kept while a track is a drum track
     * (and the lanes while it is a note track), so switching TYPE loses nothing. */
    uint8_t type;               /* TRACK_NOTES or TRACK_DRUMS */
    uint8_t style;              /* Style (gen.h) this track was made from: its generator defaults and drum kit */
    uint8_t part;               /* which of that style's parts (0..3; starts as the track's own number) */
    uint8_t drum_lock;          /* bit per lane: generators and evolution leave the lane alone */
    uint8_t drum_busy;          /* 0..100: how busy the drum generator is (50 = the style's own) */
    uint8_t drum_mute;          /* bit per lane: the lane is muted (saved) */
    uint8_t drum_solo;          /* bit per lane: while any lane of the track is soloed, only those play (not saved) */
    SeqDrums drum;
    SeqDrums drum_anchor;       /* anchor of the lanes (with has_anchor) */
    uint8_t drum_down;          /* bit per lane: its note is sounding (not saved) */
    uint8_t drum_note[SEQ_LANES];   /* note each sounding lane sent */
    uint32_t drum_off[SEQ_LANES];   /* tick its note-off is due */
    uint32_t drum_step_lock;    /* bit per step: generators and evolution leave every lane's hit on it alone */
    /* Track lock (Y on the GRID's track box): roll, evolve, transforms and NEW
     * FROM STYLE leave the whole track alone. Hand edits, slots and chains
     * still work. */
    uint8_t locked;
    /* STEP RATE (polymeter): how long one of the track's steps lasts, as an
     * index into seq_rate_ticks (SEQ_RATE_16 = a 1/16, the default). With
     * different lengths and rates the tracks drift against each other and
     * meet again later; REALIGN (Seq.realign) brings them back together. */
    uint8_t rate;
    uint32_t origin;            /* tick the track's step 1 last started from (0, or a REALIGN bar line; not saved) */
    /* Euclidean drum lanes: a lane with euc_hits > 0 gets that many hits spread
     * evenly over the track's length (turned by euc_rot steps) whenever the
     * drum kit rolls, instead of the kit's own pattern. */
    uint8_t euc_hits[SEQ_LANES];
    uint8_t euc_rot[SEQ_LANES];
    /* Ratchets still to play (not saved): the repeats of the last step (note
     * tracks) or of each lane's last hit (drum tracks). A new step on the
     * track cancels the note repeats; a new hit on a lane cancels that lane's. */
    SeqStep rat_st;             /* the step being repeated */
    uint8_t rat_r, rat_k;       /* its ratchet count, and the next repeat to play (1..rat_r-1; rat_k >= rat_r: none left) */
    uint32_t rat_t0;            /* tick the step started on */
    uint8_t lrat_r[SEQ_LANES], lrat_k[SEQ_LANES], lrat_vel[SEQ_LANES];
    uint32_t lrat_t0[SEQ_LANES];
    uint8_t pre;                /* result of the track's last A:B / FIRST condition, for PRE (not saved) */
    /* FOLLOW HARMONY (HARM_*, gen.h): how this track's notes follow the
     * HARMONY progression as they are sent. The pattern is unchanged. */
    uint8_t harm_mode;
} SeqTrack;

/* Live playing and recording (M > LIVE; live.h has the functions). The
 * settings belong to the session, not to a song: they are saved in the
 * pattern file as "lv" lines, and loading a song keeps them. */
#define LIVE_MAX_LENGTH 999   /* longest SONG LENGTH, in bars */
enum { STOP_NOW, STOP_AT_BAR };   /* what Start does while playing */
typedef struct {                  /* one track as it was when the start mark was set */
    SeqStep step[SEQ_STEPS];
    SeqDrums drum;
    uint8_t len, cur_slot, chain_pos;
    uint32_t gen_state;
} LiveTake;
typedef struct {
    /* Settings (saved). */
    uint8_t count_in;     /* bars of count-in before Start: 0, 1, 2 or 4 */
    uint8_t click;        /* count-in click: a short note each beat */
    uint16_t length;      /* SONG LENGTH: stop at this bar line; 0 = off */
    uint8_t stop_mode;    /* STOP_NOW or STOP_AT_BAR */
    uint8_t spp;          /* send Song Position Pointer 0 just before Start */
    uint8_t from_mark;    /* every Start puts the start mark back first */
    uint8_t quant;        /* queued scenes and songs switch at a bar line that is a multiple of this: 1, 2, 4 or 8 */
    int8_t density[SEQ_TRACKS];   /* PERFORM: -100 (thinner) .. +100 (busier); 0 = as written */
    /* Running state (not saved). */
    uint32_t count_left;  /* ticks of count-in still to go; 0 = not counting in */
    int16_t click_note;   /* click note sounding, or -1 */
    uint8_t click_ch, click_left;
    uint8_t stop_pending; /* Start was pressed: stop at the next bar line */
    uint16_t fill;        /* bit per track: a fill plays while it is set (held X on PERFORM; not saved) */
    uint16_t fill_auto;   /* ...and the ARRANGE section fills, set for a section's last bar */
    int8_t scene;         /* scene last recalled, -1 = none */
    int8_t scene_q;       /* scene waiting for the switch bar, -1 = none */
    int8_t song_q;        /* song waiting for the switch bar, -1 = none */
    int8_t song_now;      /* song the engine switched to, -1 = none (the UI reads and clears it) */
    void *song_next;      /* ...its contents (a Seq the UI read from the file) */
    void (*song_swap)(void *seq, void *song);   /* puts song_next in, and frees it (the UI's song_apply) */
    uint8_t has_mark;
    uint32_t mark_rng;
    LiveTake mark[SEQ_TRACKS];
} Live;
enum { SEQ_RATE_32, SEQ_RATE_16T, SEQ_RATE_16, SEQ_RATE_8T, SEQ_RATE_8, SEQ_RATE_4, SEQ_RATE_2, SEQ_RATE_BAR, SEQ_RATES };

/* ARRANGE (M > LIVE): the song as sections, each a scene (slot k of every
 * track) played for some bars, optionally with a fill in its last bar. It
 * belongs to the song: saved with it ("ar" and "as" lines). */
#define ARR_MAX 16
enum { ARR_LOOP, ARR_STOP };                           /* what happens after the last section */
enum { ARR_FILL_OFF, ARR_FILL_DRUMS, ARR_FILL_ALL };   /* fill in a section's last bar */
typedef struct { uint8_t scene, bars, fill; } ArrSection;
typedef struct {
    uint8_t on, end, n;
    ArrSection sec[ARR_MAX];
    uint8_t pos;        /* section playing (not saved) */
    uint16_t bar;       /* bars of it played so far (not saved) */
} Arrange;

typedef void (*SeqSend)(void *ctx, const uint8_t *msg, int len);

typedef struct {
    SeqTrack track[SEQ_TRACKS];
    int bpm;            /* 30..300 */
    int swing;          /* 0..SEQ_MAX_SWING ticks */
    int scale;          /* index into the scale list (see gen.h) */
    int root;           /* 0..11, C = 0 */
    int clock_out;      /* send MIDI clock (0xF8); as clock master, clock also runs while stopped */
    int transport_out;  /* send MIDI Start/Stop (0xFA/0xFC) */
    int style;          /* Style (gen.h): sets generator defaults; STYLE_AMBIENT by default */
    int playing;
    uint32_t tick;      /* next tick to process */
    uint32_t idle_tick; /* tick counter while stopped, for the free-running clock */
    int resync;         /* send Stop+Start at the next bar boundary (after a MIDI reconnect) */
    uint32_t rng;
    uint8_t lane_note[SEQ_LANES];                 /* DRUM MAP: MIDI note of each lane, shared by every drum track */
    char lane_name[SEQ_LANES][SEQ_LANE_NAME + 1];
    int aud_note, aud_ch, aud_left;               /* a note auditioned from the UI: ticks until its note-off (0 = none) */
    /* HARMONY: a chord progression every note track can follow (see gen.h). */
    int harm;           /* HARMONY preset (HP_*); 0 = off */
    int harm_bars;      /* bars per chord: 1, 2, 4 or 8 */
    int harm_drift;     /* the DRIFT preset's current chord, a scale degree 0..6 (not saved) */
    int key_next;       /* KEY MOVE waiting for the next bar line: the new root 0..11, or -1 (not saved) */
    int realign;        /* REALIGN: every this many bars, tracks that are not on their step 1 jump back to it (0 = never) */
    Mod mod;                                      /* M > MODULATION: LFOs sending CCs (mod.h) */
    Arrange arr;                                  /* M > LIVE: ARRANGE (live.h); saved with the song */
    Live live;                                    /* M > LIVE: count-in, song length, start mark (live.h) */
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
/* Turn MIDI clock and Start/Stop on or off (M > DEVICE). Turning Start/Stop
 * on while playing sends Start at the next bar line, so the receiver joins in
 * on beat 1. */
void seq_set_sync(Seq *s, int clock, int transport);
void seq_tick(Seq *s);
/* M > MODULATION SEND ONCE: send destination d's CC now, on its channels. */
int  seq_mod_send_once(Seq *s, int d);                 /* advance one tick (1/96 beat) */
int  seq_playhead(const Seq *s, int t);/* step being played on track t, or -1 */
/* Velocity a note of velocity `vel` is sent with on track t at tick `now`,
 * after the track's swell (unchanged when the swell is off). */
int  seq_swell_vel(const Seq *s, int t, uint32_t now, int vel);

void seq_step_toggle(Seq *s, int t, int i);
/* Every note of a step: its own note first, then its chord notes, no
 * repeats. Returns the count (1..SEQ_POLY). */
int  seq_step_notes(const Seq *s, const SeqStep *st, uint8_t out[SEQ_POLY]);
int  seq_step_more_count(const SeqStep *st);          /* chord notes the step holds besides its own, 0..3 */
int  seq_step_has_note(const Seq *s, const SeqStep *st, int note);
/* Add a note to step i: an empty step gets it as its note (and no chord);
 * a sounding step gets it as a chord note. 0 = added, 1 = already there,
 * -1 = the step is full (SEQ_POLY notes). */
int  seq_step_add_note(Seq *s, int t, int i, int note);
/* Take a note out of step i. Taking out the step's own note promotes its
 * lowest chord note; taking out the last note switches the step off.
 * Returns 0, or -1 if the note isn't on the step. */
int  seq_step_remove_note(Seq *s, int t, int i, int note);
/* Replace step i's chord notes with a CHORD_* shape (gen.h) on its note;
 * CHORD_OFF clears them. */
void seq_step_set_chord(Seq *s, int t, int i, int chord);
/* The CHORD_* shape the step's chord notes make, CHORD_OFF for none, or -1
 * for a hand-made chord that is no shape. */
int  seq_step_chord_shape(const Seq *s, const SeqStep *st);
/* VOICES: 0 MONO, 1 POLY. Releases what the track has sounding. */
void seq_track_poly(Seq *s, int t, int poly);
/* Note-off for everything track t has sounding (notes, chords, drum hits). */
void seq_track_release(Seq *s, int t);
/* Adjust step parameters; a note change also switches the step on. */
void seq_step_adjust(Seq *s, int t, int i, int dnote, int dvel, int dgate, int dprob);
void seq_step_hold(Seq *s, int t, int i, int delta);   /* change a step's hold, 0..SEQ_MAX_HOLD */
void seq_step_nudge(Seq *s, int t, int i, int delta);
void seq_step_ratchet(Seq *s, int t, int i, int delta);   /* 1..SEQ_MAX_RATCHET (1 = off) */
void seq_step_cond(Seq *s, int t, int i, int delta);      /* steps through COND_* */
/* The same for drum hit (lane, step); a step with no hit is left alone. */
void seq_drum_ratchet(Seq *s, int t, int lane, int i, int delta);
void seq_drum_cond(Seq *s, int t, int lane, int i, int delta);  /* change a step's offset, -SEQ_MAX_NUDGE..SEQ_MAX_NUDGE ticks */
void seq_track_mute_toggle(Seq *s, int t);
/* Solo: while any track is soloed, only soloed, unmuted tracks play. Several
 * tracks can be soloed at once. Notes held by tracks that go silent are
 * released straight away. */
void seq_track_solo_toggle(Seq *s, int t);
int  seq_any_solo(const Seq *s);
int  seq_track_audible(const Seq *s, int t);   /* not muted, and soloed if any track is */
/* Drum lanes have their own mute and solo, inside the track (the track's own
 * mute and solo still apply on top). Notes of lanes that go silent stop. */
void seq_lane_mute_toggle(Seq *s, int t, int lane);
void seq_lane_solo_toggle(Seq *s, int t, int lane);
int  seq_lane_audible(const SeqTrack *tr, int lane);   /* not muted, and soloed if any lane is */
void seq_track_lock_toggle(Seq *s, int t);   /* the track lock (SeqTrack.locked) */
int  seq_step_locked(const Seq *s, int t, int i);   /* step i is locked (note step lock, or a drum track's step lock) */
void seq_step_lock_toggle(Seq *s, int t, int i);

/* ALL STEPS: change one setting on every step of a track at once (steps
 * within the track's length; on a drum track every hit of every lane, and only
 * ALL_VEL and ALL_PROB apply). Locked steps are changed too: this is a hand
 * edit, and locks only keep generators away. */
enum { ALL_VEL, ALL_GATE, ALL_HOLD, ALL_NUDGE, ALL_PROB, ALL_KINDS };
void seq_all_adjust(Seq *s, int t, int what, int delta);   /* add delta to each, clamped */
void seq_all_set(Seq *s, int t, int what, int value);      /* give each the same value */
/* Lowest, highest and average value over the steps that play (on steps, or
 * hits); returns how many there are (0: lo/hi/avg are 0). */
int  seq_all_range(const Seq *s, int t, int what, int *lo, int *hi, int *avg);
/* Step locks of the whole track: lock every step, or (if any is locked) unlock every step. Returns 1 if now locked. */
int  seq_all_lock_toggle(Seq *s, int t);
int  seq_locked_steps(const Seq *s, int t);   /* locked steps within the track's length */
void seq_track_len(Seq *s, int t, int delta);
/* STEP RATE: engine ticks per step for a rate (12 for 1/32 ... 384 for a bar),
 * its name ("1/16", "1/8T", "1 BAR"), and a track's own. */
int  seq_rate_ticks(int rate);
const char *seq_rate_name(int rate);
int  seq_track_tps(const SeqTrack *tr);
void seq_track_rate(Seq *s, int t, int delta);      /* the track keeps playing from where its new step count falls */
/* REALIGN: 0 (off), 1, 2, 4, 8, 16, 32 or 64 bars; delta steps through those. */
void seq_adjust_realign(Seq *s, int delta);
/* Bring every track back to its step 1 now (at the next tick). */
void seq_realign_now(Seq *s);
void seq_track_channel(Seq *s, int t, int delta);
void seq_adjust_bpm(Seq *s, int delta);
void seq_adjust_swing(Seq *s, int delta);
void seq_track_shuffle(Seq *s, int t, int delta);   /* live shuffle amount, 0..SEQ_MAX_NUDGE ticks */
void seq_track_offgrid(Seq *s, int t, int on);      /* NEVER ON GRID on (1) or off (0) */
void seq_reshuffle(Seq *s, int t);                  /* draw fresh live offsets for every step */
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
