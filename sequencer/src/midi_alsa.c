#define _GNU_SOURCE
#include "midi.h"

#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One output that can use either ALSA backend:
 *  - the sequencer (/dev/snd/seq, needs the snd-seq kernel modules), or
 *  - raw MIDI (/dev/snd/midiC*D*, needs only snd-usb-audio + snd-rawmidi).
 * Stock Knulli kernels for the H700 do not always build the sequencer, so raw
 * MIDI is the fallback. midi_alsa_poll() reconnects when the device is
 * unplugged and plugged in again, or plugged in after start. */

typedef struct {
    Midi base;
    char match[MIDI_PORT_NAME];
    /* sequencer */
    snd_seq_t *seq;
    int port;               /* our port, or -1 */
    int dc, dp;             /* connected destination, dc < 0 = none */
    /* raw MIDI */
    snd_rawmidi_t *raw;
    int dropped;            /* raw bytes dropped because the output was full */
    int logged;             /* last connection state written to the log: 0 unknown, 1 up, 2 waiting */
} AlsaMidi;

static void set_desc(AlsaMidi *a, const char *s) { snprintf(a->base.desc, sizeof a->base.desc, "%s", s); }

/* ---------- sequencer ---------- */

/* Walk the writable ports of other clients: calls fn(ctx, client, port, "client:port").
 * Stops and returns 1 when fn returns non-zero. */
static int seq_each(snd_seq_t *seq, int (*fn)(void *, int, int, const char *), void *ctx) {
    int self = snd_seq_client_id(seq);
    snd_seq_client_info_t *ci;
    snd_seq_port_info_t *pi;
    snd_seq_client_info_alloca(&ci);
    snd_seq_port_info_alloca(&pi);
    snd_seq_client_info_set_client(ci, -1);
    while (snd_seq_query_next_client(seq, ci) >= 0) {
        int c = snd_seq_client_info_get_client(ci);
        if (c == self || c == SND_SEQ_CLIENT_SYSTEM || !strcmp(snd_seq_client_info_get_name(ci), "Midi Through")) continue;
        snd_seq_port_info_set_client(pi, c);
        snd_seq_port_info_set_port(pi, -1);
        while (snd_seq_query_next_port(seq, pi) >= 0) {
            unsigned need = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
            unsigned caps = snd_seq_port_info_get_capability(pi);
            if ((caps & need) != need || (caps & SND_SEQ_PORT_CAP_NO_EXPORT)) continue;
            char name[128];
            snprintf(name, sizeof name, "%s:%s", snd_seq_client_info_get_name(ci), snd_seq_port_info_get_name(pi));
            if (fn(ctx, c, snd_seq_port_info_get_port(pi), name)) return 1;
        }
    }
    return 0;
}

typedef struct { const char *match; int c, p; char *desc; size_t dn; } SeqFind;

static int seq_find(void *ctx, int c, int p, const char *name) {
    SeqFind *f = ctx;
    if (f->match && *f->match && !strcasestr(name, f->match)) return 0;
    f->c = c; f->p = p;
    snprintf(f->desc, f->dn, "%s", name);
    return 1;
}

static int seq_print(void *ctx, int c, int p, const char *name) {
    (void)ctx;
    printf("seq  %d:%d  %s\n", c, p, name);
    return 0;
}

/* Collects names into a list (MIDI_PORT_NAME each). */
typedef struct { char (*names)[MIDI_PORT_NAME]; int n, max; } NameList;

static int seq_collect(void *ctx, int c, int p, const char *name) {
    (void)c; (void)p;
    NameList *l = ctx;
    if (l->n < l->max) snprintf(l->names[l->n++], MIDI_PORT_NAME, "%s", name);
    return l->n >= l->max;
}

static int seq_dest_alive(AlsaMidi *a) {
    snd_seq_port_info_t *pi;
    snd_seq_port_info_alloca(&pi);
    return snd_seq_get_any_port_info(a->seq, a->dc, a->dp, pi) >= 0;
}

static int seq_connect(AlsaMidi *a) {
    char desc[128];
    SeqFind f = {a->match, -1, -1, desc, sizeof desc};
    if (!seq_each(a->seq, seq_find, &f)) return 0;
    if (snd_seq_connect_to(a->seq, a->port, f.c, f.p) < 0) return 0;
    a->dc = f.c; a->dp = f.p;
    set_desc(a, desc);
    return 1;
}

