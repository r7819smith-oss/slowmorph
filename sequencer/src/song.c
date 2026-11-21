#include "song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void song_path(const char *dir, int k, char *out, size_t n) {
    snprintf(out, n, "%s/seq4h-song%d.pat", dir && *dir ? dir : ".", k + 1);
}

void song_dir_of(const char *pattern, char *out, size_t n) {
    const char *slash = pattern ? strrchr(pattern, '/') : NULL;
    if (!slash) snprintf(out, n, ".");
    else if (slash == pattern) snprintf(out, n, "/");
    else snprintf(out, n, "%.*s", (int)(slash - pattern), pattern);
}

static void default_name(int k, char *out) {
    memcpy(out, "SONG ", 5);
    out[5] = (char)('1' + (k < 0 ? 0 : k > 8 ? 8 : k));
    out[6] = 0;
}

static void copy_name(char *out, const char *in) {   /* out holds SEQ_SONG_NAME + 1 */
    int n = 0;
    for (; in[n] && n < SEQ_SONG_NAME; n++) out[n] = in[n];
    out[n] = 0;
}

void song_clean_name(int k, const char *in, char *out) {
    char def[SEQ_SONG_NAME + 1];
    int n = 0;
    for (; in && in[n] && n < SEQ_SONG_NAME; n++) {
        char c = in[n];
        if (c == '_') c = ' ';
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        out[n] = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ? c : ' ';
    }
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
    default_name(k, def);
    if (!strcmp(out, def)) out[0] = 0;
}

void song_show_name(int k, const char *name, char *out) {
    if (name && name[0]) copy_name(out, name);
    else default_name(k, out);
}

int song_save_file(const Seq *s, const char *name, const char *path) {
    size_t n = strlen(path) + 5;
    char *tmp = malloc(n);
    if (!tmp) return -1;
    snprintf(tmp, n, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    int rc = -1;
    if (f) {
        seq_save(s, f);
        if (name && name[0]) {   /* "name NAME", spaces as '_'; pattern loading skips this line */
            char nm[SEQ_SONG_NAME + 1];
            copy_name(nm, name);
            for (char *c = nm; *c; c++) if (*c == ' ') *c = '_';
            fprintf(f, "name %s\n", nm);
        }
        rc = (fflush(f) == 0 && fsync(fileno(f)) == 0) ? 0 : -1;
        if (fclose(f) != 0) rc = -1;
        if (rc == 0 && rename(tmp, path) != 0) rc = -1;
        if (rc != 0) remove(tmp);
    }
    free(tmp);
    return rc;
}

/* The name line, wherever it is in the file. */
static void read_name(FILE *f, char *name) {
    char line[128], nm[32];
    name[0] = 0;
    rewind(f);
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "name %31s", nm) == 1) { copy_name(name, nm); break; }
}

int song_read_file(const char *path, Seq *out, char *name) {
    FILE *f = fopen(path, "r");
    if (name) name[0] = 0;
    if (!f) return -1;
    seq_init(out, NULL, NULL);
    int rc = seq_load(out, f);
    if (rc == 0 && name) {
        char raw[SEQ_SONG_NAME + 1];
        read_name(f, raw);
        for (char *c = raw; *c; c++) if (*c == '_') *c = ' ';
        copy_name(name, raw);
    }
    fclose(f);
    return rc;
}

void song_info(const char *path, int k, SongInfo *info) {
    memset(info, 0, sizeof *info);
    Seq *s = malloc(sizeof *s);
    char raw[SEQ_SONG_NAME + 1];
    if (!s) return;
    if (song_read_file(path, s, raw) == 0) {
        info->used = 1;
        song_clean_name(k, raw, info->name);
        info->bpm = s->bpm;
        info->style = s->style;
        info->scale = s->scale;
        info->root = s->root;
        info->slots = seq_slots_used(s);
        for (int t = 0; t < SEQ_TRACKS; t++) {
            info->type[t] = s->track[t].type;
            info->len[t] = s->track[t].len;
            info->chain[t] = s->track[t].chain_len;
        }
    }
    free(s);
}

void song_apply(Seq *s, const Seq *song) {
    seq_panic(s);
    SeqSend send = s->send;
    void *ctx = s->ctx;
    int playing = s->playing, clock_out = s->clock_out, transport_out = s->transport_out, resync = s->resync;
    uint32_t tick = s->tick, idle = s->idle_tick, rng = s->rng;
    *s = *song;
    s->send = send;
    s->ctx = ctx;
    s->playing = playing;
    s->clock_out = clock_out;
    s->transport_out = transport_out;
    s->resync = resync;
    s->tick = tick;
    s->idle_tick = idle;
    s->rng = rng;
    s->aud_left = 0;
    for (int t = 0; t < SEQ_TRACKS; t++) {
        SeqTrack *tr = &s->track[t];
        tr->solo = 0;
        tr->rest_left = 0;
        tr->sounding = tr->last_note = -1;
        tr->nvoice = tr->last_nvoice = 0;
        tr->drum_down = 0;
        memset(tr->played, 0, sizeof tr->played);
        memset(tr->jit, 0, sizeof tr->jit);
    }
    for (int t = 0; t < SEQ_TRACKS; t++) seq_reshuffle(s, t);
}
