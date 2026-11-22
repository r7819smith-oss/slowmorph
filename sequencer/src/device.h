/* seq4h device presets (M > DEVICE) and the per-output settings kept in
 * seq4h.midi.
 *
 * A preset sets, in one go, the eight tracks' MIDI channels, the DRUM MAP,
 * MIDI clock and MIDI Start/Stop for one instrument. Clock and Start/Stop
 * belong to the output: each output port remembers its own (and which
 * device it was set to) in seq4h.midi, so going back to a port brings its
 * settings back. */
#ifndef SEQ4H_DEVICE_H
#define SEQ4H_DEVICE_H

#include <stdio.h>

#include "engine.h"
#include "midi.h"

enum { DEV_FELUCCA, DEV_JANGADA, DEV_DIGITONE, DEV_BLACKBOX2, DEV_COUNT, DEV_CUSTOM = DEV_COUNT };

typedef struct {
    const char *name;           /* shown on M > DEVICE and kept in seq4h.midi */
    uint8_t channel[SEQ_TRACKS];/* MIDI channel of each track, 0-based */
    int drum_map;               /* DRUM_* preset (drum.h) */
    int clock, transport;       /* send MIDI clock / Start+Stop */
    const char *port_match[3];  /* parts of a port name that suggest this device (upper case) */
    const char *setup[8];       /* what to set on the device itself, one short line each */
} DevPreset;

const DevPreset *device_preset(int dev);   /* dev 0..DEV_COUNT-1 */
const char *device_name(int dev);          /* "CUSTOM" for DEV_CUSTOM */
int  device_by_name(const char *name);     /* DEV_*, or DEV_CUSTOM if unknown */
/* Puts channels, drum map, clock and Start/Stop on the preset. Tracks keep
 * their notes, type and everything else. */
void device_apply(Seq *s, int dev);
/* The preset the current channels, drum map, clock and Start/Stop match
 * exactly, or DEV_CUSTOM. */
int  device_match(const Seq *s);
/* A preset whose port_match is in the port name (any case), or DEV_CUSTOM. */
int  device_guess(const char *port);

/* seq4h.midi. Line 1 is the chosen port ("" = AUTO, first found), as before.
 * Then one line per output that has its own settings:
 *     out CLOCK TRANSPORT DEVICE PORT
 * e.g. "out 1 0 DIGITONE Elektron Digitone" (PORT is the rest of the line,
 * "" for AUTO). Older files have line 1 only. */
#define MIDICONF_MAX 16
typedef struct {
    char port[MIDI_PORT_NAME];
    int clock, transport, device;
} OutConf;
typedef struct {
    char port[MIDI_PORT_NAME];  /* chosen port */
    OutConf out[MIDICONF_MAX];
    int n;
} MidiConf;

void midiconf_read(MidiConf *c, FILE *f);            /* f may be NULL: empty */
void midiconf_write(const MidiConf *c, FILE *f);
/* The settings of `port`, or NULL if it has none (create: add them, from s). */
OutConf *midiconf_get(MidiConf *c, const char *port);
OutConf *midiconf_set(MidiConf *c, const char *port, const Seq *s, int device);
int  midiconf_load(MidiConf *c, const char *path);   /* 0 = read (or no file) */
int  midiconf_save(const MidiConf *c, const char *path);

#endif
