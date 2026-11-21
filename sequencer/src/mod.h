/* seq4h modulation (M > MODULATION): up to 4 LFOs driving up to 16 MIDI CC
 * destinations, plus a MIDI input monitor and CC learn.
 *
 * The model, per destination: the CC moves between LOW and HIGH (0..127),
 * never outside them. HIGH - LOW is how far it moves (the depth); the middle
 * of the two is its centre. The LFO's 0..1 output maps LOW..HIGH (INVERT
 * swaps the direction). A destination sends on a track's channel (T1-T4, which
 * follows the track if its channel changes), on all four track channels
 * (T1-T4 ALL), or on a fixed channel 1-16.
 *
 * Output is kept light: a CC is only sent when its value changes, at most once
 * every MOD_GAP_MS per destination, and at most MOD_PER_TICK messages per
 * engine tick in all. Nothing received is ever sent back out.
 *
 * Pure C with no I/O except through the send callback, like the engine. */
#ifndef SEQ4H_MOD_H
#define SEQ4H_MOD_H

#include <stdint.h>
#include <stdio.h>

#define MOD_LFOS 4
#define MOD_DESTS 16
#define MOD_GAP_MS 20     /* shortest time between two CCs of one destination */
#define MOD_PER_TICK 4    /* most CC messages sent on one engine tick */
#define MOD_TRACKS 4      /* = SEQ_TRACKS */
#define MOD_PPQN 96       /* = SEQ_PPQN */

enum { WAVE_SINE, WAVE_TRI, WAVE_RAMP_UP, WAVE_RAMP_DOWN, WAVE_SQUARE, WAVE_SH, WAVE_SMOOTH, WAVE_COUNT };

/* Where a destination sends: a track's channel, all four, or a fixed channel. */
enum { TO_T1 = 0, TO_ALL = MOD_TRACKS, TO_CH1, TO_COUNT = TO_CH1 + 16 };

typedef struct {
    uint8_t wave;     /* WAVE_* */
    uint8_t sync;     /* 1 = rate in bars and beats (follows the BPM), 0 = free, in seconds */
    uint8_t rate;     /* index into the sync or the free rate table */
    uint8_t phase;    /* 0..95: where the cycle starts, percent (applied at a restart) */
    uint8_t hold;     /* 1 = paused: it stops where it is, so its destinations stop changing */
    uint8_t stop_hold;/* 1 = it also pauses while the sequencer is stopped */
    /* not saved */
    uint32_t count;   /* sync: ticks since its last restart */
    double fph;       /* free: position in the cycle, 0..1 */
    uint32_t cycle;   /* cycles completed (new random values each cycle) */
    uint16_t r0, r1;  /* random values: the cycle's start and end (S+H holds r1) */
    double out;       /* current output, 0..1 */
} ModLfo;

typedef struct {
    uint8_t on;       /* 0 = off: sends nothing (the parameter stays where it was) */
    uint8_t lfo;      /* 0..MOD_LFOS-1 */
    uint8_t to;       /* TO_* */
    uint8_t cc;       /* 0..119 */
    uint8_t lo, hi;   /* the CC moves between these, lo <= hi */
    uint8_t inv;      /* 1 = HIGH when the LFO is low */
    /* not saved */
    int16_t last;     /* last value sent, -1 = none yet (send at the next chance) */
    uint32_t last_at; /* Mod.ticks when it was sent */
} ModDest;

typedef struct {
    uint8_t on;       /* ALL MOD: 0 = nothing is sent */
    ModLfo lfo[MOD_LFOS];
    ModDest dest[MOD_DESTS];
    /* not saved */
    uint32_t ticks;
    uint32_t rng;
    uint8_t rr;       /* destination to look at first (round robin) */
    uint32_t sent;    /* CC messages sent since start (for the screen) */
} Mod;

typedef void (*ModSend)(void *ctx, const uint8_t *msg, int len);

void mod_init(Mod *m);
/* One engine tick (1/96 beat): move the LFOs and send what changed. ch[] is
 * each track's MIDI channel (0..15). Called for every tick, playing or not. */
void mod_tick(Mod *m, int bpm, int playing, const uint8_t ch[MOD_TRACKS], ModSend send, void *ctx);
/* The sequencer started: synced LFOs go back to their PHASE, so they line up
 * with bar 1. Free LFOs carry on. */
