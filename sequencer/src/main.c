#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "player.h"
#include "ui.h"

static void usage(void) {
    puts("usage: seq4h [options]\n"
         "  --midi NAME      MIDI output port: case-insensitive part of \"client:port\"\n"
         "                   (default: first available; 'none' = no output; 'list' = show ports)\n"
         "  --pattern FILE   pattern file to load and autosave (default seq4h.pat)\n"
         "  --channel N      put all four tracks on MIDI channel N (1-16), e.g. for Felucca's\n"
         "                   MIDI IN: SEL mode, where every channel plays the selected track\n"
         "  --fullscreen     fullscreen window (use on the handheld)\n"
         "  --no-clock       do not send MIDI clock/start/stop\n"
         "  --demo           start from the built-in demo pattern, ignoring the file\n"
         "  --autoplay       start playing immediately\n"
         "  --verbose        print MIDI messages to stderr when no hardware is used\n"
         "  --screenshot F   render the UI to a BMP file and exit (for testing)");
}

int main(int argc, char **argv) {
    const char *midi_name = "", *pattern = "seq4h.pat", *shot = NULL, *keys = NULL;
    int fullscreen = 0, no_clock = 0, demo = 0, verbose = 0, autoplay = 0, channel = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--midi") && i + 1 < argc) midi_name = argv[++i];
        else if (!strcmp(argv[i], "--pattern") && i + 1 < argc) pattern = argv[++i];
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--channel") && i + 1 < argc) channel = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = argv[++i];
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        else if (!strcmp(argv[i], "--no-clock")) no_clock = 1;
        else if (!strcmp(argv[i], "--demo")) demo = 1;
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else if (!strcmp(argv[i], "--autoplay")) autoplay = 1;
        else { usage(); return !strcmp(argv[i], "--help") ? 0 : 2; }
    }
    if (!strcmp(midi_name, "list")) { midi_alsa_list(); return 0; }

    Midi *midi;
    if (!strcmp(midi_name, "none")) {
        midi = midi_null_open(verbose);
    } else {
        char err[128];
        midi = midi_alsa_open(midi_name, err, sizeof err);
        if (!midi) {
            fprintf(stderr, "seq4h: %s; running without MIDI output\n", err);
            midi = midi_null_open(verbose);
        }
    }
    if (!midi) return 1;

    Player p;
    player_init(&p, midi);
    FILE *f = demo ? NULL : fopen(pattern, "r");
    if (f) {
        if (seq_load(&p.seq, f) != 0) fprintf(stderr, "seq4h: ignoring unreadable %s\n", pattern);
        fclose(f);
    } else {
        seq_load_demo(&p.seq);
    }
    if (no_clock) p.seq.clock_out = 0;
    if (channel < 0 || channel > 16) { fprintf(stderr, "seq4h: --channel must be 1-16\n"); return 2; }
    if (channel)
        for (int t = 0; t < SEQ_TRACKS; t++) p.seq.track[t].channel = (uint8_t)(channel - 1);

    char desc[128];
    snprintf(desc, sizeof desc, "%s", midi->desc);   /* player_end frees midi */
    if (autoplay) seq_start(&p.seq);
    if (player_begin(&p) != 0) { fprintf(stderr, "seq4h: cannot start timing thread\n"); return 1; }

    UiOptions opt = {.fullscreen = fullscreen, .screenshot = shot, .keys = keys, .midi_desc = desc};
    int rc = ui_run(&p, &opt);

    if (!shot) {
        player_lock(&p);
        FILE *o = fopen(pattern, "w");
        if (o) { seq_save(&p.seq, o); fclose(o); }
        else fprintf(stderr, "seq4h: cannot save %s\n", pattern);
        player_unlock(&p);
    }
    player_end(&p);
    return rc;
}
