/* seq4h drum tracks: 8 lanes per track, each lane its own MIDI note (the
 * DRUM MAP). The engine plays them (engine.c); this file has the editing,
 * the DRUM MAP, and the drum generator, evolution, morph and transforms. */
#ifndef SEQ4H_DRUM_H
#define SEQ4H_DRUM_H

#include "engine.h"

/* Lane notes and names start as the General MIDI drum map, which Felucca's
 * DRUM engine follows ("an 8-lane kit on the General MIDI key map"). Which 8
 * GM notes Felucca's lanes answer to is not published, so they can be changed
 * on M > DRUM MAP. */
void seq_drum_map_gm(Seq *s);
void seq_lane_note(Seq *s, int lane, int delta);       /* change a lane's note, 0..127 */
/* Name a lane: as slot names (A-Z, 0-9, space, '-'); "" gives the GM name back. */
void seq_lane_set_name(Seq *s, int lane, const char *name);
const char *seq_lane_gm_name(int lane);

/* Switch a track between TRACK_NOTES and TRACK_DRUMS; sounding notes stop. */
void seq_track_type(Seq *s, int t, int type);
int  seq_drum_column(const SeqTrack *tr, int i);       /* 1 if any lane hits on step i */
int  seq_drum_hits(const SeqTrack *tr);                 /* hits within the track's length */
void seq_drum_toggle(Seq *s, int t, int lane, int i);   /* a new hit: velocity 100, chance 100% */
/* Change a hit's velocity (1..127) and chance (0..100); a step with no hit gets one. */
void seq_drum_adjust(Seq *s, int t, int lane, int i, int dvel, int dprob);
void seq_drum_lock(Seq *s, int t, int lane);            /* toggle the lane's lock */

/* Play `note` on channel ch now and stop it about half a beat later (the
 * engine's tick sends the note-off, playing or not). For DRUM MAP and for
 * hearing a note as it is placed. */
void seq_audition(Seq *s, int ch, int note, int vel);

/* Generator for drum tracks: a kit pattern in the current style, with
 * drum_busy setting how many hits. Locked lanes are left alone. */
void gen_drum_roll(Seq *s, int t);
/* One evolution bar on a drum track: RHYTHM moves a hit to a free step next
 * to it, VELOCITY and PROB nudge hits, ROTATE turns a lane by a step, REGEN
 * re-rolls; amount = chance per lane (per hit for VELOCITY and PROB). PITCH
 * and OFFSET do nothing on drums. Then the anchor pull, as on note tracks. */
void gen_drum_evolve(Seq *s, int t);
/* Each unlocked hit that differs from target becomes target's with chance pct. */
void gen_drum_morph(Seq *s, int t, const SeqDrums *target, int pct);
/* Transforms on drum lanes: REVERSE, SHIFT, THIN, FILL, HUMANIZE and CLEAR.
 * Returns 0, or -1 for one that has no meaning on drums (INVERT, ...). */
int  gen_drum_transform(Seq *s, int t, int op, int amount);

#endif
