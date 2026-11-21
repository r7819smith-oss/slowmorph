#include "midi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { Midi base; int verbose; int fed; } NullMidi;

static void null_send(Midi *m, const uint8_t *msg, int len) {
    NullMidi *n = (NullMidi *)m;
    if (!n->verbose || msg[0] == 0xF8) return;
    fprintf(stderr, "midi:");
    for (int i = 0; i < len; i++) fprintf(stderr, " %02X", msg[i]);
    fputc('\n', stderr);
}

/* Testing only: with SEQ4H_TEST_PORTS="NAME,NAME,..." set, the null output
 * pretends to have those ports, so M > MIDI can be checked without hardware. */
static int null_ports(Midi *m, char (*names)[MIDI_PORT_NAME], int max) {
    (void)m;
    const char *e = getenv("SEQ4H_TEST_PORTS");
    int n = 0;
    while (e && *e && n < max) {
        size_t l = strcspn(e, ",");
        snprintf(names[n++], MIDI_PORT_NAME, "%.*s", (int)l, e);
        e += l + (e[l] == ',');
    }
    return n;
}

static void null_choose(Midi *m, const char *match) {
    char names[8][MIDI_PORT_NAME];
    int n = null_ports(m, names, 8);
    snprintf(m->want, sizeof m->want, "%s", match);
    snprintf(m->desc, sizeof m->desc, "WAITING FOR %.63s", *match ? match : "DEVICE");
    for (int i = 0; i < n; i++)
        if (!*match || strstr(names[i], match)) { snprintf(m->desc, sizeof m->desc, "%.63s", names[i]); break; }
}

/* Testing only: with SEQ4H_TEST_IN="B0 4A 64 90 3C 64 ..." set, the null
 * output pretends to have a MIDI input that receives those bytes (hex) once,
 * when listening starts, so MIDI IN and LEARN can be checked without hardware. */
static int null_listen(Midi *m, int on) {
    NullMidi *n = (NullMidi *)m;
    if (!on) { n->fed = 0; snprintf(m->in_desc, sizeof m->in_desc, "NOT LISTENING"); return 0; }
    snprintf(m->in_desc, sizeof m->in_desc, "TEST INPUT (SEQ4H_TEST_IN)");
    return 1;
}

static int null_recv(Midi *m, uint8_t *buf, int max) {
    NullMidi *n = (NullMidi *)m;
    const char *e = getenv("SEQ4H_TEST_IN");
    if (n->fed || !e) return 0;
    n->fed = 1;
    int k = 0;
    unsigned v;
    int off;
    while (k < max && sscanf(e, " %x%n", &v, &off) == 1) { buf[k++] = (uint8_t)v; e += off; }
    return k;
}

static void null_close(Midi *m) { free(m); }

Midi *midi_null_open(int verbose) {
    NullMidi *n = calloc(1, sizeof *n);
    if (!n) return NULL;
    n->base.send = null_send;
    n->base.close = null_close;
    n->verbose = verbose;
    strcpy(n->base.desc, "NONE");
    if (getenv("SEQ4H_TEST_PORTS")) {
        n->base.ports = null_ports;
        n->base.choose = null_choose;
        null_choose(&n->base, "");
    }
    if (getenv("SEQ4H_TEST_IN")) { n->base.listen = null_listen; n->base.recv = null_recv; }
    snprintf(n->base.in_desc, sizeof n->base.in_desc, "%s", getenv("SEQ4H_TEST_IN") ? "NOT LISTENING" : "NO MIDI OUTPUT, SO NO INPUT");
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
