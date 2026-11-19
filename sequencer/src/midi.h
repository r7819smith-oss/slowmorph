#ifndef SEQ4H_MIDI_H
#define SEQ4H_MIDI_H

#include <stddef.h>
#include <stdint.h>

typedef struct Midi {
    void (*send)(struct Midi *self, const uint8_t *msg, int len);
    /* Optional (may be NULL). Called about once a second from the UI thread with
     * the player lock held: checks the device is still there and reconnects if
     * it was unplugged or plugged in late. Returns 1 when connected. */
    int (*poll)(struct Midi *self);
    void (*close)(struct Midi *self);
    char desc[128];
} Midi;

/* Prints MIDI to stderr when verbose; otherwise discards it. */
Midi *midi_null_open(int verbose);

/* ALSA output. Uses the ALSA sequencer when the kernel has it (USB MIDI and
 * BlueZ BLE-MIDI both appear as sequencer ports), and falls back to raw MIDI
 * devices (hw:C,D,S) when it does not. `match` is a case-insensitive substring
 * of the port name ("client:port", or "hw:C,D,S card:name" for raw MIDI);
 * NULL or "" = first real port. If no port is found yet, it still returns an
 * output that waits and connects from poll(), and fills err with the reason.
 * Returns NULL only if it cannot allocate. */
Midi *midi_alsa_open(const char *match, char *err, size_t errlen);
void midi_alsa_list(void);

#endif
