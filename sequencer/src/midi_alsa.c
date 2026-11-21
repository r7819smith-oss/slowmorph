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
    char out_id[32];        /* "hw:C,D,S" of the raw output */
    /* input (only while listening) */
    snd_rawmidi_t *rin;     /* raw MIDI input of the output's card */
    snd_seq_t *sin;         /* ...or a sequencer input connected from the device's port */
    int sin_port;
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
static int raw_each_dir(snd_rawmidi_stream_t stream, int (*fn)(void *, const char *, const char *), void *ctx) {
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
            snd_rawmidi_info_set_stream(ri, stream);
            snd_rawmidi_info_set_subdevice(ri, 0);
            if (snd_ctl_rawmidi_info(ctl, ri) < 0) continue;   /* not this direction */
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

static int raw_each(int (*fn)(void *, const char *, const char *), void *ctx) {
    return raw_each_dir(SND_RAWMIDI_STREAM_OUTPUT, fn, ctx);
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
    snprintf(a->out_id, sizeof a->out_id, "%s", id);
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

/* ---------- input (M > MODULATION > MIDI IN and LEARN) ---------- */

static void in_close(AlsaMidi *a, const char *why) {
    if (a->rin) snd_rawmidi_close(a->rin);
    if (a->sin) snd_seq_close(a->sin);
    a->rin = NULL;
    a->sin = NULL;
    snprintf(a->base.in_desc, sizeof a->base.in_desc, "%s", why);
}

/* The sound card the output is on, or -1. */
static int out_card(AlsaMidi *a) {
    int c = -1;
    if (a->raw && sscanf(a->out_id, "hw:%d", &c) == 1) return c;
    if (a->seq && a->dc >= 0) {
        snd_seq_client_info_t *ci;
        snd_seq_client_info_alloca(&ci);
        if (snd_seq_get_any_client_info(a->seq, a->dc, ci) >= 0) return snd_seq_client_info_get_card(ci);
    }
    return -1;
}

typedef struct { AlsaMidi *a; int card; const char *only; int seen; } InTry;

static int in_try(void *ctx, const char *id, const char *name) {
    InTry *t = ctx;
    int c = -1;
    if (sscanf(id, "hw:%d", &c) != 1 || c != t->card) return 0;
    t->seen++;
    if (t->only && strcmp(id, t->only)) return 0;
    if (snd_rawmidi_open(&t->a->rin, NULL, id, SND_RAWMIDI_NONBLOCK) < 0) { t->a->rin = NULL; return 0; }
    snprintf(t->a->base.in_desc, sizeof t->a->base.in_desc, "%s (raw)", name);
    return 1;
}

/* A sequencer input connected from a readable port of the output's client. */
static int seq_in_open(AlsaMidi *a) {
    if (snd_seq_open(&a->sin, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) { a->sin = NULL; return 0; }
    snd_seq_set_client_name(a->sin, "seq4h-in");
    a->sin_port = snd_seq_create_simple_port(a->sin, "in", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                             SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    snd_seq_port_info_t *pi;
    snd_seq_port_info_alloca(&pi);
    snd_seq_port_info_set_client(pi, a->dc);
    snd_seq_port_info_set_port(pi, -1);
    int best = -1;
    char name[128] = "";
    while (a->sin_port >= 0 && snd_seq_query_next_port(a->sin, pi) >= 0) {
        unsigned need = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
        if ((snd_seq_port_info_get_capability(pi) & need) != need) continue;
        int p = snd_seq_port_info_get_port(pi);
        if (best < 0 || p == a->dp) { best = p; snprintf(name, sizeof name, "%s", snd_seq_port_info_get_name(pi)); }
    }
    if (best < 0 || snd_seq_connect_from(a->sin, a->sin_port, a->dc, best) < 0) {
        snd_seq_close(a->sin);
        a->sin = NULL;
        return 0;
    }
    snprintf(a->base.in_desc, sizeof a->base.in_desc, "%d:%d %s", a->dc, best, name);
    return 1;
}

static int alsa_listen(Midi *m, int on) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (!on) { in_close(a, "NOT LISTENING"); return 0; }
    if (a->rin || a->sin) return 1;
    if (!connected(a)) { snprintf(a->base.in_desc, sizeof a->base.in_desc, "NO DEVICE CONNECTED"); return 0; }
    if (a->seq && a->dc >= 0 && seq_in_open(a)) return 1;
    int card = out_card(a);
    InTry t = {a, card, a->raw ? a->out_id : NULL, 0};
    if (card >= 0 && (raw_each_dir(SND_RAWMIDI_STREAM_INPUT, in_try, &t) || (t.only = NULL, t.seen = 0, raw_each_dir(SND_RAWMIDI_STREAM_INPUT, in_try, &t))))
        return 1;
    snprintf(a->base.in_desc, sizeof a->base.in_desc, card < 0 ? "CANNOT TELL WHICH CARD THE OUTPUT IS ON"
             : t.seen ? "THE DEVICE'S INPUT IS BUSY" : "THE DEVICE HAS NO MIDI INPUT PORT");
    return 0;
}

static int seq_in_read(AlsaMidi *a, uint8_t *buf, int max) {
    int k = 0;
    snd_seq_event_t *ev;
    while (k + 3 <= max && snd_seq_event_input(a->sin, &ev) >= 0 && ev) {
        uint8_t ch = ev->data.note.channel & 15;
        switch (ev->type) {
        case SND_SEQ_EVENT_NOTEON: buf[k++] = 0x90 | ch; buf[k++] = ev->data.note.note & 127; buf[k++] = ev->data.note.velocity & 127; break;
        case SND_SEQ_EVENT_NOTEOFF: buf[k++] = 0x80 | ch; buf[k++] = ev->data.note.note & 127; buf[k++] = ev->data.note.velocity & 127; break;
        case SND_SEQ_EVENT_KEYPRESS: buf[k++] = 0xA0 | ch; buf[k++] = ev->data.note.note & 127; buf[k++] = ev->data.note.velocity & 127; break;
        case SND_SEQ_EVENT_CONTROLLER:
            ch = ev->data.control.channel & 15;
            buf[k++] = 0xB0 | ch; buf[k++] = ev->data.control.param & 127; buf[k++] = ev->data.control.value & 127; break;
        case SND_SEQ_EVENT_PGMCHANGE: buf[k++] = 0xC0 | (ev->data.control.channel & 15); buf[k++] = ev->data.control.value & 127; break;
        case SND_SEQ_EVENT_CHANPRESS: buf[k++] = 0xD0 | (ev->data.control.channel & 15); buf[k++] = ev->data.control.value & 127; break;
        case SND_SEQ_EVENT_PITCHBEND: {
            int v = ev->data.control.value + 8192;
            buf[k++] = 0xE0 | (ev->data.control.channel & 15); buf[k++] = v & 127; buf[k++] = (v >> 7) & 127; break;
        }
        case SND_SEQ_EVENT_CLOCK: buf[k++] = 0xF8; break;
        case SND_SEQ_EVENT_START: buf[k++] = 0xFA; break;
        case SND_SEQ_EVENT_CONTINUE: buf[k++] = 0xFB; break;
        case SND_SEQ_EVENT_STOP: buf[k++] = 0xFC; break;
        case SND_SEQ_EVENT_SENSING: buf[k++] = 0xFE; break;
        default: break;
        }
    }
    return k;
}

static int alsa_recv(Midi *m, uint8_t *buf, int max) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (a->sin) return seq_in_read(a, buf, max);
    if (!a->rin) return 0;
    ssize_t n = snd_rawmidi_read(a->rin, buf, (size_t)max);
    if (n == -EAGAIN || n == 0) return 0;
    if (n < 0) { in_close(a, "INPUT LOST (UNPLUGGED?)"); return 0; }
    return (int)n;
}

static void alsa_send(Midi *m, const uint8_t *msg, int len) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (len <= 0) return;
    if (a->raw) raw_send(a, msg, len);
    else if (a->seq && a->dc >= 0) seq_send(a, msg);
}

static int alsa_poll(Midi *m) {
    AlsaMidi *a = (AlsaMidi *)m;
    if (a->seq && a->dc >= 0 && !seq_dest_alive(a)) a->dc = -1;
    if (!connected(a) && (a->rin || a->sin)) in_close(a, "INPUT LOST (UNPLUGGED?)");
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
        in_close(a, "NOT LISTENING");
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
    in_close(a, "");
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
    a->base.listen = alsa_listen;
    a->base.recv = alsa_recv;
    a->base.close = alsa_close;
    snprintf(a->base.in_desc, sizeof a->base.in_desc, "NOT LISTENING");

    if (snd_seq_open(&a->seq, "default", SND_SEQ_OPEN_OUTPUT, 0) >= 0) {
        snd_seq_set_client_name(a->seq, "seq4h");
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
    printf("inputs:\n");
    raw_each_dir(SND_RAWMIDI_STREAM_INPUT, raw_print, NULL);
}
