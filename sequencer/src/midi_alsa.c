#define _GNU_SOURCE
#include "midi.h"

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { Midi base; snd_seq_t *seq; int port; } AlsaMidi;

static void alsa_send(Midi *m, const uint8_t *msg, int len) {
    AlsaMidi *a = (AlsaMidi *)m;
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, a->port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    uint8_t st = msg[0] < 0xF0 ? (msg[0] & 0xF0) : msg[0];
    uint8_t ch = msg[0] & 0x0F;
    switch (st) {
    case 0x80: snd_seq_ev_set_noteoff(&ev, ch, msg[1], msg[2]); break;
    case 0x90: snd_seq_ev_set_noteon(&ev, ch, msg[1], msg[2]); break;
    case 0xB0: snd_seq_ev_set_controller(&ev, ch, msg[1], msg[2]); break;
    case 0xF8: ev.type = SND_SEQ_EVENT_CLOCK; break;
    case 0xFA: ev.type = SND_SEQ_EVENT_START; break;
    case 0xFB: ev.type = SND_SEQ_EVENT_CONTINUE; break;
    case 0xFC: ev.type = SND_SEQ_EVENT_STOP; break;
    default: (void)len; return;
    }
    snd_seq_event_output_direct(a->seq, &ev);
}

static void alsa_close(Midi *m) {
    AlsaMidi *a = (AlsaMidi *)m;
    snd_seq_close(a->seq);
    free(a);
}

/* Iterate writable ports of other clients. If match is non-NULL and non-empty,
 * pick the first whose "client:port" contains it. With list != 0, print them. */
static int scan(snd_seq_t *seq, const char *match, int list, int *oc, int *op, char *desc, size_t dn) {
    int self = snd_seq_client_id(seq), found = 0;
    snd_seq_client_info_t *ci;
    snd_seq_port_info_t *pi;
    snd_seq_client_info_alloca(&ci);
    snd_seq_port_info_alloca(&pi);
    snd_seq_client_info_set_client(ci, -1);
    while (snd_seq_query_next_client(seq, ci) >= 0) {
        int c = snd_seq_client_info_get_client(ci);
        if (c == self || c == SND_SEQ_CLIENT_SYSTEM || c == 14 /* Midi Through */) continue;
        snd_seq_port_info_set_client(pi, c);
        snd_seq_port_info_set_port(pi, -1);
        while (snd_seq_query_next_port(seq, pi) >= 0) {
            unsigned need = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
            unsigned caps = snd_seq_port_info_get_capability(pi);
            if ((caps & need) != need || (caps & SND_SEQ_PORT_CAP_NO_EXPORT)) continue;
            char name[128];
            snprintf(name, sizeof name, "%s:%s", snd_seq_client_info_get_name(ci), snd_seq_port_info_get_name(pi));
            if (list) {
                printf("%d:%d  %s\n", c, snd_seq_port_info_get_port(pi), name);
                continue;
            }
            if (match && *match && !strcasestr(name, match)) continue;
            *oc = c; *op = snd_seq_port_info_get_port(pi);
            snprintf(desc, dn, "%s", name);
            found = 1;
            goto done;
        }
    }
done:
    return found;
}

Midi *midi_alsa_open(const char *match, char *err, size_t errlen) {
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        snprintf(err, errlen, "cannot open ALSA sequencer");
        return NULL;
    }
    snd_seq_set_client_name(seq, "seq4h");
    int port = snd_seq_create_simple_port(seq, "out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                          SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    int c = 0, p = 0;
    char desc[128] = "";
    if (port < 0 || !scan(seq, match, 0, &c, &p, desc, sizeof desc)) {
        snprintf(err, errlen, match && *match ? "no MIDI port matching \"%s\"" : "no MIDI output port found",
                 match ? match : "");
        snd_seq_close(seq);
        return NULL;
    }
    if (snd_seq_connect_to(seq, port, c, p) < 0) {
        snprintf(err, errlen, "cannot connect to %s", desc);
        snd_seq_close(seq);
        return NULL;
    }
    AlsaMidi *a = calloc(1, sizeof *a);
    if (!a) { snd_seq_close(seq); return NULL; }
    a->seq = seq; a->port = port;
    a->base.send = alsa_send;
    a->base.close = alsa_close;
    snprintf(a->base.desc, sizeof a->base.desc, "%s", desc);
    return &a->base;
}

void midi_alsa_list(void) {
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        fprintf(stderr, "cannot open ALSA sequencer\n");
        return;
    }
    int c, p; char d[8];
    scan(seq, NULL, 1, &c, &p, d, sizeof d);
    snd_seq_close(seq);
}
