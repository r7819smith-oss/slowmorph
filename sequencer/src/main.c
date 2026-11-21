#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gen.h"
#include "player.h"
#include "song.h"
#include "ui.h"

static void sleep_ms(long ms) {
    struct timespec t = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&t, NULL);
}

static void note(Midi *m, int ch, int key, int on) {
    uint8_t msg[3] = {(uint8_t)((on ? 0x90 : 0x80) | ch), (uint8_t)key, (uint8_t)(on ? 100 : 0)};
    m->send(m, msg, 3);
}

/* Sends a fixed test so the FM1's channel mapping can be checked without the UI. */
static int midi_test(Midi *m) {
    static const int keys[4] = {60, 64, 67, 72};
    if (m->poll && !m->poll(m)) { fprintf(stderr, "seq4h: no MIDI device connected\n"); return 1; }
    printf("sending to %s\n", m->desc);
    for (int ch = 0; ch < 4; ch++) {
        printf("channel %d (Felucca track %d): note %d\n", ch + 1, ch + 1, keys[ch]);
        fflush(stdout);
        note(m, ch, keys[ch], 1); sleep_ms(600);
        note(m, ch, keys[ch], 0); sleep_ms(300);
    }
    printf("all four channels together\n");
    for (int ch = 0; ch < 4; ch++) note(m, ch, keys[ch], 1);
    sleep_ms(1200);
    for (int ch = 0; ch < 4; ch++) note(m, ch, keys[ch], 0);
    for (int ch = 0; ch < 16; ch++) {   /* all notes off, in case anything hangs */
        uint8_t off[3] = {(uint8_t)(0xB0 | ch), 123, 0};
        m->send(m, off, 3);
    }
    sleep_ms(100);
    return 0;
}

static void usage(void) {
    puts("usage: seq4h [options]\n"
         "  --midi NAME      MIDI output port: case-insensitive part of \"client:port\"\n"
         "                   (default: first available; 'none' = no output; 'list' = show ports;\n"
         "                   'capture:FILE' = write timestamped MIDI to FILE, see tools/midicheck.py)\n"
         "  --capture FILE   also write timestamped MIDI to FILE while sending to --midi\n"
         "  --pattern FILE   pattern file to load and autosave (default seq4h.pat)\n"
         "  --songs DIR      folder for the M > SONG SAVE files seq4h-song1..8.pat (default:\n"
         "                   the pattern file's folder; with --screenshot, songs are off unless given)\n"
         "  --channel N      put all four tracks on MIDI channel N (1-16), e.g. for Felucca's\n"
         "                   MIDI IN: SEL mode, where every channel plays the selected track\n"
         "  --input FILE     button mapping file (default seq4h.input if it exists)\n"
         "  --show-input     show each button's raw code on screen and on stderr\n"
         "  --dump-input     print the button mapping in use, in file format, and exit\n"
         "  --fullscreen     fullscreen window (use on the handheld)\n"
         "  --bpm N          set the tempo (30-300), overriding the pattern file\n"
         "  --headless SECS  no window: play for SECS seconds, then stop and exit (implies\n"
         "                   --autoplay; does not autosave the pattern)\n"
         "  --no-clock       do not send MIDI clock/start/stop\n"
         "  --demo           start from the built-in demo pattern, ignoring the file\n"
         "  --autoplay       start playing immediately\n"
         "  --verbose        print MIDI messages to stderr when no hardware is used\n"
         "  --midi-test      play one note on each of channels 1-4 (C4 E4 G4 C5), then a\n"
         "                   chord on all four, and exit; for checking the FM1's tracks\n"
         "  --screenshot F   render the UI to a BMP file and exit (for testing)");
}

/* The port picked on M > MIDI is kept in seq4h.midi next to the pattern file,
 * and used whenever --midi is not given or is empty. */
static void midi_file_path(const char *pattern, char *out, size_t n) {
    const char *slash = strrchr(pattern, '/');
    if (slash) snprintf(out, n, "%.*s/seq4h.midi", (int)(slash - pattern), pattern);
    else snprintf(out, n, "seq4h.midi");
}

static void midi_file_read(const char *path, char *out, size_t n) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (!fgets(out, (int)n, f)) out[0] = 0;
    fclose(f);
    out[strcspn(out, "\r\n")] = 0;
}

