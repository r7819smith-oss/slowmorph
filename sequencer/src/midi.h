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

#endif
