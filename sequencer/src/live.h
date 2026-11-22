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

void live_save(const Seq *s, FILE *f);
int  live_load_line(Seq *s, const char *line);   /* 1 if it was a live line */

#endif
