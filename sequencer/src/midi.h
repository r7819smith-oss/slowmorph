#ifndef SEQ4H_MIDI_H
#define SEQ4H_MIDI_H

#include <stddef.h>
#include <stdint.h>

typedef struct Midi {
    void (*send)(struct Midi *self, const uint8_t *msg, int len);
    void (*close)(struct Midi *self);
    char desc[128];
} Midi;

/* Prints MIDI to stderr when verbose; otherwise discards it. */
Midi *midi_null_open(int verbose);

/* ALSA sequencer output. Works for USB MIDI and for BlueZ BLE-MIDI, because
 * both appear as ALSA sequencer ports. `match` is a case-insensitive substring
 * of "client:port" (NULL or "" = first real port). Returns NULL and fills err
 * on failure. */
Midi *midi_alsa_open(const char *match, char *err, size_t errlen);
void midi_alsa_list(void);

/* Writes each message with a timestamp in microseconds to `path` ("-" =
 * stdout), for checking a run without hardware (tools/midicheck.py). With
 * fake_us set, that value is used as the time instead of the real clock. */
Midi *midi_capture_open(const char *path, int verbose, const long long *fake_us, char *err, size_t errlen);
/* Sends every message to both a and b (in that order); closing it closes both. */
Midi *midi_tee_open(Midi *a, Midi *b);

#endif