void mod_restart(Mod *m);
/* The value destination d would send now (lo..hi). */
int  mod_value(const Mod *m, int d);
/* The LFO's output now, 0..1, and at cycle position p (0..1), for drawing. */
double mod_lfo_out(const Mod *m, int l);
double mod_wave_at(int wave, double p, uint16_t r0, uint16_t r1);
/* Send destination d's current value once, now, whether or not it is on
 * (SEND ONCE, for an instrument's own MIDI learn). Returns how many messages. */
int  mod_send_once(Mod *m, int d, const uint8_t ch[MOD_TRACKS], ModSend send, void *ctx);
/* After a destination's settings change: send again at the next chance. */
void mod_touch(Mod *m, int d);
/* MIDI channel (0..15) of destination d's k-th target; returns the number of
 * channels it sends on (1, or up to 4 different ones for TO_ALL). */
int  mod_channels(const Mod *m, int d, const uint8_t ch[MOD_TRACKS], uint8_t out[MOD_TRACKS]);
/* The first ON destination (other than `skip`) sending CC cc on channel chan, or -1. */
int  mod_find(const Mod *m, int chan, int cc, const uint8_t ch[MOD_TRACKS], int skip);

/* Setting changes, each clamped; HIGH is pushed up / LOW down to keep lo <= hi. */
void mod_set_lo(Mod *m, int d, int v);
void mod_set_hi(Mod *m, int d, int v);
void mod_shift(Mod *m, int d, int delta);   /* move LOW and HIGH together, keeping the width */

/* Names and rate tables. */
const char *mod_wave_name(int w);
int  mod_rate_count(int sync);
void mod_rate_name(int sync, int rate, char *out, int n);   /* "1/4", "4 BARS", "30 S", "2 MIN" */
uint32_t mod_sync_ticks(int rate);                           /* cycle length in ticks */
int  mod_free_tenths(int rate);                              /* cycle length in tenths of a second */
void mod_to_name(int to, char *out, int n);                  /* "T1", "T1-T4", "CH 9" */
/* Felucca's fixed CC map (firmware midi_control.c MIDI_CC_MAP) plus CC1, or
 * NULL: "CUTOFF", "PAN" ... */
const char *mod_felucca_cc(int cc);
/* General MIDI meaning of a CC, or NULL. */
const char *mod_gm_cc(int cc);
/* A CC that should not be modulated: bank select, data entry, RPN/NRPN and
 * the channel mode messages (120-127). */
int  mod_cc_reserved(int cc);

/* Pattern file lines: "mo ON", "lf L WAVE SYNC RATE PHASE HOLD STOPHOLD",
 * "md D ON LFO TO CC LO HI INV". Older builds skip them. */
void mod_save(const Mod *m, FILE *f);
int  mod_load_line(Mod *m, const char *line);   /* 1 = it was a mod line */

/* ---- MIDI input: a byte parser, a monitor and CC learn ---- */

typedef struct {
    uint8_t status;   /* running status, 0 = none */
    uint8_t data[2];
    uint8_t n, need;
    uint8_t sysex;    /* inside a SysEx message */
} MidiParse;

/* Feed one received byte; when it completes a message, writes it to out and
 * returns its length (1..3). Real-time bytes (clock, start, stop) come out at
 * once and do not disturb running status. SysEx is skipped (returns 0). */
int midi_parse_byte(MidiParse *p, uint8_t b, uint8_t out[3]);

#define MON_LINES 14
typedef struct {
    MidiParse parse;
    uint8_t msg[MON_LINES][3];
    uint8_t len[MON_LINES];
    uint8_t echo[MON_LINES];     /* the message matches a CC seq4h sends itself */
    int head, nlog;              /* newest at head - 1 */
    uint32_t notes, ccs, clocks, other, bytes;
} MidiMon;

void mon_clear(MidiMon *mon);
/* Log one complete message (clock and active sensing are counted, not listed). */
void mon_add(MidiMon *mon, const uint8_t *msg, int len, int echo);
/* The k-th newest logged message (0 = newest); returns its length, 0 if none. */
int  mon_get(const MidiMon *mon, int k, uint8_t msg[3], int *echo);
/* "CH 1  CC 74 = 100", "CH 2  NOTE ON C4 100", "START" ... */
void mon_text(const uint8_t *msg, int len, char *out, int n);

/* Learn: destination d takes the incoming CC's number, and its channel (as
 * that track when a track uses the channel, else as a fixed channel). Only a
 * Control Change of 0..119 is learned. Returns 1 if it was taken. */
int mod_learn(Mod *m, int d, const uint8_t *msg, int len, const uint8_t ch[MOD_TRACKS]);

#endif
