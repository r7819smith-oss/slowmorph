/* Device presets (M > DEVICE), clock and Start/Stop on or off, and the
 * per-output settings in seq4h.midi. */
#include "../src/device.h"
#include "../src/drum.h"
#include "../src/engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #c); fails++; } } while (0)

typedef struct { int clocks, starts, stops; } Log;
static void rec(void *ctx, const uint8_t *m, int n) {
    Log *l = ctx;
    (void)n;
    if (m[0] == 0xF8) l->clocks++;
    if (m[0] == 0xFA) l->starts++;
    if (m[0] == 0xFC) l->stops++;
}
static void run(Seq *s, int ticks) { for (int i = 0; i < ticks; i++) seq_tick(s); }

int main(void) {
    Seq s;
    Log l = {0};

    /* defaults are Felucca's */
    seq_init(&s, rec, &l);
    CHECK(device_match(&s) == DEV_FELUCCA);
    CHECK(!strcmp(device_name(DEV_CUSTOM), "CUSTOM"));
    CHECK(device_by_name("BLACKBOX 2") == DEV_BLACKBOX2 && device_by_name("NOPE") == DEV_CUSTOM);

    /* clock off, Start/Stop on: Start and Stop but no clock */
    seq_set_sync(&s, 0, 1);
    run(&s, 96); seq_start(&s); run(&s, 96); seq_stop(&s);
    CHECK(l.clocks == 0 && l.starts == 1 && l.stops == 1);
    CHECK(device_match(&s) == DEV_CUSTOM);

    /* clock on, Start/Stop off: clock all the time, no Start or Stop */
    memset(&l, 0, sizeof l);
    seq_set_sync(&s, 1, 0);
    run(&s, 96); seq_start(&s); run(&s, 96); seq_stop(&s);
    CHECK(l.clocks == 48 && l.starts == 0 && l.stops == 0);

    /* Start/Stop turned on while playing: Start at the next bar line */
    memset(&l, 0, sizeof l);
    seq_start(&s); run(&s, 100);
    seq_set_sync(&s, 1, 1);
    run(&s, 200);
    CHECK(l.starts == 0);
    run(&s, 384);
    CHECK(l.starts == 1);
    seq_stop(&s);

    /* a preset sets channels, drum map, clock and Start/Stop and nothing else */
    s.track[2].channel = 9;
    s.track[3].type = TRACK_DRUMS;
    s.bpm = 77;
    device_apply(&s, DEV_DIGITONE);
    CHECK(device_match(&s) == DEV_DIGITONE);
    CHECK(s.track[2].channel == 2 && s.clock_out == 1 && s.transport_out == 0);
    CHECK(seq_drum_map_is(&s, DRUM_GM) && s.track[3].type == TRACK_DRUMS && s.bpm == 77);
    device_apply(&s, DEV_BLACKBOX2);
    CHECK(device_match(&s) == DEV_BLACKBOX2 && s.lane_note[0] == 36 && s.lane_note[1] == 47 && s.lane_note[7] == 42);
    CHECK(!strcmp(s.lane_name[0], "BD 909") && !strcmp(s.lane_name[4], "CLAP 909"));
    device_apply(&s, DEV_FELUCCA);
    CHECK(device_match(&s) == DEV_FELUCCA && s.lane_note[7] == 56);
    for (int d = 0; d < DEV_COUNT; d++) { device_apply(&s, d); CHECK(device_match(&s) == d || d == DEV_JANGADA); }

    /* guessing from the port name */
    CHECK(device_guess("Elektron Digitone:Elektron Digitone MIDI 1 24:0") == DEV_DIGITONE);
    CHECK(device_guess("hw:1,0,0 FM-1") == DEV_FELUCCA);
    CHECK(device_guess("Jangada:Jangada MIDI 1") == DEV_JANGADA);
    CHECK(device_guess("blackbox 2") == DEV_BLACKBOX2);
    CHECK(device_guess("USB MIDI Interface") == DEV_CUSTOM);

    /* seq4h.midi: old one-line files still read; settings per output round-trip */
    MidiConf c;
    FILE *f = tmpfile();
    fputs("FM-1\n", f); rewind(f);
    midiconf_read(&c, f); fclose(f);
    CHECK(!strcmp(c.port, "FM-1") && c.n == 0 && midiconf_get(&c, "FM-1") == NULL);

    device_apply(&s, DEV_DIGITONE);
    midiconf_set(&c, "Elektron Digitone", &s, DEV_DIGITONE);
    seq_set_sync(&s, 0, 0);
    midiconf_set(&c, "", &s, DEV_CUSTOM);
    seq_set_sync(&s, 1, 1);
    midiconf_set(&c, "FM-1", &s, DEV_FELUCCA);
    midiconf_set(&c, "FM-1", &s, DEV_BLACKBOX2);   /* same port: updated, not added */
    CHECK(c.n == 3);
    f = tmpfile();
    midiconf_write(&c, f); rewind(f);
    MidiConf d;
    midiconf_read(&d, f); fclose(f);
    CHECK(!strcmp(d.port, "FM-1") && d.n == 3);
    const OutConf *o = midiconf_get(&d, "Elektron Digitone");
    CHECK(o && o->clock == 1 && o->transport == 0 && o->device == DEV_DIGITONE);
    o = midiconf_get(&d, "");
    CHECK(o && o->clock == 0 && o->transport == 0 && o->device == DEV_CUSTOM);
    o = midiconf_get(&d, "FM-1");
    CHECK(o && o->device == DEV_BLACKBOX2);

    /* full: the oldest output is forgotten */
    for (int i = 0; i < MIDICONF_MAX + 2; i++) {
        char nm[24];
        snprintf(nm, sizeof nm, "PORT %d", i);
        midiconf_set(&d, nm, &s, DEV_FELUCCA);
    }
    CHECK(d.n == MIDICONF_MAX && midiconf_get(&d, "PORT 0") == NULL && midiconf_get(&d, "PORT 17") != NULL);

    /* the pattern file keeps both settings; old files with "clock 0" turn both off */
    seq_set_sync(&s, 1, 0);
    f = tmpfile();
    seq_save(&s, f); rewind(f);
    Seq t;
    seq_init(&t, rec, &l);
    CHECK(seq_load(&t, f) == 0); fclose(f);
    CHECK(t.clock_out == 1 && t.transport_out == 0);
    f = tmpfile();
    fputs("seq4h 1\nbpm 100\nclock 0\n", f); rewind(f);
    seq_init(&t, rec, &l);
    CHECK(seq_load(&t, f) == 0); fclose(f);
    CHECK(t.clock_out == 0 && t.transport_out == 0);

    if (fails) { fprintf(stderr, "test_device: %d failed\n", fails); return 1; }
    printf("test_device: all passed\n");
    return 0;
}
