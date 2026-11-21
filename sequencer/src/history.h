#ifndef SEQ4H_HISTORY_H
#define SEQ4H_HISTORY_H

/* Undo history: the pattern of every track as it was before each edit, so
 * undo can go back many steps and redo forward again. A snapshot holds what
 * an edit changes (notes, drum lanes, length, chord colour, MONO/POLY), not
 * settings such as channel, mute, generator or tempo, and not playback state.
 * Evolution and chains change the pattern by themselves while playing; undo
 * goes back to the pattern as it was before the edit, so it drops those
 * changes too. */

#include "engine.h"

#include <stdint.h>

#define HIST_DEPTH 32      /* edits kept; the oldest goes first */
#define HIST_WHAT 24       /* characters in an entry's label */
#define HIST_MERGE_MS 1500 /* the same edit repeated within this long is one entry */

typedef struct {
    SeqStep step[SEQ_STEPS];
    SeqDrums drum;
    uint32_t drum_step_lock;
    uint8_t len, chord, poly;
} HistTrack;

typedef struct {
    HistTrack t[SEQ_TRACKS];
    char what[HIST_WHAT + 1];   /* e.g. "ROLL T2" */
} HistSnap;

typedef struct {
    HistSnap undo[HIST_DEPTH];  /* ring: the newest is at (head + n - 1) % HIST_DEPTH */
    int head, nundo;
    HistSnap redo[HIST_DEPTH];  /* stack: redo[nredo - 1] is the next redo */
    int nredo;
    uint32_t last_key, last_ms; /* the last entry's merge key and time */
} History;

void hist_clear(History *h);
void hist_capture(const Seq *s, HistSnap *out);   /* the pattern now (what = "") */
int  hist_changed(const HistSnap *a, const HistSnap *b);   /* bit per track that differs */
void hist_restore(Seq *s, const HistSnap *snap);
/* An edit happened: `before` is the pattern before it. key != 0 with the same
 * key as the last entry within HIST_MERGE_MS keeps just the older entry, so
 * holding right on a row is one undo. Any edit clears the redo list. */
void hist_push(History *h, const HistSnap *before, const char *what, uint32_t key, uint32_t now_ms);
/* Step back (or forward). Return 1 and the entry's label in what (if not
 * NULL), or 0 when there is nothing to undo / redo. */
int  hist_undo(History *h, Seq *s, char what[HIST_WHAT + 1]);
int  hist_redo(History *h, Seq *s, char what[HIST_WHAT + 1]);
/* Entry k back from now (0 = the next undo) and k forward (0 = the next redo). */
const HistSnap *hist_undo_entry(const History *h, int k);
const HistSnap *hist_redo_entry(const History *h, int k);

#endif
