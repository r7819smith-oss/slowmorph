/* seq4h drum tracks: 8 lanes per track, each lane its own MIDI note (the
 * DRUM MAP). The engine plays them (engine.c); this file has the editing,
 * the DRUM MAP, and the drum generator, evolution, morph and transforms. */
#ifndef SEQ4H_DRUM_H
#define SEQ4H_DRUM_H

#include "engine.h"

/* DRUM MAP presets. Lane notes and names start as FELUCCA: the 8 notes
 * Felucca's DRUM engine designs its lanes on (its firmware, eng_drum.c), all
 * in the General MIDI map. GM has a crash (49) instead of Felucca's bell (56).
 * Any lane can be changed on M > DRUM MAP. */
enum { DRUM_FELUCCA, DRUM_GM, DRUM_PRESETS };
void seq_drum_map_preset(Seq *s, int preset);
void seq_drum_map_gm(Seq *s);                          /* the default map (FELUCCA) */
int  seq_drum_map_is(const Seq *s, int preset);        /* 1 if the map is exactly that preset */
const char *seq_drum_preset_name(int preset);
/* What Felucca's DRUM engine plays for a MIDI note ("KICK", "TOM", "CONGA" ...),
 * and in *semis how many semitones from that sound's own pitch (may be NULL). */
const char *seq_felucca_sound(int note, int *semis);
/* The next note up (dir 1) or down (-1) that plays a different Felucca sound
 * at its own pitch; `note` if there is none. */
int  seq_felucca_next_sound(int note, int dir);
void seq_lane_note(Seq *s, int lane, int delta);       /* change a lane's note, 0..127 */
/* Name a lane: as slot names (A-Z, 0-9, space, '-'); "" gives the FELUCCA name back. */
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
/* The same kit pattern for a NOTE track (a style's drum part): each step plays
 * the most important lane that hits there (kick, snare, clap, hats, ...) as
 * that lane's DRUM MAP note. Locked steps are left alone. */
void gen_drum_notes(Seq *s, int t);
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