static void seq_send(AlsaMidi *a, const uint8_t *msg) {
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
    case 0xC0: snd_seq_ev_set_pgmchange(&ev, ch, msg[1]); break;
    case 0xF8: ev.type = SND_SEQ_EVENT_CLOCK; break;
    case 0xFA: ev.type = SND_SEQ_EVENT_START; break;
    case 0xFB: ev.type = SND_SEQ_EVENT_CONTINUE; break;
    case 0xFC: ev.type = SND_SEQ_EVENT_STOP; break;
    default: return;
    }
    snd_seq_event_output_direct(a->seq, &ev);
}

/* ---------- raw MIDI ---------- */

/* Walk every raw MIDI output subdevice: calls fn(ctx, "hw:C,D,S", "card name: subdevice name").
 * Stops and returns 1 when fn returns non-zero. */
static int raw_each(int (*fn)(void *, const char *, const char *), void *ctx) {
    int card = -1;
    while (snd_card_next(&card) >= 0 && card >= 0) {
        char hw[32];
        snprintf(hw, sizeof hw, "hw:%d", card);
        snd_ctl_t *ctl;
        if (snd_ctl_open(&ctl, hw, 0) < 0) continue;
        snd_ctl_card_info_t *cinfo;
        snd_ctl_card_info_alloca(&cinfo);
        const char *cname = snd_ctl_card_info(ctl, cinfo) >= 0 ? snd_ctl_card_info_get_name(cinfo) : "?";
        int dev = -1;
        while (snd_ctl_rawmidi_next_device(ctl, &dev) >= 0 && dev >= 0) {
            snd_rawmidi_info_t *ri;
            snd_rawmidi_info_alloca(&ri);
            snd_rawmidi_info_set_device(ri, dev);
            snd_rawmidi_info_set_stream(ri, SND_RAWMIDI_STREAM_OUTPUT);
            snd_rawmidi_info_set_subdevice(ri, 0);
            if (snd_ctl_rawmidi_info(ctl, ri) < 0) continue;   /* input only */
            int subs = snd_rawmidi_info_get_subdevices_count(ri);
            for (int s = 0; s < subs; s++) {
                snd_rawmidi_info_set_subdevice(ri, s);
                if (snd_ctl_rawmidi_info(ctl, ri) < 0) continue;
                char id[32], name[128];
                snprintf(id, sizeof id, "hw:%d,%d,%d", card, dev, s);
                const char *sn = snd_rawmidi_info_get_subdevice_name(ri);
                snprintf(name, sizeof name, "%s:%s", cname, sn && *sn ? sn : snd_rawmidi_info_get_name(ri));
                if (fn(ctx, id, name)) { snd_ctl_close(ctl); return 1; }
            }
        }
        snd_ctl_close(ctl);
    }
    return 0;
}

static int raw_print(void *ctx, const char *id, const char *name) {
    (void)ctx;
    printf("raw  %-10s %s\n", id, name);
    return 0;
}

typedef struct { AlsaMidi *a; } RawTry;

static int raw_try(void *ctx, const char *id, const char *name) {
    AlsaMidi *a = ((RawTry *)ctx)->a;
    char both[160];
    snprintf(both, sizeof both, "%s %s", id, name);
    if (a->match[0] && !strcasestr(both, a->match)) return 0;
    /* Non-blocking: a full or stalled device must never hold up the timing thread. */
    if (snd_rawmidi_open(NULL, &a->raw, id, SND_RAWMIDI_NONBLOCK) < 0) { a->raw = NULL; return 0; }
    char desc[128];
    snprintf(desc, sizeof desc, "%s (raw)", name);
    set_desc(a, desc);
    return 1;
}

static int raw_collect(void *ctx, const char *id, const char *name) {
    (void)id;
    NameList *l = ctx;
    if (l->n < l->max) snprintf(l->names[l->n++], MIDI_PORT_NAME, "%s", name);
    return l->n >= l->max;
}

static int raw_connect(AlsaMidi *a) {
    RawTry t = {a};
    return raw_each(raw_try, &t);
}

static void raw_drop(AlsaMidi *a) {
    if (a->raw) snd_rawmidi_close(a->raw);
    a->raw = NULL;
}

static void raw_send(AlsaMidi *a, const uint8_t *msg, int len) {
    ssize_t n = snd_rawmidi_write(a->raw, msg, (size_t)len);
    if (n == -EAGAIN || (n >= 0 && n < len)) a->dropped++;
    else if (n < 0) raw_drop(a);   /* -ENODEV etc.: unplugged; poll() reconnects */
}

/* ---------- common ---------- */

