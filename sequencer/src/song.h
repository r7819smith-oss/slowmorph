/* Songs (M > SONG SAVE): the whole session, all four tracks with their
 * pattern slots and chains, BPM, swing, scale, root, style, channels, track
 * types and the DRUM MAP, kept in one of SEQ_SONGS named song files.
 *
 * A song file is an ordinary pattern file (the seq4h.pat format) with an
 * extra "name NAME" line, so any song can also be copied over seq4h.pat, and
 * any pattern file copied to seq4h-songN.pat is a song. Older builds skip the
 * name line. The files sit next to seq4h.pat: seq4h-song1.pat ... -song8.pat. */
#ifndef SEQ4H_SONG_H
#define SEQ4H_SONG_H

#include <stddef.h>

#include "engine.h"

#define SEQ_SONGS 8
#define SEQ_SONG_NAME 8   /* characters in a song name */

/* What the SONG SAVE page shows for each song, read from its file. */
typedef struct {
    int used;                         /* the file exists and is a pattern file */
    char name[SEQ_SONG_NAME + 1];     /* "" = default ("SONG 3") */
    int bpm, style, scale, root;
    int slots;                        /* stored pattern slots, all tracks */
    uint8_t type[SEQ_TRACKS];         /* TRACK_NOTES / TRACK_DRUMS */
    uint8_t len[SEQ_TRACKS];
    uint8_t chain[SEQ_TRACKS];        /* chain length, 0 or 1 = off */
} SongInfo;

/* dir/seq4h-song<k+1>.pat ("." when dir is NULL or empty). */
void song_path(const char *dir, int k, char *out, size_t n);
/* The folder a pattern file is in, for song_path: "." for a bare name. */
void song_dir_of(const char *pattern, char *out, size_t n);

/* Clean a name as slot names are: upper case, A-Z 0-9 '-' and spaces only,
 * trailing spaces dropped, at most SEQ_SONG_NAME characters. The default name
 * of song k ("SONG 3") is stored as "". */
void song_clean_name(int k, const char *in, char *out);
/* The name to show: the song's own name, or "SONG 3". */
void song_show_name(int k, const char *name, char *out);

/* Save s as a song, safely (path.tmp, sync, rename). 0 on success. */
int song_save_file(const Seq *s, const char *name, const char *path);
/* Read a song into *out (a fresh Seq, so nothing of the current session leaks
 * in) and its name into name (SEQ_SONG_NAME + 1, may be NULL). 0 on success,
 * -1 when the file is missing or not a pattern file. */
int song_read_file(const char *path, Seq *out, char *name);
/* Fill *info from the song file (info->used = 0 when there is none). */
void song_info(const char *path, int k, SongInfo *info);
/* Make `song` the live session. Held notes are released first (seq_panic).
 * Kept from the live session: the MIDI callback, playing/stopped, the tick
 * position (so a playing song carries on in time and the FM1's clock does not
 * restart) and the MIDI clock setting, which belongs to the device, not the
 * song. Solo is cleared, as it is never saved. */
void song_apply(Seq *s, const Seq *song);

#endif
