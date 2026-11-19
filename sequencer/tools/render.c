/* seq4h-render: run the sequencer engine offline, as fast as possible, and
 * write the MIDI it sends in the capture format (see src/midi_capture.c), with
 * each message stamped at the ideal time of its tick. Needs no SDL, ALSA,
 * threads or hardware, so the output is exactly repeatable for a given pattern.
 *
 *   seq4h-render [--demo | --pattern FILE] [--bars N] [--bpm N] [--swing N]
 *                [--channel N] [--no-clock] [--out FILE]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/engine.h"
#include "../src/midi.h"

static Midi *out;
static void send_cb(void *ctx, const uint8_t *msg, int len) { (void)ctx; out->send(out, msg, len); }

int main(int argc, char **argv) {
    const char *pattern = NULL, *path = "-";
    int bars = 4, bpm = 0, swing = -1, channel = 0, no_clock = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pattern") && i + 1 < argc) pattern = argv[++i];
        else if (!strcmp(argv[i], "--demo")) pattern = NULL;
        else if (!strcmp(argv[i], "--bars") && i + 1 < argc) bars = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--bpm") && i + 1 < argc) bpm = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--swing") && i + 1 < argc) swing = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--channel") && i + 1 < argc) channel = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-clock")) no_clock = 1;
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) path = argv[++i];
        else { fprintf(stderr, "usage: see the comment at the top of tools/render.c\n"); return 2; }
    }

    long long t_us = 0;
    char err[160];
    out = midi_capture_open(path, 0, &t_us, err, sizeof err);
    if (!out) { fprintf(stderr, "seq4h-render: %s\n", err); return 1; }

    Seq s;
    seq_init(&s, send_cb, NULL);
    if (pattern) {
        FILE *f = fopen(pattern, "r");
        if (!f || seq_load(&s, f) != 0) { fprintf(stderr, "seq4h-render: cannot load %s\n", pattern); return 1; }
        fclose(f);
    } else {
        seq_load_demo(&s);
    }
    if (bpm) s.bpm = bpm;
    if (swing >= 0) s.swing = swing;
    if (no_clock) s.clock_out = 0;
    if (channel >= 1 && channel <= 16)
        for (int t = 0; t < SEQ_TRACKS; t++) s.track[t].channel = (uint8_t)(channel - 1);

    /* Tempo is read every tick, as the timing thread does, so a pattern's
     * evolution cannot change it but the stamp still follows s.bpm. */
    double us = 0;
    seq_start(&s);
    for (long k = 0; k < (long)bars * SEQ_PPQN * 4; k++) {
        t_us = (long long)(us + 0.5);
        seq_tick(&s);
        us += 60e6 / ((double)s.bpm * SEQ_PPQN);
    }
    t_us = (long long)(us + 0.5);
    seq_stop(&s);
    out->close(out);
    return 0;
}