int main(int argc, char **argv) {
    const char *midi_name = "", *pattern = "seq4h.pat", *shot = NULL, *keys = NULL, *capture = NULL;
    const char *input_file = NULL, *songs = NULL;
    int show_input = 0, dump_input = 0;
    int fullscreen = 0, no_clock = 0, demo = 0, verbose = 0, autoplay = 0, channel = 0, test = 0, bpm = 0;
    double headless = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--midi") && i + 1 < argc) midi_name = argv[++i];
        else if (!strcmp(argv[i], "--pattern") && i + 1 < argc) pattern = argv[++i];
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--channel") && i + 1 < argc) channel = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = argv[++i];
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc) capture = argv[++i];
        else if (!strcmp(argv[i], "--bpm") && i + 1 < argc) bpm = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--headless") && i + 1 < argc) headless = atof(argv[++i]);
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) input_file = argv[++i];
        else if (!strcmp(argv[i], "--songs") && i + 1 < argc) songs = argv[++i];
        else if (!strcmp(argv[i], "--show-input")) show_input = 1;
        else if (!strcmp(argv[i], "--dump-input")) dump_input = 1;
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        else if (!strcmp(argv[i], "--no-clock")) no_clock = 1;
        else if (!strcmp(argv[i], "--demo")) demo = 1;
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else if (!strcmp(argv[i], "--autoplay")) autoplay = 1;
        else if (!strcmp(argv[i], "--midi-test")) test = 1;
        else { usage(); return !strcmp(argv[i], "--help") ? 0 : 2; }
    }
    if (!strcmp(midi_name, "list")) { midi_alsa_list(); return 0; }

    InputMap input;
    input_defaults(&input);
    FILE *inf = fopen(input_file ? input_file : "seq4h.input", "r");
    if (inf) {
        input_load(&input, inf, input_file ? input_file : "seq4h.input", stderr);
        fclose(inf);
    } else if (input_file) {
        fprintf(stderr, "seq4h: cannot open %s\n", input_file);
        return 2;
    }
    if (dump_input) { input_save(&input, stdout); return 0; }

    char midi_file[512], midi_saved[MIDI_PORT_NAME];
    midi_file_path(pattern, midi_file, sizeof midi_file);
    if (!*midi_name && !shot && !demo) {
        midi_file_read(midi_file, midi_saved, sizeof midi_saved);
        if (*midi_saved) {
            fprintf(stderr, "seq4h: MIDI output from %s: %s\n", midi_file, midi_saved);
            midi_name = midi_saved;
        }
    }

    Midi *midi;
    if (!strcmp(midi_name, "none")) {
        midi = midi_null_open(verbose);
    } else if (!strncmp(midi_name, "capture:", 8)) {
        char err[160];
        midi = midi_capture_open(midi_name + 8, verbose, NULL, err, sizeof err);
        if (!midi) { fprintf(stderr, "seq4h: %s\n", err); return 1; }
    } else {
        char err[128] = "";
        midi = midi_alsa_open(midi_name, err, sizeof err);
        if (!midi) {
            fprintf(stderr, "seq4h: %s; running without MIDI output\n", err);
            midi = midi_null_open(verbose);
        } else if (*err) {
            fprintf(stderr, "seq4h: %s; will connect when it appears\n", err);
        } else {
            fprintf(stderr, "seq4h: MIDI output %s\n", midi->desc);
        }
    }
    if (!midi) return 1;
    if (test) {
        int rc = midi_test(midi);
        midi->close(midi);
        return rc;
    }
    if (capture) {
        char err[160];
        Midi *cap = midi_capture_open(capture, 0, NULL, err, sizeof err);
        if (!cap) { fprintf(stderr, "seq4h: %s\n", err); return 1; }
        midi = midi_tee_open(midi, cap);
        if (!midi) return 1;
    }

    Player p;
    player_init(&p, midi);
    FILE *f = demo ? NULL : fopen(pattern, "r");
    if (f) {
        if (seq_load(&p.seq, f) != 0) fprintf(stderr, "seq4h: ignoring unreadable %s\n", pattern);
        else fprintf(stderr, "seq4h: loaded %s (%d stored slots)\n", pattern, seq_slots_used(&p.seq));
        fclose(f);
    } else if (demo) {
        seq_load_demo(&p.seq);
    } else {
        style_apply(&p.seq, STYLE_AMBIENT, 0);   /* first run: an evolving ambient starting set */
    }
    if (no_clock) p.seq.clock_out = 0;
    if (bpm && (bpm < 30 || bpm > 300)) { fprintf(stderr, "seq4h: --bpm must be 30-300\n"); return 2; }
    if (bpm) p.seq.bpm = bpm;
    if (headless > 0) autoplay = 1;
    if (channel < 0 || channel > 16) { fprintf(stderr, "seq4h: --channel must be 1-16\n"); return 2; }
    if (channel)
        for (int t = 0; t < SEQ_TRACKS; t++) p.seq.track[t].channel = (uint8_t)(channel - 1);

    if (autoplay) seq_start(&p.seq);
    if (player_begin(&p) != 0) { fprintf(stderr, "seq4h: cannot start timing thread\n"); return 1; }

    if (headless > 0) {   /* testing without a display: play, then stop cleanly */
        struct timespec t = {(time_t)headless, (long)((headless - (time_t)headless) * 1e9)};
        while (nanosleep(&t, &t) != 0) {}
        player_end(&p);
        return 0;
    }

    char song_dir[512];
    if (songs) snprintf(song_dir, sizeof song_dir, "%s", songs);
    else song_dir_of(pattern, song_dir, sizeof song_dir);
    UiOptions opt = {.fullscreen = fullscreen, .screenshot = shot, .keys = keys,
                     .input = &input, .show_input = show_input, .verbose = verbose || show_input,
                     .autosave = shot ? NULL : pattern, .midi_file = shot ? NULL : midi_file,
                     .song_dir = songs || !shot ? song_dir : NULL};
    int rc = ui_run(&p, &opt);

    if (!shot) {
        player_lock(&p);
        if (seq_save_file(&p.seq, pattern) == 0)
            fprintf(stderr, "seq4h: saved %s (%d stored slots)\n", pattern, seq_slots_used(&p.seq));
        else fprintf(stderr, "seq4h: cannot save %s\n", pattern);
        player_unlock(&p);
    }
    player_end(&p);
    return rc;
}
