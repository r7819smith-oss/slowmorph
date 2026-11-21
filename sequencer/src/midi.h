#ifndef SEQ4H_MIDI_H
#define SEQ4H_MIDI_H

#include <stddef.h>
#include <stdint.h>

#define MIDI_PORT_NAME 64   /* longest port name kept, with its terminator */

typedef struct Midi {
    void (*send)(struct Midi *self, const uint8_t *msg, int len);
    /* Optional (may be NULL). Called about once a second from the UI thread with
     * the player lock held: checks the device is still there and reconnects if
     * it was unplugged or plugged in late. Returns 1 when connected. */
    int (*poll)(struct Midi *self);
    /* Optional: fills names with the output ports that could be used now and
     * returns how many (at most max). Opens its own handles and touches no
     * shared state, so it needs no lock. */
    int (*ports)(struct Midi *self, char (*names)[MIDI_PORT_NAME], int max);
    /* Optional, with the player lock held: silence the current port, then use
     * the first port whose name contains match ("" = first port of any kind).
     * Connects at once if the port is there, else poll() keeps trying. */
    void (*choose)(struct Midi *self, const char *match);
    void (*close)(struct Midi *self);
    char desc[128];
    char want[MIDI_PORT_NAME];   /* the port asked for; "" = the first one found */
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

/* Writes each message with a timestamp in microseconds to `path` ("-" =
 * stdout), for checking a run without hardware (tools/midicheck.py). With
 * fake_us set, that value is used as the time instead of the real clock. */
Midi *midi_capture_open(const char *path, int verbose, const long long *fake_us, char *err, size_t errlen);
/* Sends every message to both a and b (in that order); closing it closes both. */
Midi *midi_tee_open(Midi *a, Midi *b);

#endif