static int connected(AlsaMidi *a) { return a->raw || (a->seq && a->dc >= 0); }

static void alsa_send(Midi *m, const uint8_t *msg, int len) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (len <= 0) return;
    if (a->raw) raw_send(a, msg, len);
    else if (a->seq && a->dc >= 0) seq_send(a, msg);
}

static int alsa_poll(Midi *m) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (a->seq && a->dc >= 0 && !seq_dest_alive(a)) a->dc = -1;
    if (!connected(a)) {
        if (!(a->seq && seq_connect(a)) && !raw_connect(a)) {
            snprintf(a->base.desc, sizeof a->base.desc, "WAITING FOR %s", a->match[0] ? a->match : "DEVICE");
            if (a->logged != 2) { fprintf(stderr, "seq4h: MIDI %s\n", a->base.desc); a->logged = 2; }
            return 0;
        }
    }
    if (a->logged != 1) { fprintf(stderr, "seq4h: MIDI connected to %s\n", a->base.desc); a->logged = 1; }
    return 1;
}

/* The ports poll() could connect to: sequencer ports when there are any, else
 * raw MIDI devices (poll() tries them in the same order). Uses its own
 * sequencer handle, so the timing thread's one is never touched. */
static int alsa_ports(Midi *m, char (*names)[MIDI_PORT_NAME], int max) {
    (void)m;
    NameList l = {names, 0, max};
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) >= 0) {
        seq_each(seq, seq_collect, &l);
        snd_seq_close(seq);
    }
    if (l.n == 0) raw_each(raw_collect, &l);
    return l.n;
}

static void alsa_choose(Midi *m, const char *match) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (connected(a)) {
        /* All notes off and all sound off on every channel of the old port,
         * so nothing hangs there. */
        for (int ch = 0; ch < 16; ch++) {
            uint8_t off[3] = {(uint8_t)(0xB0 | ch), 123, 0}, snd[3] = {(uint8_t)(0xB0 | ch), 120, 0};
            alsa_send(m, off, 3);
            alsa_send(m, snd, 3);
        }
        if (a->seq && a->dc >= 0) snd_seq_disconnect_to(a->seq, a->port, a->dc, a->dp);
        a->dc = -1;
        raw_drop(a);
    }
    snprintf(a->match, sizeof a->match, "%s", match ? match : "");
    snprintf(a->base.want, sizeof a->base.want, "%s", a->match);
    fprintf(stderr, "seq4h: MIDI output chosen: %s\n", a->match[0] ? a->match : "first port found");
    a->logged = 0;
    alsa_poll(m);
}

static void alsa_close(Midi *m) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (a->dropped) fprintf(stderr, "seq4h: %d raw MIDI messages dropped (output full)\n", a->dropped);
    raw_drop(a);
    if (a->seq) snd_seq_close(a->seq);
    free(a);
}

Midi *midi_alsa_open(const char *match, char *err, size_t errlen) {
    AlsaMidi *a = calloc(1, sizeof *a);
    if (!a) { snprintf(err, errlen, "out of memory"); return NULL; }
    snprintf(a->match, sizeof a->match, "%s", match ? match : "");
    snprintf(a->base.want, sizeof a->base.want, "%s", a->match);
    a->port = -1; a->dc = -1;
    a->base.send = alsa_send;
    a->base.poll = alsa_poll;
    a->base.ports = alsa_ports;
    a->base.choose = alsa_choose;
    a->base.close = alsa_close;

    if (snd_seq_open(&a->seq, "default", SND_SEQ_OPEN_OUTPUT, 0) >= 0) {
        snd_seq_set_client_name(a->seq, "Genvolve");
        a->port = snd_seq_create_simple_port(a->seq, "out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                             SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        if (a->port < 0) { snd_seq_close(a->seq); a->seq = NULL; }
    }
    if (!a->seq) fprintf(stderr, "seq4h: no ALSA sequencer (snd-seq not loaded?); using raw MIDI\n");

    if (!alsa_poll(&a->base)) {
        /* Keep the output open: poll() connects when the device appears. */
        if (a->match[0]) snprintf(err, errlen, "no MIDI port matching \"%s\" yet", a->match);
        else snprintf(err, errlen, "no MIDI output port yet");
    }
    return &a->base;
}

void midi_alsa_list(void) {
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) >= 0) {
        seq_each(seq, seq_print, NULL);
        snd_seq_close(seq);
    } else {
        printf("(no ALSA sequencer: snd-seq is not loaded or not built)\n");
    }
    raw_each(raw_print, NULL);
}
