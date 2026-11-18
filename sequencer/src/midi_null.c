#include "midi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { Midi base; int verbose; } NullMidi;

static void null_send(Midi *m, const uint8_t *msg, int len) {
    NullMidi *n = (NullMidi *)m;
    if (!n->verbose || msg[0] == 0xF8) return;
    fprintf(stderr, "midi:");
    for (int i = 0; i < len; i++) fprintf(stderr, " %02X", msg[i]);
    fputc('\n', stderr);
}

static void null_close(Midi *m) { free(m); }

Midi *midi_null_open(int verbose) {
    NullMidi *n = calloc(1, sizeof *n);
    if (!n) return NULL;
    n->base.send = null_send;
    n->base.close = null_close;
    n->verbose = verbose;
    strcpy(n->base.desc, "NONE");
    return &n->base;
}

#ifndef HAVE_ALSA
Midi *midi_alsa_open(const char *match, char *err, size_t errlen) {
    (void)match;
    snprintf(err, errlen, "built without ALSA");
    return NULL;
}
void midi_alsa_list(void) { fprintf(stderr, "built without ALSA\n"); }
#endif
