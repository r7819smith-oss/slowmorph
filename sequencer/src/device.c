#include "device.h"

#include <ctype.h>
#include <string.h>

#include "drum.h"

/* Researched 2026-11-21, not all tested on hardware (see NOTES.md, "Device
 * presets"). Each track n plays on channel n on all four: Felucca and Jangada
 * play FM1 track n from channel n (MIDI IN CH1-4), the Digitone's TRACK 1-4
 * CHANNEL and the Blackbox's included projects use 1-4 too. */
static const DevPreset PRESETS[DEV_COUNT] = {
    {"FELUCCA", {0, 1, 2, 3}, DRUM_FELUCCA, 1, 1, {"FELUCCA", "FM-1", "FM1"},
     {"FM1: MENU > MIDI > MIDI IN: CH1-4",
      "HOME > MIXER > CLOCK: SOURCE USB",
      "FM1 TRACK N PLAYS SEQ4H CHANNEL N",
      "A DRUM TRACK: ITS FM1 TRACK ON DRUM"}},
    {"JANGADA", {0, 1, 2, 3}, DRUM_GM, 1, 1, {"JANGADA"},
     {"FM1 ON JANGADA (A FELUCCA FORK)",
      "GLO > GLOBAL CLK: USB",
      "TRACK 4 AS SYNTH PLAYS CHANNEL 4",
      "T4 SILENT ON DRUM? TRY CHANNEL 10",
      "  (TRACK > MIDI CHANNEL)",
      "DRUM NOTES NOT PUBLISHED: GM MAP"}},
    {"DIGITONE", {0, 1, 2, 3}, DRUM_GM, 1, 0, {"DIGITONE"},
     {"SETTINGS > MIDI CONFIG:",
      " PORT CONFIG > INPUT FROM: USB",
      "  (OR MIDI+USB), RECEIVE NOTES ON",
      " CHANNELS > TRACK 1-4: CH 1-4",
      " SYNC > CLOCK RECEIVE: ON",
      "START/STOP OFF: ITS OWN PATTERN",
      "  STAYS STOPPED. ON: IT PLAYS TOO",
      "NO DRUM ENGINE: USE NOTE TRACKS"}},
    {"BLACKBOX 2", {0, 1, 2, 3}, DRUM_BLACKBOX, 1, 1, {"BLACKBOX", "1010"},
     {"USB INTO ITS PWR/DATA PORT",
      "TRACK N: MIDI IN CHAN N",
      "GLOB SETTINGS: CLOCK SOURCE USB",
      "DRUM MAP: 909 KIT PADS (PADS ARE",
      "  36-43 BOTTOM ROW, 44-51 TOP)",
      "PAD REC ON: NOTES 68 AND UP",
      "  START RECORDING. KEEP IT OFF"}},
};

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

const DevPreset *device_preset(int dev) { return &PRESETS[clampi(dev, 0, DEV_COUNT - 1)]; }

const char *device_name(int dev) { return dev >= 0 && dev < DEV_COUNT ? PRESETS[dev].name : "CUSTOM"; }

int device_by_name(const char *name) {
    for (int d = 0; d < DEV_COUNT; d++) if (!strcmp(name, PRESETS[d].name)) return d;
    return DEV_CUSTOM;
}

void device_apply(Seq *s, int dev) {
    if (dev < 0 || dev >= DEV_COUNT) return;
    const DevPreset *p = &PRESETS[dev];
    for (int t = 0; t < SEQ_TRACKS; t++)   /* through the engine, so held notes stop */
        if (s->track[t].channel != p->channel[t]) seq_track_channel(s, t, p->channel[t] - s->track[t].channel);
    seq_drum_map_preset(s, p->drum_map);
    seq_set_sync(s, p->clock, p->transport);
}

int device_match(const Seq *s) {
    for (int d = 0; d < DEV_COUNT; d++) {
        const DevPreset *p = &PRESETS[d];
        int ok = seq_drum_map_is(s, p->drum_map) && !s->clock_out == !p->clock && !s->transport_out == !p->transport;
        for (int t = 0; t < SEQ_TRACKS && ok; t++) ok = s->track[t].channel == p->channel[t];
        if (ok) return d;
    }
    return DEV_CUSTOM;
}

int device_guess(const char *port) {
    char up[MIDI_PORT_NAME];
    size_t i = 0;
    for (; port[i] && i < sizeof up - 1; i++) up[i] = (char)toupper((unsigned char)port[i]);
    up[i] = 0;
    for (int d = 0; d < DEV_COUNT; d++)
        for (int k = 0; k < 3; k++)
            if (PRESETS[d].port_match[k] && strstr(up, PRESETS[d].port_match[k])) return d;
    return DEV_CUSTOM;
}

/* ---- seq4h.midi ---- */

void midiconf_read(MidiConf *c, FILE *f) {
    memset(c, 0, sizeof *c);
    if (!f) return;
    char line[256];
    int first = 1;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (first) { snprintf(c->port, sizeof c->port, "%.*s", (int)sizeof c->port - 1, line); first = 0; continue; }
        int clock, transport, n = 0;
        char dev[32];
        if (sscanf(line, "out %d %d %31s %n", &clock, &transport, dev, &n) < 3 || !n) continue;
        for (int d = 0; dev[d]; d++) if (dev[d] == '_') dev[d] = ' ';
        const char *port = line + n;
        OutConf *o = midiconf_get(c, port);
        if (!o) {
            if (c->n >= MIDICONF_MAX) continue;
            o = &c->out[c->n++];
            snprintf(o->port, sizeof o->port, "%.*s", (int)sizeof o->port - 1, port);
        }
        o->clock = clock != 0;
        o->transport = transport != 0;
        o->device = device_by_name(dev);
    }
}

void midiconf_write(const MidiConf *c, FILE *f) {
    fprintf(f, "%s\n", c->port);
    for (int i = 0; i < c->n; i++) {
        char dev[32];
        snprintf(dev, sizeof dev, "%s", device_name(c->out[i].device));
        for (int d = 0; dev[d]; d++) if (dev[d] == ' ') dev[d] = '_';
        fprintf(f, "out %d %d %s %s\n", c->out[i].clock, c->out[i].transport, dev, c->out[i].port);
    }
}

OutConf *midiconf_get(MidiConf *c, const char *port) {
    for (int i = 0; i < c->n; i++) if (!strcmp(c->out[i].port, port)) return &c->out[i];
    return NULL;
}

OutConf *midiconf_set(MidiConf *c, const char *port, const Seq *s, int device) {
    OutConf *o = midiconf_get(c, port);
    if (!o) {
        if (c->n >= MIDICONF_MAX) memmove(&c->out[0], &c->out[1], sizeof c->out[0] * --c->n);   /* forget the oldest */
        o = &c->out[c->n++];
        snprintf(o->port, sizeof o->port, "%s", port);
    }
    o->clock = s->clock_out;
    o->transport = s->transport_out;
    o->device = device;
    return o;
}

int midiconf_load(MidiConf *c, const char *path) {
    FILE *f = fopen(path, "r");
    midiconf_read(c, f);
    if (f) fclose(f);
    return 0;
}

int midiconf_save(const MidiConf *c, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    midiconf_write(c, f);
    return fclose(f) ? -1 : 0;
}
