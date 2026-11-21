/* Capture backend: writes every MIDI message to a text file with a timestamp,
 * so a run can be checked without any MIDI hardware (see tools/midicheck.py).
 *
 * File format, one message per line:
 *     <microseconds> <hex byte> [<hex byte> ...]
 * Lines starting with '#' are comments. Live captures use CLOCK_MONOTONIC
 * time since the backend was opened; offline renders (tools/render.c) use the
 * ideal time of each tick, so the two can be checked by the same script. */
#define _GNU_SOURCE
#include "midi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    Midi base;
    FILE *f;
    struct timespec t0;
    int verbose;
    const long long *fake_us;   /* offline render: use this time instead of the clock */
} CaptureMidi;

static long long now_us(const CaptureMidi *c) {
    if (c->fake_us) return *c->fake_us;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_sec - c->t0.tv_sec) * 1000000LL + (t.tv_nsec - c->t0.tv_nsec) / 1000;
}

static void capture_send(Midi *m, const uint8_t *msg, int len) {
    CaptureMidi *c = (CaptureMidi *)m;
    fprintf(c->f, "%lld", now_us(c));
    for (int i = 0; i < len; i++) fprintf(c->f, " %02X", msg[i]);
    fputc('\n', c->f);
    if (c->verbose && msg[0] != 0xF8) {
        fprintf(stderr, "midi:");
        for (int i = 0; i < len; i++) fprintf(stderr, " %02X", msg[i]);
        fputc('\n', stderr);
    }
}

static void capture_close(Midi *m) {
    CaptureMidi *c = (CaptureMidi *)m;
    fprintf(c->f, "# end\n");
    if (c->f != stdout) fclose(c->f); else fflush(c->f);
    free(c);
}

Midi *midi_capture_open(const char *path, int verbose, const long long *fake_us, char *err, size_t errlen) {
    CaptureMidi *c = calloc(1, sizeof *c);
    if (!c) { snprintf(err, errlen, "out of memory"); return NULL; }
    c->f = strcmp(path, "-") ? fopen(path, "w") : stdout;
    if (!c->f) { snprintf(err, errlen, "cannot write %s", path); free(c); return NULL; }
    setvbuf(c->f, NULL, _IOFBF, 1 << 16);   /* the timing thread must not block on small writes */
    clock_gettime(CLOCK_MONOTONIC, &c->t0);
    c->verbose = verbose;
    c->fake_us = fake_us;
    c->base.send = capture_send;
    c->base.close = capture_close;
    snprintf(c->base.desc, sizeof c->base.desc, "CAPTURE %s", path);
    fprintf(c->f, "# seq4h capture v1 (%s)\n", fake_us ? "offline render" : "live");
    return &c->base;
}

/* Tee: send to two backends, e.g. the FM1 and a capture file. */
typedef struct { Midi base; Midi *a, *b; } TeeMidi;

static void tee_send(Midi *m, const uint8_t *msg, int len) {
    TeeMidi *t = (TeeMidi *)m;
    t->a->send(t->a, msg, len);
    t->b->send(t->b, msg, len);
}

/* Hot-plug goes to the real port; its description (e.g. "WAITING FOR ...")
 * is copied so the header keeps showing it. */
static int tee_poll(Midi *m) {
    TeeMidi *t = (TeeMidi *)m;
    int up = t->a->poll ? t->a->poll(t->a) : 1;
    snprintf(t->base.desc, sizeof t->base.desc, "%s", t->a->desc);
    return up;
}

static int tee_ports(Midi *m, char (*names)[MIDI_PORT_NAME], int max) {
    TeeMidi *t = (TeeMidi *)m;
    return t->a->ports(t->a, names, max);
}

static void tee_choose(Midi *m, const char *match) {
    TeeMidi *t = (TeeMidi *)m;
    t->a->choose(t->a, match);
    snprintf(t->base.desc, sizeof t->base.desc, "%s", t->a->desc);
    snprintf(t->base.want, sizeof t->base.want, "%s", t->a->want);
}

static void tee_close(Midi *m) {
    TeeMidi *t = (TeeMidi *)m;
    t->a->close(t->a);
    t->b->close(t->b);
    free(t);
}

Midi *midi_tee_open(Midi *a, Midi *b) {
    TeeMidi *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->a = a; t->b = b;
    t->base.send = tee_send;
    t->base.poll = a->poll ? tee_poll : NULL;
    t->base.ports = a->ports ? tee_ports : NULL;
    t->base.choose = a->choose ? tee_choose : NULL;
    t->base.close = tee_close;
    snprintf(t->base.want, sizeof t->base.want, "%s", a->want);
    snprintf(t->base.desc, sizeof t->base.desc, "%s", a->desc);   /* the header shows the real port */
    return &t->base;
}
