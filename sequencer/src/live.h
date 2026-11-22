/* Live playing and recording (M > LIVE): count-in, a fixed song length,
 * stopping at the bar line, Song Position before Start, and a start mark so
 * every take starts from the same pattern.
 *
 * The settings belong to the session, not to a song: they are kept in the
 * pattern file (seq4h.pat) as "lv" lines, and loading a song leaves them as
 * they are. Pure C, no I/O except through the Seq's send callback. */
#ifndef SEQ4H_LIVE_H
#define SEQ4H_LIVE_H

#include <stdio.h>

#include "engine.h"

/* The Live settings and state themselves are in engine.h (Seq.live). */

void live_init(Live *l);


/* Called by seq_start once the tick count is back at 0. Puts the start mark
 * back (START FROM MARK), and with a count-in starts counting: the receiver
 * then gets MIDI Start only when the count-in ends. Returns 1 while counting in. */
int  live_start(Seq *s);
/* MIDI Start, with Song Position 0 first if SONG POSITION is on (only when
 * Start/Stop is sent at all). */
void live_send_start(Seq *s);
/* Called by seq_stop: the click stops and nothing waits for the bar line. */
void live_stop(Seq *s);
/* One tick of count-in (seq_tick calls it instead of playing while
 * count_left > 0): MIDI clock, the click, and Start at the end. */
void live_count_tick(Seq *s);
/* At every bar line while playing (bar counts from 1). Returns 1 if playback
 * stopped here (SONG LENGTH reached, or a stop waiting for the bar line). */
int  live_bar(Seq *s, uint32_t bar);
/* Start pressed while playing: stops now, or with STOP_AT_BAR at the next bar
 * line (pressing again while it waits stops now). Returns 1 if it waits. */
int  live_stop_request(Seq *s);

/* Start mark: remember every track's pattern and the random state now, and put
 * them back later, so a take can start the same way again. */
void live_mark(Seq *s);
int  live_restore_mark(Seq *s);   /* -1 if no mark is set */

/* Where playback is: bar (from 1) and beat (1-4); 0 and 0 while stopped or
 * counting in. */
void live_position(const Seq *s, int *bar, int *beat);
/* Count-in left, as bars and beats to go (e.g. 1 and 3); 0 when not counting. */
int  live_counting(const Seq *s, int *bars, int *beats);

/* Scenes: scene k is pattern slot k of every track (PATTERN SAVE), so one
 * press changes every track at once. Tracks whose slot k is empty, and
 * tracks with a chain of 2 or more slots, keep playing what they play. */
#define LIVE_SCENES SEQ_SLOTS
int  live_scene_tracks(const Seq *s, int k);   /* tracks that have slot k stored */
void live_scene_store(Seq *s, int k);          /* store every track's pattern in slot k */
int  live_scene_recall(Seq *s, int k);         /* recall slot k on every track that has it; tracks changed */
/* Queue scene k for the next switch bar (a bar line that is a multiple of
 * QUANT bars from Start). Stopped or counting in, it is recalled now (returns
 * 0); queued returns 1. k = -1 cancels. */
int  live_queue_scene(Seq *s, int k);
/* Queue a song (a Seq the caller allocated) for the next switch bar; `swap`
 * puts it in and frees it (playing on in time), and song_now becomes k.
 * Replaces and frees a song already queued. k = -1 cancels (and frees). */
void live_queue_song(Seq *s, int k, void *song, void (*swap)(void *seq, void *song));
/* Ticks until the next switch bar, as whole bars and beats to go (rounded
 * up); 0 if nothing is queued or it is not playing. */
/* Put a queued song in straight away (used while stopped). */
void live_start_song_now(Seq *s);
int  live_switch_in(const Seq *s, int *bars, int *beats);
void live_set_quant(Seq *s, int bars);         /* 1, 2, 4 or 8 */

/* DENSITY (PERFORM page), as each step plays; the pattern is not changed.
 * Below 0, notes and hits drop out (beats 1 and 3 last); above 0, empty steps
 * play softer ghost notes (the track's last note before them) and drum lanes
 * that already play get ghost hits. */
void live_density(Seq *s, int t, int delta);
/* For seq_tick: whether a note track's step plays, and as what: st itself,
 * NULL (dropped), or tmp filled with a ghost note. */
const SeqStep *live_note_step(Seq *s, int t, int idx, const SeqStep *st, SeqStep *tmp);
/* For seq_tick: a drum step's hits after DENSITY, chance already rolled:
 * vel[lane], 0 = no hit. Returns how many. */
int  live_drum_step(Seq *s, int t, int idx, uint8_t vel[SEQ_LANES]);
/* FILL: while on, track t plays busier: a note track plays every step (the
 * empty ones as softer ghosts of its last note) and lifts the last beat of
 * each bar an octave; a drum track adds a snare roll rising over the second
 * half of each bar and closed hats on every step, its kick as written.
 * Stopping the fill puts the track straight back. */
void live_fill(Seq *s, int t, int on);
int  live_busy(const Seq *s, int t);   /* DENSITY (or later a fill) changes what track t plays */

void live_save(const Seq *s, FILE *f);
int  live_load_line(Seq *s, const char *line);   /* 1 if it was a live line */

#endif
